#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <vector>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <fstream>

#include <sys/types.h>

#include "prof_impl.hpp"
#include "rcollect.hpp"
#include "pt_parser.hpp"

struct perf_pt_prof_state {
    std::vector<uint64_t> timings;
    int64_t skip_counter;
};

pid_t gettid() {
    return syscall(SYS_gettid);
}

perf_pt_prof_engine::perf_pt_prof_engine(int skip_n, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest):
        m_skip_cntr(skip_n)
    {
        m_data_sz = trace_sz;
        m_data_buf = ::malloc(m_data_sz);

	if (strcmp(trace_dest.c_str(), "none") == 0) {
	    m_trace_target = ::fopen("/dev/null", "w");
	} else if (strcmp(trace_dest.c_str(), "stdout") == 0) {
	    m_trace_target = stdout;
	} else {
	    m_trace_target = ::fopen(trace_dest.c_str(), "w");
	}

	m_trace = ::create_pt_trace(100000000);
    }

void perf_pt_prof_engine::start() {
        if (--m_skip_cntr <= 0) {
            do_start();
        }
    }

void perf_pt_prof_engine::stop() {
        if (m_skip_cntr <= 0) {
            do_stop();
        }
    }

void perf_pt_prof_engine::start_prof() {
	pt_tracer_start(m_trace, 16, 256);

        ::clock_gettime(CLOCK_MONOTONIC_RAW, &m_start_time);
    }

uint64_t perf_pt_prof_engine::stop_prof() {

        struct timespec end_time;
        ::clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);
        pid_t tid = ::gettid();
        auto duration = (uint64_t)end_time.tv_sec - (uint64_t)m_start_time.tv_sec;
        duration *= 1000000000;
        duration += end_time.tv_nsec;
        duration -= m_start_time.tv_nsec;

	pt_tracer_stop();

        return duration;
    }

void perf_pt_prof_engine::parse_trace() {
        process_pt(m_trace->addr, m_trace->sz, m_trace_target);
        std::cout << "Profiling DONE" << std::endl;
        ::exit(1);
    }

struct perf_pt_prof_first_occurence: perf_pt_prof_engine {
    perf_pt_prof_first_occurence(int skip_n, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest):
        perf_pt_prof_engine(skip_n, buf_sz, aux_sz, trace_sz, trace_dest) {}

    virtual void do_start() override {
        start_prof();
    }
    virtual void do_stop() override {
        stop_prof();
        parse_trace();
    }

    virtual ~perf_pt_prof_first_occurence() {

    }
};

struct perf_pt_prof_percentile: perf_pt_prof_engine {
    perf_pt_prof_percentile(int skip_n, double percentile, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest):
        perf_pt_prof_engine(skip_n, buf_sz, aux_sz, trace_sz, trace_dest), m_percentile(percentile), m_percentile_timing(0) {

        double one_minus_n = (double)1 - percentile;
        m_number_of_iterations = ceil((double)1 / one_minus_n) - 1;
        m_timings.reserve(m_number_of_iterations);
    }

    virtual void do_start() override {
        start_prof();
    }
    virtual void do_stop() override {
        auto elapsed = stop_prof();
        if (m_percentile_timing) {
	    if (elapsed > m_percentile_timing) {
		std::cout << "Processing trace with length " << elapsed << " > " << m_percentile_timing << std::endl;
		parse_trace();
	    }
        } else {
            m_timings.push_back(elapsed);
            if (m_timings.size() == m_number_of_iterations) {
		std::sort(std::begin(m_timings), std::end(m_timings));
                m_percentile_timing = m_timings.back();
		m_median_timing = m_timings[m_timings.size()/2];
                std::cout << "Median: " << m_median_timing << "ns\n";
                std::cout << "Timing for " << m_percentile << " timings: " << m_percentile_timing << std::endl;
            }
        }
    }

    virtual ~perf_pt_prof_percentile() {

    }

private:
    std::vector<uint64_t> m_timings;
    uint64_t m_number_of_iterations;
    double m_percentile;
    uint64_t m_percentile_timing;
    uint64_t m_median_timing;
};

perf_pt_prof_engine* create_prof_engine(int skip_n, double percentile, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest) {
    if (percentile <= 0.5) {
        return new perf_pt_prof_first_occurence(skip_n, buf_sz, aux_sz, trace_sz, trace_dest);
    } else {
        return new perf_pt_prof_percentile(skip_n, percentile, buf_sz, aux_sz, trace_sz, trace_dest);
    }
}
