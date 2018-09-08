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

#include <sys/types.h>

#include "ru_raiffeisen_PerfPtProf.h"
#include "jni.h"

/*
extern "C" {
#include "collect.h"
}
*/

#include "rcollect.hpp"

#include "pt_parser.hpp"

struct perf_pt_prof_engine;

struct perf_pt_prof_engine* g_prof_engine;

struct perf_pt_prof_state {
    std::vector<uint64_t> timings;
    int64_t skip_counter;
};


pid_t gettid() {
    return syscall(SYS_gettid);
}

struct perf_pt_prof_engine {
    virtual ~perf_pt_prof_engine() {}

    perf_pt_prof_engine(int skip_n, long buf_sz, long aux_sz, long trace_sz, const std::string& trace_dest):
        m_skip_cntr(skip_n)
    {
        //m_tracer_config.data_bufsize = buf_sz;
        //m_tracer_config.aux_bufsize = aux_sz;

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

    void start() {
        if (--m_skip_cntr <= 0) {
            do_start();
        }
    }

    void stop() {
        if (m_skip_cntr <= 0) {
            do_stop();
        }
    }

protected:
    virtual void do_start() = 0;
    virtual void do_stop() = 0;

    void start_prof() {
        //::memset(m_trace->addr, 0, m_trace->sz);

	pt_tracer_start(m_trace, 16, 256);

        ::clock_gettime(CLOCK_MONOTONIC_RAW, &m_start_time);
    }

    uint64_t stop_prof() {

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

    void parse_trace() {
        process_pt(m_trace->addr, m_trace->sz, m_trace_target);
        std::cout << "Profiling DONE" << std::endl;
        ::exit(1);
    }

private:
    struct timespec m_start_time;
    int m_data_sz;
    void* m_data_buf;
    int64_t m_skip_cntr;
    struct tracer_ctx* m_tracer_ctx;
    struct pt_trace* m_trace;
    FILE* m_trace_target;
};

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

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    init
 * Signature: (IDJJJLjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_init
(
    JNIEnv* env,
    jclass,
    jint skip_n,
    jdouble percentile,
    jlong buf_sz,
    jlong aux_sz,
    jlong trace_sz,
    jstring trace_direction)
{
    const char* target;
    if (trace_direction) {
	target = env->GetStringUTFChars(trace_direction, 0);
    } else {
	target = "stdout";
    }

    g_prof_engine = create_prof_engine(skip_n, percentile, buf_sz, aux_sz, trace_sz, target);

    if (trace_direction) {
	env->ReleaseStringUTFChars(trace_direction, target);
    }
}

std::atomic<bool> thread_safety_checker(false);
std::mutex locker;
/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_start(JNIEnv *, jclass) {

one_more_time:
    auto old = thread_safety_checker.exchange(true);
    if (old != false) {
	//std::cout << "Concurrent run detected!" << std::endl;
	goto one_more_time;
    }

    g_prof_engine->start();
}

/*
 * Class:     ru_raiffeisen_PerfPtProf
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_ru_raiffeisen_PerfPtProf_stop(JNIEnv *, jclass) {

        g_prof_engine->stop();

one_more_time:
    auto old = thread_safety_checker.exchange(false);
    if (old != true) {
	std::cout << "WTF?????!!!!" << std::endl;
	::exit(2);
    }
}

#include <execinfo.h>

extern "C" void perf_pt_set_err(struct perf_pt_cerror *, int, int) {
    int j, nptrs;
    void *buffer[100];
    char **strings;

    nptrs = backtrace(buffer, 100);
    printf("backtrace() returned %d addresses\n", nptrs);

    /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
       would produce similar output to the following: */

    strings = backtrace_symbols(buffer, nptrs);
    if (strings == NULL) {
        perror("backtrace_symbols");
        exit(EXIT_FAILURE);
    }

    for (j = 0; j < nptrs; j++)
        printf("%s\n", strings[j]);

    free(strings);


    ::exit(11);
}
