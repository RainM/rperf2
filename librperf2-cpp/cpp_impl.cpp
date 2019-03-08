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
#include <mutex>
#include <sys/types.h>

#include "prof_impl.hpp"
#include "librperf2_cpp.hpp"

struct perf_pt_prof_engine* g_prof_engine;

void _pt_profiler_init
(
    int skip_n,
    double percentile,
    int buf_sz,
    int aux_sz,
    int trace_sz,
    const char* trace_direction)
{
    const char* target;
    if (trace_direction) {
	target = trace_direction;
    } else {
	target = "stdout";
    }

    g_prof_engine = create_prof_engine(skip_n, percentile, buf_sz, aux_sz, trace_sz, target);
}

__attribute__((constructor))
static void do_init() {
    auto skip_n_str = std::getenv("PT_PROF_SKIP_N");
    auto percentile_str = std::getenv("PT_PROF_PERCENTILE");
    auto buf_sz_str = std::getenv("PT_PROF_BUF_SZ");
    auto aux_sz_str = std::getenv("PT_PROF_AUX_SZ");
    auto trace_sz_str = std::getenv("PT_PROF_TRACE_SZ");
    auto trace_direction_str = std::getenv("PT_PROF_TRACE_DEST");

    auto skip_n = 0;
    if (skip_n_str) {
	skip_n = ::atoi(skip_n_str);
    }

    auto percentile = 0.0;
    if (percentile_str) {
	percentile = ::atof(percentile_str);
    }

    auto buf_sz = 32;
    if (buf_sz_str) {
	buf_sz = ::atoi(buf_sz_str);
    }

    auto aux_sz = 256;
    if (aux_sz_str) {
	aux_sz = ::atoi(aux_sz_str);
    }

    auto trace_sz = 10000000;
    if (trace_sz_str) {
	trace_sz = ::atoi(trace_sz_str);
    }

    const char* trace_direction = "stdout";
    if (trace_direction_str) {
	trace_direction = trace_direction_str;
    }

    std::cout << "Profiler start args:" << std::endl;
    std::cout << "\tSKIP_N: " << skip_n << std::endl;
    std::cout << "\tPERCENTILE: " << percentile << std::endl;
    std::cout << "\tBUF_SZ: " << buf_sz << std::endl;
    std::cout << "\tAUX_SZ: " << aux_sz << std::endl;
    std::cout << "\tTRACE_SZ: " << trace_sz << std::endl;
    std::cout << "\tTRACE_DEST: " << trace_direction << std::endl; 

    _pt_profiler_init(
	skip_n,
	percentile,
	buf_sz,
	aux_sz,
	trace_sz,
	trace_direction);

}

std::atomic<bool> thread_safety_checker(false);
std::mutex locker;


void _pt_profiler_start() {

one_more_time:
    auto old = thread_safety_checker.exchange(true);
    if (old != false) {
	goto one_more_time;
    }

    g_prof_engine->start();
}

void _pt_profiler_stop() {

        g_prof_engine->stop();

one_more_time:
    auto old = thread_safety_checker.exchange(false);
    if (old != true) {
	std::cout << "WTF?????!!!!" << std::endl;
	::exit(2);
    }
}
