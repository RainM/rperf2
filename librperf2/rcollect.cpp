#include "rcollect.hpp"

#include <atomic>
#include <thread>
#include <iostream>
#include <mutex>
#include <fstream>

#include <cstring>
#include <cassert>

#include <sys/types.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>

enum pt_tracer_step {
    PT_TRACER_UNKNOWN,
    PT_TRACER_WAITING_START,
    PT_TRACER_WORKING,
    PT_TRACER_WAITING_STOP,
    PT_TRACER_STOPPED
};

struct pt_tracer_context {
    std::thread tracer_thread;
    std::atomic<pt_tracer_step> step;
    struct pt_trace* trace;
    volatile int perf_fd;

    void* perf_data_ptr;
    size_t perf_data_sz;

    void* aux_data_ptr;
    size_t aux_data_sz;
};

std::atomic<pt_tracer_context*> g_state(nullptr);
std::mutex g_state_creation_lock;

typedef std::unique_lock<std::mutex> locker_t;

template <typename T>
T load_atomically(T* ptr) {
    return std::atomic_load<T>((std::atomic<T>*)ptr);
}

template <typename T>
void store_atomically(T* ptr, T value) {
    std::atomic_store((std::atomic<T>*)ptr, value);
}

struct pt_trace* create_pt_trace(int sz) {
    struct pt_trace* result = new pt_trace();

    result->addr = new char[sz];
    for (int i = 0; i < sz; ++i) {
        result->addr[i] = 0;
    }
    result->write_pos = result->addr;
    result->sz = sz;

    return result;
}

static int process_aux_record(int flags, struct pt_trace* trace, struct perf_event_mmap_page* header, void* aux) {
    if (flags & PERF_AUX_FLAG_TRUNCATED) {
        std::cout << "TRUNCATED" << std::endl;
        return -1;
    }

    auto head = load_atomically(&header->aux_head);
    auto tail = load_atomically(&header->aux_tail);

    auto head_idx = head % header->aux_size;
    auto tail_idx = tail % header->aux_size;

    if (head_idx < tail_idx) {
        // wrap-around
        auto sz = header->aux_size - tail_idx;
        ::memcpy(trace->write_pos, (char*)aux + tail_idx, sz);
        trace->write_pos += sz;
        tail_idx = 0;
    }
    if(head_idx) {
        auto sz = head_idx - tail_idx;
        ::memcpy(trace->write_pos, (char*)aux + tail_idx, sz);
        trace->write_pos += sz;
    }

    store_atomically(&header->aux_tail, head);

    //std::cout << "read: " << tail_idx << " -> " << head_idx << "\n";
}


struct perf_record_aux_sample {
    struct perf_event_header header;
    __u64    aux_offset;
    __u64    aux_size;
    __u64    flags;
    // ...
    // More variable-sized data follows, but we don't use it.
};

static int process_perf_record(struct pt_trace* trace, struct perf_event_mmap_page* header, void* perf_ptr, void* aux_ptr) {
    auto head = load_atomically(&header->data_head);
    auto tail = load_atomically(&header->data_tail);

    char* perf_data_ptr = (char*)header + header->data_offset;

    __u64 data_begin = (tail % header->data_size);
    __u64 data_end   = (head % header->data_size);

    while (data_begin != data_end) {
        struct perf_event_header* event_header = (struct perf_event_header*) (perf_data_ptr + data_begin);
        data_begin += event_header->size;
        assert(data_begin - 1 < header->data_size);

        if (data_begin != data_begin % header->data_size) {
            std::cout << "overlap!!!" << std::endl;
        } else {
            //std::cout << data_begin << " -> " << event_header->type << std::endl;
        }

        switch(event_header->type) {
        case PERF_RECORD_AUX:
        {
            perf_record_aux_sample* aux_event_header = (perf_record_aux_sample*)event_header;
            if (process_aux_record(aux_event_header->flags, trace, header, aux_ptr) < 0) {
                return -1;
            }
            break;
        }
        case PERF_RECORD_LOST:
            return -1;
        case PERF_RECORD_LOST_SAMPLES:
            return -2;
        }

        data_begin = data_begin % header->data_size;
    }

    store_atomically(&header->data_tail, head);
    return 1;
}

static void __thread_func() {

    {
        locker_t _l(g_state_creation_lock);
        // just to be sure the construction has been done
    }
    assert(g_state.load() != nullptr);
    auto state = g_state.load();
    while (true) {
        while (state->step.load() != PT_TRACER_WAITING_START) {

        }

//      std::cout << "thread starting PT" << std::endl;

        assert(state->perf_fd > 0);
        state->step.store(PT_TRACER_WORKING);

        {
            struct pollfd poll_fd[1] = { { state->perf_fd, POLLIN | POLLHUP, 0} };
            while (true) {
                int n_events = ::poll(
                    poll_fd,
                    1, /* 1 fd           */
                    -1 /* wait eternally */ );

                if (poll_fd[0].revents & POLLIN) {
//                  std::cout << "event!" << std::endl;



                    struct perf_event_mmap_page* header = (struct perf_event_mmap_page*)state->perf_data_ptr;

                    process_perf_record(state->trace, header, state->perf_data_ptr, state->aux_data_ptr);



                } else if (poll_fd[0].revents & POLLHUP) {
                    std::cout << "thread exited???" << std::endl;
                    break;
                } else {
                    std::cout << "unknown???" << std::endl;
                }

                if (state->step.load() == PT_TRACER_WAITING_STOP) {
                    break;
                }
            }
        }

        while (state->step.load() != PT_TRACER_WAITING_STOP) {

        }

//      std::cout << "stopping" << std::endl;
        state->step.store(PT_TRACER_STOPPED);
    }
}

static int open_perf_pt() {
    struct perf_event_attr attr = {};

    attr.size = sizeof(attr);

    {
        std::ifstream pt_id_fl("/sys/bus/event_source/devices/intel_pt/type");
        if (pt_id_fl) {
            pt_id_fl >> attr.type;
        } else {
            return -1;
        }
    }

    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.disabled = 1;
    attr.precise_ip = 3;
    attr.watermark = 1;
    attr.wakeup_watermark = 1;
    attr.aux_watermark = getpagesize();
    attr.config = 0x8c602;

    int result;
    pid_t target_tid = ::syscall(__NR_gettid);
    for (auto end_time = ::time(nullptr) + 3; end_time > ::time(nullptr);) {
        result = syscall(SYS_perf_event_open, &attr, target_tid, -1, -1, 0);
        if (result != -1 && result != EBUSY) {
            return result;
        }
    }
    return -1; // timeout
}

int allocate_memory_for_perf_data(pt_tracer_context* ctx, int perf_data_sz_pages, int aux_data_sz_pages) {
    ctx->perf_data_sz = ( 1 + perf_data_sz_pages) * getpagesize();
    ctx->perf_data_ptr = ::mmap(nullptr, ctx->perf_data_sz, PROT_WRITE, MAP_SHARED, ctx->perf_fd, 0);
    if (ctx->perf_data_ptr == MAP_FAILED) {
        return -1;
    }

    struct perf_event_mmap_page* header = (struct perf_event_mmap_page*)ctx->perf_data_ptr;
    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size = aux_data_sz_pages * getpagesize();

    ctx->aux_data_sz = header->aux_size;
    ctx->aux_data_ptr = ::mmap(NULL, ctx->aux_data_sz, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->perf_fd, header->aux_offset);
    if (ctx->aux_data_ptr == MAP_FAILED) {
        return -1;
    }

    return 1;
}

int pt_tracer_start(struct pt_trace* trace, int perf_data_pages, int aux_data_pages) {

    if (!g_state.load()) {
        // does double-lock here
        locker_t _l(g_state_creation_lock);
        if (!g_state.load()) {
            pt_tracer_context* ctx = new pt_tracer_context();
            ctx->tracer_thread = std::thread(&__thread_func);
            ctx->step.store(PT_TRACER_UNKNOWN);

            g_state.store(ctx);
        }
    }

    auto state = g_state.load();
    assert(state != nullptr);
    assert(state->step.load() == PT_TRACER_UNKNOWN);

    state->trace = trace;
    trace->write_pos = trace->addr;

    state->perf_fd = open_perf_pt();
    if (state->perf_fd < 0) {
        return -1;
    }

    if (allocate_memory_for_perf_data(state, perf_data_pages, aux_data_pages) < 0) {
        ::close(state->perf_fd);
        return -1;
    }

    assert(state->perf_data_ptr != nullptr);
    assert(state->perf_data_sz != 0);
    assert(state->aux_data_ptr != nullptr);
    assert(state->aux_data_sz != 0);

    state->step.store(PT_TRACER_WAITING_START);

    while (state->step.load() != PT_TRACER_WORKING) {
        // just spin here a bit
    }

    if (::ioctl(state->perf_fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        ::munmap(state->aux_data_ptr, state->aux_data_sz);
        ::munmap(state->perf_data_ptr, state->perf_data_sz);
        ::close(state->perf_fd);
        return -1;
    }

    //std::cout << "Pt has been started" << std::endl;
}

int pt_tracer_stop() {
    auto state = g_state.load();
    assert(state->step.load() == PT_TRACER_WORKING);

    state->step.store(PT_TRACER_WAITING_STOP);

    while (state->step.load() != PT_TRACER_STOPPED) {

    }

    ::munmap(state->aux_data_ptr, state->aux_data_sz);
    ::munmap(state->perf_data_ptr, state->perf_data_sz);
    ::close(state->perf_fd);

    state->step.store(PT_TRACER_UNKNOWN);

    //std::cout << "pt was stopped" << std::endl;
}
