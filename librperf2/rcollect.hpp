#pragma once

#include <cstddef>

struct pt_tracer_ctx {
    int perf_fd;
};

struct pt_trace {
    char* addr;
    char* write_pos;
    size_t sz;
};

struct pt_trace* create_pt_trace(int sz);

int pt_tracer_start(struct pt_trace* trace, int perf_data_pages, int aux_data_pages);

int pt_tracer_stop();
