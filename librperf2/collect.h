#pragma once

#include <linux/types.h>
#include <stddef.h>
//#include <intel-pt.h>
#include <pthread.h>
#include "perf_pt_private.h"

/*
 * Passed from Rust to C to configure tracing.
 * Must stay in sync with the Rust-side.
 */
struct perf_pt_config {
    size_t      data_bufsize;          // Data buf size (in pages).
    size_t      aux_bufsize;           // AUX buf size (in pages).
  //    size_t      initial_trace_bufsize; // Initial capacity (in bytes) of a
                                       // trace storage buffer.
};

/*
 * Stores all information about the tracer.
 * Exposed to Rust only as an opaque pointer.
 */
struct tracer_ctx {
    pthread_t           tracer_thread;      // Tracer thread handle.
    struct perf_pt_cerror
                        tracer_thread_err;  // Errors from inside the tracer thread.
//    int                 stop_fds[2];        // Pipe used to stop the poll loop.
    int                 perf_fd;            // FD used to talk to the perf API.
    void                *aux_buf;           // Ptr to the start of the the AUX buffer.
    size_t              aux_bufsize;        // The size of the AUX buffer's mmap(2).
    void                *base_buf;          // Ptr to the start of the base buffer.
    size_t              base_bufsize;       // The size the base buffer's mmap(2).
};


/*
 * The manually malloc/free'd buffer managed by the Rust side.
 * To understand why this is split out from `struct perf_pt_trace`, see the
 * corresponding struct in the Rust side.
 */
struct perf_pt_trace_buf {
    void *p;
};

/*
 * Storage for a trace.
 *
 * Shared with Rust code. Must stay in sync.
 */
struct perf_pt_trace {
    struct perf_pt_trace_buf buf;
    __u64 len;
    __u64 capacity;
};


// Exposed Prototypes.
struct tracer_ctx *perf_pt_init_tracer(struct perf_pt_config *, struct perf_pt_cerror *);
bool perf_pt_start_tracer(struct tracer_ctx *, struct perf_pt_trace *, struct perf_pt_cerror *);
bool perf_pt_stop_tracer(struct tracer_ctx *tr_ctx, struct perf_pt_cerror *);
bool perf_pt_free_tracer(struct tracer_ctx *tr_ctx, struct perf_pt_cerror *);

