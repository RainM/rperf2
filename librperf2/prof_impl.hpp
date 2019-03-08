#pragma once

#include <string>
#include <stdio.h>
#include <time.h>

struct perf_pt_prof_engine;

perf_pt_prof_engine* create_prof_engine(int skip_n, double percentile, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest);

struct pt_trace;
struct tracer_ctx;

struct perf_pt_prof_engine {
    virtual ~perf_pt_prof_engine() {}

    perf_pt_prof_engine(int skip_n, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest);

    void start();

    void stop();

protected:
    virtual void do_start() = 0;
    virtual void do_stop() = 0;

    void start_prof();
    uint64_t stop_prof();
    void parse_trace();

private:
    struct timespec m_start_time;
    int m_data_sz;
    void* m_data_buf;
    int64_t m_skip_cntr;
    struct tracer_ctx* m_tracer_ctx;
    struct pt_trace* m_trace;
    FILE* m_trace_target;
};
