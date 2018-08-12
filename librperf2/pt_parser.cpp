#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>


#include "pt_parser.hpp"

extern "C" {
  //#include "pt_cpu.h"
#include "intel-pt.h"
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

extern "C" {
  //#include <xed-interface.h>
}

#include <iostream>
#include <map>
#include <mutex>

int decoder_callback(struct pt_packet_unknown* unknown, const struct pt_config* config, const uint8_t* pos, void* context) {

}

struct routine_description {
    uint64_t addr;
    size_t len;
    std::string name;
};

#if 0
bool operator < (const routine_description& r1, const routine_description& r2) {
    return r1.addr < r2.addr;
}

bool operator < (const routine_description& r, uint64_t addr) {
    /*if (r.addr >= addr) {
	if (r.addr + r.len < addr) {
	    return false;
	}
	}*/
    return r.addr < addr;
}

bool operator < (uint64_t addr, const routine_description& r) {
    /*
    if (r.addr >= addr) {
	if (r.addr + r.len < addr) {
	    return false;
	}
	}*/
    return addr < r.addr;
}

std::map<routine_description, std::string, std::less<void> > symbols_by_addr;
#endif 

std::map<uint64_t, routine_description> symbols_by_addr2;

void add_symbol(const std::string& symbol_name, uint64_t addr, size_t len) {
    //std::cout << "Add symbol: " << symbol_name << " at " << std::hex << addr << "@" << len << std::endl;
    //symbols_by_addr[addr] = symbol_name;
    routine_description descr;
    descr.addr = addr;
    descr.len = len;
    descr.name = symbol_name;
    symbols_by_addr2[addr] = descr;
}


int read_memory(uint8_t *buffer, size_t size,const struct pt_asid *asid,uint64_t ip, void *context) {
    //std::cout << "read memory callback for IP " << std::hex << ip << std::endl;

    if (ip == 0) {
	return 0;
    }

    for (int i = 0; i < size; ++i) {
	buffer[i] = *(((uint8_t*)ip) + i);
    }
    return size;
}

int read_memory_callback(uint8_t* buffer, size_t size, const struct pt_asid* asid, uint64_t ip, void* context) {
    std::cout << "read memory callback for IP " << ip << std::endl;
}

void process_pt(char* pt_begin, size_t len) {
  
    struct pt_block_decoder* decoder = nullptr;
    struct pt_config config = {};
    
    int errcode;

    config.size = sizeof(config);
    config.begin = reinterpret_cast<uint8_t*>(pt_begin);
    config.end = reinterpret_cast<uint8_t*>(pt_begin) + len;
    /*config.decode.callback = decoder_callback;
      config.decode.context = nullptr;*/


    decoder = pt_blk_alloc_decoder(&config);
    if (!decoder) {
	std::cout << "can't allocate decoder" << std::endl;
    }

    auto default_image = pt_blk_get_image(decoder);
    pt_image_set_callback(default_image, read_memory, nullptr);

    while (true) {
	auto status = pt_blk_sync_forward(decoder);
	if (status < 0) {
	    std::cout << "can't sync" << std::endl;

	    uint64_t new_sync;

	    if (status == -pte_eos) {
		std::cout << "EOS!!!" << std::endl;
		/*
		std::cout << "symbol table!!!!" << std::endl;
		auto it =  symbols_by_addr2.begin(), it_end =  symbols_by_addr2.end();
		while (it != it_end) {
		    std::cout << std::hex << it->first << "@" << it->second.len << " -> " << it->second.name << "\n";
		    ++it;
		}
		*/
		return;
	    }
	    
	    errcode = pt_blk_get_offset(decoder, &new_sync);

	    std::cout << "new sync: " << new_sync << std::endl;
	}

	for (;;) {
	    struct pt_block block = {};
	    
	    while (status & pts_event_pending) {
		struct pt_event event;
		status = pt_blk_event(decoder, &event, sizeof(event));

		if (status < 0) {
		    std::cout << "can't get pending event" << std::endl;
		    break;
		}
	    }

	    status = pt_blk_next(decoder, &block, sizeof(block));

	    if (status < 0) {
		auto code = pt_error_code(-status);
		std::cout << "can't pt_blk_next " << pt_errstr(code) << std::endl;
		break;
	    } else {
		//std::cout << "decode - ok" << std::endl;
		
		//std::cout << "Block " << std::hex << block.ip << "->" << std::hex << block.end_ip << std::endl;
		
		std::string symbol_by_ip;

		//auto it = symbols_by_addr.find((uint64_t)block.ip);

		uint64_t ip = (uint64_t)block.ip;
		
		auto it = symbols_by_addr2.lower_bound(ip);

		//std::cout << "for ip " << std::hex << (uint64_t)block.ip << "\n";
		{
		    {
			if (it != symbols_by_addr2.end()) {
			    //std::cout << "first: " << it->first << std::endl;
			    --it;
			      if (it != symbols_by_addr2.end()) 
			    {
				//std::cout << "second: " << it->first << std::endl;
				if (it->first > ip) {
				    std::cout << "STRANGE: " << std::hex << it->first << " <=> " << ip << std::endl;
				} else {
				    auto end_addr = it->second.addr + it->second.len;
				    if (end_addr >= ip) {
					symbol_by_ip = it->second.name;
					//std::cout << "Resolved from cache: " << ip << " -> " << it->second.name << "(" << std::hex << it->second.addr << "@" << it->second.len << ")" << std::endl;
					goto out;
				    }
				}
			    }
			}
		    }
		    std::cout << "resolving via dladdr" << std::endl;
		    {
			Dl_info info;
			Elf64_Sym* syment;
			auto status = ::dladdr((void*)block.ip, &info/*, (void**)&syment, RTLD_DL_SYMENT*/);

			std::string symbol;
			if (status) {
			    symbol = (info.dli_sname ? info.dli_sname : "null");

			    symbol += " -> ";
			    symbol += (info.dli_fname ? info.dli_fname : "null");

			} else {
			    symbol = "totally unknown";
			}
			routine_description key;
			//key.addr = syment->st_value;
			//key.len = syment->st_size;
			key.addr = block.ip;
			key.len = 100;
			key.name = symbol;
			
			symbols_by_addr2[ip] = key;
			symbol_by_ip = symbol;

			std::cout << "new block " << std::hex << key.addr << "@" << key.len << " -> " << symbol << std::endl;
		    }
		
		}
		out:
#if 0
		//if (it == symbols_by_addr.end()) {
		    Dl_info info;
		    Elf64_Sym* syment;
		    auto status = ::dladdr((void*)block.ip, &info/*, (void**)&syment, RTLD_DL_SYMENT*/);

		    std::string symbol;
		    if (status) {
			symbol = (info.dli_sname ? info.dli_sname : "null");

			routine_description key;
			//key.addr = syment->st_value;
			//key.len = syment->st_size;
			key.addr == block.ip;
			key.len = 100;
			
			symbols_by_addr[key] = symbol;
			
		    } else {
			symbol = "totally unknown";
		    }		    
		    symbol_by_ip = symbol;

		} else {
		    symbol_by_ip = it->second;
		}


#endif


		if (symbol_by_ip != std::string("wait_next_recorded_bunch")) {
		    if (symbol_by_ip != std::string("wait_next_recorded_bunch -> ./librperf2.so")) {
			//if (block.ip & 0x402000 == 0) {

			uint64_t now;
			uint32_t lost_mtc;
			uint32_t lost_cyc;
			auto status = pt_blk_time(decoder, &now, &lost_mtc, &lost_cyc);
			if (status < 0) {
			    std::cout << "can't get tsc" << std::endl;
			}

			std::cout << "timestamp: " << now <<  " -> Routine: " << symbol_by_ip << "@" << std::hex << block.ip << std::endl;
			if (lost_mtc || lost_cyc) {
			    std::cout << "Lost: " << lost_mtc << "/" << lost_cyc << " mtc/cyc" << std::endl;
			}

			//uint64_t new_sync;
			//errcode = pt_blk_get_offset(decoder, &new_sync);
			//std::cout << "offset = " << std::dec << new_sync << "\n";

			//}
		    }
		}
	    }
	}

	std::cout << "status == " << status << std::endl;
    }

/*
    auto default_image = pt_insn_get_image(decoder);
    pt_image_set_callback(default_image, read_memory_callback, nullptr);

    for (;;) {

    errcode = pt_insn_sync_forward(decoder);
    if (errcode < 0) {
    std::cout << "can't sync decoder" << std::endl;
}

    for (;;) {
    struct pt_insn insn;

    errcode = pt_insn_next(decoder, &insn, sizeof(insn));

    if (insn.iclass != ptic_error) {
    std::cout << "Instruction!" << std::endl;
}

    if (errcode < 0) {
    break;
}
}
}
*/

    pt_blk_free_decoder(decoder);
    
}
