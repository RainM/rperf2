#include "rdtsc_utils.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdio.h>

#include <iostream>

#define NOMINAL_FREQ_EXT ".nominal_freq"

static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

double tsc_to_ns_multiplicator;
double tsc_to_ns_scale_factor;
uint64_t nominal_freq;


uint64_t tsc_to_ns(uint64_t ts) {
    return (ts * tsc_to_ns_multiplicator) / tsc_to_ns_scale_factor;
}

int get_nom_freq() {
    return nominal_freq;
}

static void set_tsc_to_ns_consts() {
    struct timespec start_time, end_time;
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    auto start_tsc = rdtsc();

    struct timespec ts1, ts2;
    

    //////// do sleep 100ms ///////////////////
    ts1.tv_sec = 0;
    ts1.tv_nsec = 100 /*ms*/ * 1000 /*us in ms*/ * 1000 /*ns in us*/;
    ::nanosleep(&ts1, &ts2);
    ///////////////////////////////////////////

    auto end_tsc = rdtsc();
    ::clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    auto duration_ns = (uint64_t)end_time.tv_sec - (uint64_t)start_time.tv_sec;
    duration_ns *= 1000000000;
    duration_ns += end_time.tv_nsec;
    duration_ns -= start_time.tv_nsec;
    
    auto tsc_diff = end_tsc - start_tsc;

    tsc_to_ns_scale_factor = 1 << 20;
    tsc_to_ns_multiplicator = (duration_ns * tsc_to_ns_scale_factor) / tsc_diff;
}

static void set_nominal_freq() {
    Dl_info info;
    auto status = ::dladdr((void*)&tsc_to_ns, &info);
    if (status) {
	if (info.dli_fname) {
	    std::string nom_freq_file;
	    nom_freq_file += info.dli_fname;
	    nom_freq_file += NOMINAL_FREQ_EXT;

	    FILE* nom_freq_stm = ::fopen(nom_freq_file.c_str(), "r");
	    if (nom_freq_stm) {
		if (::fread(&nominal_freq, 1, sizeof(nominal_freq), nom_freq_stm) == sizeof(nominal_freq)) {
		    ::fclose(nom_freq_stm);
		} else {
		    ::fclose(nom_freq_stm);
		    std::cout << "Can't read nominal freq from file" << std::endl;
		}
	    } else {
		std::cout << "Cant open file with nominal freq" << std::endl;
		std::cout << "Try to run $ sudo " << info.dli_fname << std::endl;
	    }
	} else {
	    std::cout << "Can't lookup DSO name" << std::endl;
	}
    } else {
	std::cout << "Cant find required symbol" << std::endl;
    }

    ::exit(1);
}

bool run_as_executable() {
    auto executable_ptr = ::dlopen(nullptr, 0);
    auto dso_ptr = ::dlopen("librperf2.so", 0);

    return executable_ptr == dso_ptr;
}

__attribute__((constructor))
static void init() {
    if (!run_as_executable) {
	set_tsc_to_ns_consts();
	set_nominal_freq();
    }
}

#define MSR_PLATFORM_INFO_OFFSET 0xce

int main() {
    char path[PATH_MAX] = {};
    if (::readlink("/proc/self/exe", path, PATH_MAX) != -1) {
	std::cout << "exe path: " << path << std::endl;
    } else {
	std::cout << "Can't read symlink" << std::endl;
	return 1;
    }

    int fd = ::open("/dev/cpu/0/msr", O_RDONLY);
    if (fd < 0) {
	std::cout << "Can't open /dev/cpu/0/msr. Are you root?" << std::endl;
	return 1;
    }
    
    if (::lseek(fd, MSR_PLATFORM_INFO_OFFSET, SEEK_CUR) == -1) {
	std::cout << "Can't seek at MSR file" << std::endl;
	::close(fd);
	return 1;
    }

    uint64_t nom_freq;
    if (::read(fd, &nom_freq, sizeof(nom_freq)) != sizeof(nom_freq)) {
	std::cout << "Can't read from MSR file" << std::endl;
	::close(fd);
	return 1;
    }

    nom_freq = (nom_freq & 0xff00) >> 8;

    std::cout << "Successlull read for MSR. Nominal freq is " << nom_freq << std::endl;
    ::close(fd);
    

    std::string out_file_name(path);
    out_file_name += NOMINAL_FREQ_EXT;
    FILE* out_file_stm = ::fopen(out_file_name.c_str(), "w");
    if (out_file_stm) {
	auto written = ::fwrite(&nom_freq, 1, sizeof(nom_freq), out_file_stm);
	if (written != sizeof(nom_freq)) {
	    std::cout << "Can't write data to file" << std::endl;
	    ::fclose(out_file_stm);
	    return 1;
	}
	::fclose(out_file_stm);
    } else {
	std::cout << "Can't open target file: " << out_file_name << std::endl;
	return 1;
    }

    return 0;
}
