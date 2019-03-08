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

#include "ru_raiffeisen_PerfPtProf.h"
#include "jni.h"

#include "prof_impl.hpp"

struct perf_pt_prof_engine* g_prof_engine;

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
