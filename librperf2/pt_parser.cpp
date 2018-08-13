#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>


#include "pt_parser.hpp"

extern "C" {
  //#include "pt_cpu.h"
#include "intel-pt.h"
}

#include <link.h>
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
#include <unordered_map>
#include <vector>
#include <set>

int decoder_callback(struct pt_packet_unknown* unknown, const struct pt_config* config, const uint8_t* pos, void* context) {

}

int symbols_resolver::size() {
    return m_symbols_by_addr2.size();
}

void symbols_resolver::add_symbol(const std::string& dso, const std::string& symbol_name, uint64_t addr, size_t len) {
    routine_description descr;
    descr.addr = addr;
    descr.len = len;
    descr.name = symbol_name;
    descr.dso = dso;
    m_symbols_by_addr2[addr] = descr;

    //std::cout << "new symbol " << symbol_name << "@" << dso << " at " << std::hex << addr << std::endl;
}

routine_description* symbols_resolver::lookup_symbol(uint64_t addr) {
    auto it = m_symbols_by_addr2.lower_bound(addr);
    if (it != m_symbols_by_addr2.end()) {
	--it;
	if (it != m_symbols_by_addr2.end()) {
	    if (it->first > addr) {
		std::cout << "STRANGE: " << std::hex << it->first << " <=> " << addr << std::endl;
	    } else {
		auto end_addr = it->second.addr + it->second.len;
		if (end_addr >= addr) {
		    return &it->second;
		}
	    }
	}
    }

    return nullptr;
}

symbols_resolver static_resolver;
symbols_resolver* get_symbols_resolver() {
    return &static_resolver;
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

uint64_t get_ns_from_tsc(uint64_t tsc) {
    uint64_t tsc_freq_khz = 29 * 100000;
    uint64_t scale = (1000000 << 10) / (tsc_freq_khz);
    return (tsc * scale ) >> 10;
}

struct profile_record {
    uint64_t total_counter;
    std::vector<std::string> symbol_name;
};

bool operator < (const profile_record& pr1, const profile_record& pr2) {
    return pr1.total_counter < pr2.total_counter;
}

void process_pt(char* pt_begin, size_t len) {
    uint64_t dummy_symbol_start = 0;
    uint64_t dummy_symbol_end = 0;
    {
	void* sym_addr = ::dlsym(nullptr, "wait_next_recorded_bunch");
	Dl_info info = {};
	Elf64_Sym* syment = nullptr;
	auto status = ::dladdr1(sym_addr, &info, (void**)&syment, RTLD_DL_SYMENT);
	if (status && syment) {
	    dummy_symbol_start = (uint64_t)sym_addr;
	    dummy_symbol_end = dummy_symbol_start + syment->st_size;
	}

	std::cout << "start addr = " << std::hex << dummy_symbol_start << std::endl;
	std::cout << "end addr   = " << std::hex << dummy_symbol_end << std::endl;
    }
  
    struct pt_block_decoder* decoder = nullptr;
    struct pt_config config = {};
    
    int errcode;

    config.size = sizeof(config);
    config.begin = reinterpret_cast<uint8_t*>(pt_begin);
    config.end = reinterpret_cast<uint8_t*>(pt_begin) + len;

    uint32_t eax, ebx;
    __asm__("cpuid;":"=a"(eax),"=b"(ebx):"a"(0x15));

    config.cpuid_0x15_eax = eax;
    config.cpuid_0x15_ebx = ebx;
    config.mtc_freq = 3;
    config.nom_freq = 0x1d;
    


    decoder = pt_blk_alloc_decoder(&config);
    if (!decoder) {
	std::cout << "can't allocate decoder" << std::endl;
    }

    auto default_image = pt_blk_get_image(decoder);
    pt_image_set_callback(default_image, read_memory, nullptr);

    uint64_t prev_timestamp = 0;
    uint64_t start_ns = 0;
    //uint64_t prev_now = 0;
    std::unordered_map<std::string, uint64_t> profile;
    std::vector<std::string> symbols_the_same_time;

    std::set<std::string> processed_dso;

    while (true) {
	auto status = pt_blk_sync_forward(decoder);
	if (status < 0) {
	    std::cout << "can't sync" << std::endl;

	    uint64_t new_sync;

	    if (status == -pte_eos) {
		std::cout << "EOS!!!" << std::endl;

		std::set<profile_record> sorted_profile;
		uint64_t total = 0;
		for (auto& item : profile) {
		    struct profile_record temp_record;
		    temp_record.total_counter = item.second;
		    total += item.second;
		    auto it = sorted_profile.find(temp_record);
		    if (it != sorted_profile.end()) {
			const_cast<std::vector<std::string> &>(it->symbol_name).push_back(item.first);
		    } else {
			profile_record record;
			record.total_counter = item.second;
			record.symbol_name.push_back(item.first);
			sorted_profile.insert(record);
		    }
		}
		
		for (auto& item : sorted_profile) {
		    double percent = ((double)100 * item.total_counter) / total;
		    //std::cout << "counter: " <<  std::dec << get_ns_from_tsc(item.total_counter) << ", " << (double)std::endl;
		    for (auto& symbol : item.symbol_name) {
			std::cout << std::dec << get_ns_from_tsc(item.total_counter) << "ns\t[" << percent << "%]";
			std::cout << "\t" << symbol << "\n";
		    }
		}
		std::cout << "Total time: " << std::dec << get_ns_from_tsc(total) << std::endl;

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
		/*
		std::cout << "tsc = " << event.tsc << std::endl;

		if (prev_timestamp > event.tsc) { 
		    std::cout << "tsc < prev tsc -> " << prev_timestamp << " < " << event.tsc << std::endl;
		}
		*/
		//prev_timestamp = event.tsc;
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

		if (ip >= dummy_symbol_start && ip <= dummy_symbol_end) {
		    continue;
		}

	    try_resolve_symbol:
		
		{
		    auto symbol_info = get_symbols_resolver()->lookup_symbol(ip);
		    if (symbol_info != nullptr) {
			symbol_by_ip = symbol_info->name;
			symbol_by_ip += "@";
			symbol_by_ip += symbol_info->dso;
			goto out;
		    }
		    {
			Dl_info info;
			Elf64_Sym* syment;
			auto status = ::dladdr1((void*)block.ip, &info, (void**)&syment, RTLD_DL_SYMENT);
			std::string symbol;
			if (status) {

			    if (info.dli_fname && strlen(info.dli_fname)) {
				auto it = processed_dso.find(info.dli_fname);
				if (it == processed_dso.end()) {
				    processed_dso.insert(info.dli_fname);
				    std::cout << "New DSO: " << info.dli_fname << std::endl;

				    std::string cmd;
				    cmd += "nm -a -S ";
				    cmd += info.dli_fname;
				    FILE* nm_stm = ::popen(cmd.c_str(), "r");
				    
				    if (nm_stm) {
					size_t addr;
					size_t sz;
					char type[10];
					char symbol[4096];
				    
					while (!feof(nm_stm)) {
					    char buf[10000] = {};
					    if (fgets(buf, sizeof(buf), nm_stm) != NULL) {
						auto scanned = sscanf(buf, "%x %x %1s %s", &addr, &sz, type, symbol);
						if (scanned) {
						    //std::cout << "New symbol: " << std::hex << addr << "/" << sz << " " << type << " " << symbol << std::endl;
						    // void add_symbol(const std::string& symbol_name, uint64_t addr, size_t len) {
						    
						    link_map* linkmap = nullptr;
						    status = ::dladdr1((void*)block.ip, &info, (void**)&linkmap, RTLD_DL_LINKMAP);
						    if (status && linkmap) {
							//std::cout << "true addr = " << std::hex << addr + linkmap->l_addr << std::endl;
							get_symbols_resolver()->add_symbol(info.dli_fname, symbol, addr + linkmap->l_addr, sz);
						    }
						}
					    }
					}

					std::cout << "SYM TABLE side: " << get_symbols_resolver()->size();

					goto try_resolve_symbol;
				    }
				}
			    }
/*
			    symbol = (info.dli_sname ? info.dli_sname : "null");

			    symbol += " -> ";
			    symbol += (info.dli_fname ? info.dli_fname : "null");

			    link_map* linkmap = nullptr;
			    status = ::dladdr1((void*)block.ip, &info, (void**)&linkmap, RTLD_DL_LINKMAP);
			    if (status && linkmap) {
				char buf[100] = {};
				sprintf(buf, "%p", linkmap->l_addr);
				symbol += "/";
				symbol += buf;
			    }
*/
			} else {
			    symbol = "totally unknown";
			}
			routine_description key;
			//key.addr = syment->st_value;
			//key.len = syment->st_size;
			key.addr = block.ip;
			if (syment) {
			    key.len = syment->st_size;
			} else {
			    key.len = 1;
			}
			//key.len = 4000;
			key.name = symbol;
			
			//symbols_by_addr2[ip] = key;
			symbol_by_ip = symbol;

			//std::cout << "new block " << std::hex << key.addr << "@" << key.len << " -> " << symbol << std::endl;
		    }
		
		}
		out:

		if (symbol_by_ip != std::string("wait_next_recorded_bunch")) {
		    if (symbol_by_ip != std::string("wait_next_recorded_bunch -> ./librperf2.so")) {
			//if (block.ip & 0x402000 == 0) {

			profile[symbol_by_ip] += 1;

			uint64_t now;
			uint32_t lost_mtc;
			uint32_t lost_cyc;
			auto status = pt_blk_time(decoder, &now, &lost_mtc, &lost_cyc);
			if (status < 0) {
			    std::cout << "can't get tsc" << std::endl;
			}

			//std::cout << "now = " << now << std::endl;


			if (prev_timestamp > now) {
			    std::cout << "Did you use time machine? " << prev_timestamp << " -> " << now << std::endl;
			}
			/*

			prev_now = now;
			*/

#if 1
			if (prev_timestamp == 0) {
			    prev_timestamp = now;
			    start_ns = get_ns_from_tsc(now);
			    //std::cout << "set prev_timestamp = " << now << std::endl;
			}

			//if (prev_timestamp == now) {
			symbols_the_same_time.push_back(symbol_by_ip);
			//} else {
			if (prev_timestamp != now) {
			    // next timestamp
			    
			    //std::cout << "symbols next timestamp: " << now << "/" << prev_timestamp << std::endl;


			    auto time_diff = now - prev_timestamp;
			    auto bb_timing = time_diff / symbols_the_same_time.size();

			    //std::cout << "time diff = " << time_diff << "/" << bb_timing << std::endl;

			    for (auto& symbol : symbols_the_same_time) {
				auto old_value = profile[symbol];
				profile[symbol] += bb_timing;
				//profile[symbol] += 1;
				//std::cout << symbol << " -> " << profile[symbol] << "\n";
				//std::cout << "Update profile for " << symbol << " " << old_value << " -> " << profile[symbol] << std::endl;
			    }

			    symbols_the_same_time.clear();

			    prev_timestamp = now;
			}
#endif

			auto ns = get_ns_from_tsc(now);

			std::cout << "timestamp: " << std::dec << (ns - start_ns) <<  " -> Routine: " << symbol_by_ip << "[" << std::hex << block.ip << "]" << std::endl;
			if (lost_mtc || lost_cyc) {
			    std::cout << "Lost: " << lost_mtc << "/" << lost_cyc << " mtc/cyc" << std::endl;
			}


			//std::cout << "Routine: " << symbol_by_ip << "@" << std::hex << block.ip << std::endl;

			uint64_t new_sync;
			auto errcode = pt_blk_get_offset(decoder, &new_sync);
			//std::cout << "offset: " << new_sync << std::endl;

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
