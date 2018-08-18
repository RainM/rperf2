#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C"
#endif
void process_pt(char* pt_begin, size_t len, FILE* trace_output);

#if defined(__cplusplus)
#include <string>
#include <map>

//void add_symbol(const std::string& dso, const std::string& symbol_name, uint64_t addr, size_t len);

struct routine_description {
    uint64_t addr;
    size_t len;
    std::string name;
    std::string dso;
};

struct symbols_resolver {
    void add_symbol(const routine_description& rd);
    void add_symbol(const std::string& dso, const std::string& symbol_name, uint64_t addr, size_t len);
    routine_description* lookup_symbol(uint64_t addr);

    int size();
private:
    std::map<uint64_t, routine_description, std::greater<uint64_t>> m_symbols_by_addr2;
};

symbols_resolver* get_symbols_resolver();

#endif

//void start_pt(uint32_t sz);
//char* stop_pt(uint32_t* sz_actual);
