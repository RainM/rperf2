#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
#endif
void process_pt(char* pt_begin, size_t len);

#if defined(__cplusplus)
#include <string>

void add_symbol(const std::string& symbol_name, uint64_t addr, size_t len);
#endif

//void start_pt(uint32_t sz);
//char* stop_pt(uint32_t* sz_actual);
