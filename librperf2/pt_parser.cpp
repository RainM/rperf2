#define _GNU_SOURCE
#include <dlfcn.h>
#include <elf.h>

#include "pt_parser.hpp"

extern "C" {
#include "intel-pt.h"
}

#include <link.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <iostream>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <set>

#include "common.h"
#include "rdtsc_utils.hpp"

typedef std::unique_lock<std::mutex> locker_t;

int decoder_callback(struct pt_packet_unknown* unknown, const struct pt_config* config, const uint8_t* pos, void* context) {

}

int symbols_resolver::size() {
    locker_t _l(m_mutex);
    return m_symbols_by_addr2.size();
}

void symbols_resolver::add_symbol(const routine_description& rd) {
    locker_t _l(m_mutex);
    m_symbols_by_addr2[rd.addr] = rd;
}

void symbols_resolver::add_symbol(
    const std::string& dso, 
    const std::string& symbol_name, 
    uint64_t addr, 
    uint64_t addr_in_disassembly,
    size_t len) 
{
    routine_description descr;
    descr.addr = addr;
    descr.len = len;
    descr.name = symbol_name;
    descr.dso = dso;
    descr.dso_addr = addr_in_disassembly;

    //std::cout << "add symbol " << symbol_name << "@" << dso << " -> " << std::hex << addr << "@" << std::dec << len << "\n";

    add_symbol(descr);
}

void symbols_resolver::add_jit_region(uint64_t addr, size_t sz) {
    locker_t _l(m_mutex);
    m_jit_region_size_by_region_addr[addr] = sz;
}

void symbols_resolver::unload_jit_symbols_from_addr(uint64_t addr) {
    locker_t _l(m_mutex);
    auto sz = m_jit_region_size_by_region_addr[addr];
    if (sz) {
	auto it_begin = map_t::reverse_iterator(m_symbols_by_addr2.find(addr));
	auto it_end = it_begin;
	if (it_end != m_symbols_by_addr2.rend()) {
	    while (it_end != m_symbols_by_addr2.rend() && it_end->second.addr < (addr + sz)) {
		++it_end;
		//std::cout << "move next to " << it_end->second.addr << std::endl;
	    }
	    m_symbols_by_addr2.erase(it_end.base(), it_begin.base());
	}
    } else {
	std::cout << "No JIT region at " << std::hex << addr << std::endl;
    }
}

routine_description* symbols_resolver::lookup_symbol(uint64_t addr) {
    locker_t _l(m_mutex);

    auto it = m_symbols_by_addr2.lower_bound(addr);

    size_t cntr = 10;
    while (cntr) {
	if (it != m_symbols_by_addr2.end()) {
	    auto end_addr = it->second.addr + it->second.len;
	    if (end_addr >= addr) {
		DEBUG(std::cout << "Lookup addr "  << std::hex << addr << " -> " << it->second.name << "@" << it->second.addr << "/" << it->second.len << std::endl);
		return &it->second;
	    }

	    ++it; // since map ordered with std::greater, we need to use '++' operator in order to get smaller addresses
	    cntr -= 1;
	} else {
	    break;
	}
    }

    return nullptr;
}

symbols_resolver static_resolver;
symbols_resolver* get_symbols_resolver() {
    return &static_resolver;
}

int load_symbols_from_dso(const char* dso, uint64_t any_sym_addr) {
    link_map* linkmap = nullptr;
    Dl_info info;
    auto status = ::dladdr1((void*)any_sym_addr, &info, (void**)&linkmap, RTLD_DL_LINKMAP);
    if (!(status && linkmap)) {
        return -1;
    }

    uint64_t offset = linkmap->l_addr;

    std::string cmd;
    cmd += "nm -a -S --demangle ";
    cmd += dso;
    std::cout << "CMD: " << cmd << std::endl;
    FILE* nm_stm = ::popen(cmd.c_str(), "r");

    if (nm_stm) {
        size_t addr;
        size_t sz;
        char type[10];
        char symbol[4096];

        while (!feof(nm_stm)) {
            char buf[10000] = {};
            if (fgets(buf, sizeof(buf), nm_stm) != NULL) {
                auto scanned = sscanf(buf, "%lx %lx %1s %[^\n]", &addr, &sz, type, symbol);
                if (scanned) {
                    get_symbols_resolver()->add_symbol(dso, symbol, addr + offset, addr, sz);
                }
            }
        }
        ::fclose(nm_stm);
	return 1;
    } else {
	auto err = errno;
	std::cout << "Error: " << err << std::endl;
	return -1;
    }
}

int read_memory(uint8_t *buffer, size_t size,const struct pt_asid *asid,uint64_t ip, void *context) {
    if (ip == 0) {
        return 0;
    }

    for (int i = 0; i < size; ++i) {
        buffer[i] = *(((uint8_t*)ip) + i);
    }
    return size;
}

struct profile_record {
    uint64_t total_counter;
    std::vector<std::string> symbol_name;
};

bool operator < (const profile_record& pr1, const profile_record& pr2) {
    return pr1.total_counter < pr2.total_counter;
}

static int
callback(struct dl_phdr_info *info, size_t size, void *data)
{
    int j;

    printf("name=%s (%d segments)\n", info->dlpi_name,
	   info->dlpi_phnum);

    for (j = 0; j < info->dlpi_phnum; j++)
	printf("\t\t header %2d: address=%10p/%p\n", j,
	       (void *) (info->dlpi_addr + info->dlpi_phdr[j].p_vaddr), info->dlpi_phdr[j].p_memsz);
    return 0;
}

void process_pt(char* pt_begin, size_t len, FILE* trace_output) {
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
    config.nom_freq = get_nom_freq();

    decoder = pt_blk_alloc_decoder(&config);
    if (!decoder) {
        std::cout << "can't allocate decoder" << std::endl;
    }

    auto default_image = pt_blk_get_image(decoder);
    pt_image_set_callback(default_image, read_memory, nullptr);

    uint64_t prev_timestamp = 0;
    uint64_t start_timestamp = 0;
    uint64_t start_ns = 0;

    std::unordered_map<std::string, uint64_t> profile;
    std::vector<std::string> symbols_the_same_time;

    std::set<std::string> processed_dso;

    while (true) {
    forward:

        auto status = pt_blk_sync_forward(decoder);
        if (status < 0) {
            DEBUG(std::cout << "can't sync" << std::endl);

            uint64_t new_sync;

            if (status == -pte_eos) {
                DEBUG(std::cout << "EOS!!!" << std::endl);

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
                    for (auto& symbol : item.symbol_name) {
                        std::cout << std::dec << tsc_to_ns(item.total_counter) << "ns\t[" << percent << "%]";
                        std::cout << "\t" << symbol << "\n";
                    }
                }
                std::cout << "Total time: " << std::dec << tsc_to_ns(total) << std::endl;

                return;
            }

            errcode = pt_blk_get_offset(decoder, &new_sync);
        }

        for (;;) {
            struct pt_block block = {};

            while (status & pts_event_pending) {
                struct pt_event event;
                status = pt_blk_event(decoder, &event, sizeof(event));

                if (status < 0) {
                    std::cout << "can't get pending event" << std::endl;
		    goto forward;
                    break;
                }
            }

            status = pt_blk_next(decoder, &block, sizeof(block));

            if (status < 0) {
                auto code = pt_error_code(-status);
                break;
            } else {
                std::string symbol_by_ip;

                uint64_t ip = (uint64_t)block.ip;
		uint64_t asm_ip = 0; // add in disassembly. relevant for code from DSO only

		bool resolved_once = false;
            try_resolve_symbol:

                {
                    auto symbol_info = get_symbols_resolver()->lookup_symbol(ip);
                    if (symbol_info != nullptr) {
                        symbol_by_ip = symbol_info->name;
                        symbol_by_ip += "@";
                        symbol_by_ip += symbol_info->dso;

			if (symbol_info->dso_addr) {
			    asm_ip = symbol_info->dso_addr + (ip - symbol_info->addr);
			}
                    } else {
                        Dl_info info;
                        Elf64_Sym* syment;
                        auto status = ::dladdr1((void*)ip, &info, (void**)&syment, RTLD_DL_SYMENT);
                        if (status) {
                            if (info.dli_fname && strlen(info.dli_fname)) {
				//std::cout << "dso: " << info.dli_fname << std::endl;
                                auto it = processed_dso.find(info.dli_fname);
                                if (it == processed_dso.end()) {
                                    processed_dso.insert(info.dli_fname);
                                    std::cout << "New DSO: " << info.dli_fname << std::endl;

                                    auto status = load_symbols_from_dso(info.dli_fname, ip);
				    if (status < 0) {
					std::cout << "Can't load symbold from DSO, status = " << status << std::endl;
				    }

                                    // if was brand new DSO. Let's try to lookup symbol one more time
                                    goto try_resolve_symbol;
                                }
                            } else {
				std::cout << "no DSO name for addr " << std::hex << ip << std::endl;
			    }
                        } else {
			    std::cout << "no DSO for addr " << std::hex << ip << std::endl;

			    routine_description key;

			    key.addr = ip;
			    key.dso_addr = 0;
			    key.len = 1;
			    key.name = "totally unknown";
			    get_symbols_resolver()->add_symbol(key);

			    goto try_resolve_symbol;
			}
			
			if (!resolved_once) {
			    resolved_once = true;
			} else {
			    std::cerr << "Something went terribly wrong... Cyclic lookup for symbol at " << std::hex << ip << std::endl;
			    // cyclic loop
			    *(int*)0 = 0;
			}

                        // if we are here, it's impossible to lookup any symbol for given IP
                        // let's just fill values from dladd call

                        routine_description key;

                        key.addr = info.dli_saddr ? (uint64_t)info.dli_saddr : ip;
                        if (syment) {
                            key.len = syment->st_size;
                        } else {
                            key.len = ip - (uint64_t)info.dli_saddr;
                        }
                        key.name = info.dli_sname ? info.dli_sname : "totally unknown";

                        get_symbols_resolver()->add_symbol(key);

			goto try_resolve_symbol;
                    }

                }

		uint64_t now;
		uint32_t lost_mtc;
		uint32_t lost_cyc;
		auto status = pt_blk_time(decoder, &now, &lost_mtc, &lost_cyc);
		if (status < 0) {
		    std::cout << "can't get tsc" << std::endl;
		}

		if (prev_timestamp > now) {
		    //std::cout << "Did you use time machine? " << prev_timestamp << " -> " << now << std::endl;
		    now = prev_timestamp;
		}

		if (prev_timestamp == 0) {
		    prev_timestamp = now;
		    start_timestamp = now;
		}

		symbols_the_same_time.push_back(symbol_by_ip);
		if (prev_timestamp != now) {

		    auto time_diff = now - prev_timestamp;
		    auto bb_timing = time_diff / symbols_the_same_time.size();

		    auto error = time_diff - (bb_timing * symbols_the_same_time.size());

		    for (auto& symbol : symbols_the_same_time) {
			profile[symbol] += bb_timing;
		    }
		    profile[symbols_the_same_time.back()] += error;

		    symbols_the_same_time.clear();

		    prev_timestamp = now;
		}

		auto ns_since_start = tsc_to_ns(now - start_timestamp);
		
		if (asm_ip) {
		    fprintf(trace_output, "Timestamp: %ld\t %s [%p/%p]\n", ns_since_start, symbol_by_ip.c_str(), ip, asm_ip);
		} else {
		    fprintf(trace_output, "Timestamp: %ld\t %s [%p]\n", ns_since_start, symbol_by_ip.c_str(), ip);
		}
		if (lost_mtc || lost_cyc) {
		    fprintf(trace_output, "Lost: %d\n", (lost_mtc + lost_cyc));
		}

		uint64_t new_sync;
		auto errcode = pt_blk_get_offset(decoder, &new_sync);
	    }
	}
    }
    pt_blk_free_decoder(decoder);
}
