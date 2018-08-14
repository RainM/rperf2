// Copyright (c) 2017-2018 King's College London
// created by the Software Development Team <http://soft-dev.org/>
//
// The Universal Permissive License (UPL), Version 1.0
//
// Subject to the condition set forth below, permission is hereby granted to any
// person obtaining a copy of this software, associated documentation and/or
// data (collectively the "Software"), free of charge and under any and all
// copyright rights in the Software, and any and all patent rights owned or
// freely licensable by each licensor hereunder covering either (i) the
// unmodified Software as contributed to or provided by such licensor, or (ii)
// the Larger Works (as defined below), to deal in both
//
// (a) the Software, and
// (b) any piece of software and/or hardware listed in the lrgrwrks.txt file
// if one is included with the Software (each a "Larger Work" to which the Software
// is contributed by such licensors),
//
// without restriction, including without limitation the rights to copy, create
// derivative works of, display, perform, and distribute the Software and make,
// use, sell, offer for sale, import, export, have made, and have sold the
// Software and the Larger Work(s), and to sublicense the foregoing rights on
// either these or other terms.
//
// This license is subject to the following condition: The above copyright
// notice and either this complete permission notice or at a minimum a reference
// to the UPL must be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <syscall.h>
#include <sys/mman.h>
#include <poll.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <semaphore.h>
#include "hwtracer_util.h"
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdatomic.h>
//#include <intel-pt.h>

#include "perf_pt_private.h"

#include "collect.h"

#define SYSFS_PT_TYPE   "/sys/bus/event_source/devices/intel_pt/type"
#define MAX_PT_TYPE_STR 8

#define MAX_OPEN_PERF_TRIES  20000
#define OPEN_PERF_WAIT_NSECS 1000 * 30

#define AUX_BUF_WAKE_RATIO 0.1

#ifndef INFTIM
#define INFTIM -1
#endif

struct tracer_thread_args;

bool is_thread_started = false;
volatile struct tracer_thread_args* context_ptr = NULL;
volatile bool error_flag = false;

enum {
    GATHER_STATE_UNKNOWN,
    GATHER_STATE_WORKING,
    GATHER_STATE_STOPPING
};

/*volatile*/ int gather_thread_state;

/*
 * Stuff used in the tracer thread
 */
struct tracer_thread_args {
    int                 perf_fd;            // Perf notification fd.
    struct perf_pt_trace
                        *trace;             // Pointer to trace storage.
    void                *aux_buf;           // The AUX buffer itself;
    struct perf_event_mmap_page
                        *base_header;       // Pointer to the header in the base buffer.
    struct perf_pt_cerror
                        *err;               // Errors generated inside the thread.
};

// A data buffer sample indicating that new data is available in the AUX
// buffer. This struct is not defined in a perf header, so we have to define it
// ourselves.
struct perf_record_aux_sample {
    struct perf_event_header header;
    __u64    aux_offset;
    __u64    aux_size;
    __u64    flags;
    // ...
    // More variable-sized data follows, but we don't use it.
};

// The format of the data returned by read(2) on a Perf file descriptor.
// Note that the size of this will change if you change the Perf `read_format`
// config field (more fields become available).
struct read_format {
    __u64 value;
};

// Private prototypes.
static bool handle_sample(void *, struct perf_event_mmap_page *, struct
                          perf_pt_trace *, void *, struct perf_pt_cerror *);
static bool read_aux(void *, struct perf_event_mmap_page *,
                     struct perf_pt_trace *, struct perf_pt_cerror *);
static bool poll_loop(int, struct perf_event_mmap_page *, void *,
                      struct perf_pt_trace *, struct perf_pt_cerror *);
static void *tracer_thread(void *);
static int open_perf(size_t, struct perf_pt_cerror *);

/*
 * Called when the poll(2) loop is woken up with a POLL_IN. Samples are read
 * from the Perf data buffer and an action is invoked for each depending its
 * type.
 *
 * Returns true on success, or false otherwise.
 */
static bool
handle_sample(void *aux_buf, struct perf_event_mmap_page *hdr,
              struct perf_pt_trace *trace, void *data_tmp,
              struct perf_pt_cerror *err)
{
    // We need to use atomics with orderings to protect against 2 cases.
    //
    // 1) It must not be possible to read the data buffer before the most
    //    recent head is obtained. This would mean that we may read nothing when
    //    there is really data available.
    //
    // 2) We must ensure that we have already copied out of the data buffer
    //    before we update the tail. Failure to do so would allow the kernel to
    //    re-use the space we have just "marked free" before we copied it.
    //
    // The initial load of the tail is relaxed since we are the only thread
    // mutating it and we don't mind variations on the ordering.
    //
    // See the following comment in the Linux kernel sources for more:
    // https://github.com/torvalds/linux/blob/3be4aaf4e2d3eb95cce7835e8df797ae65ae5ac1/kernel/events/ring_buffer.c#L60-L85
    void *data = (void *) hdr + hdr->data_offset;
    __u64 head_monotonic =
            atomic_load_explicit((_Atomic __u64 *) &hdr->data_head,
                                 memory_order_acquire);
    __u64 size = hdr->data_size; // No atomic load. Constant value.
    __u64 head = head_monotonic % size; // Head must be manually wrapped.
    __u64 tail_monotonic = atomic_load_explicit((_Atomic __u64 *) &hdr->data_tail,
                                      memory_order_relaxed);
    __u64 tail = tail_monotonic % size;

    // Copy samples out, removing wrap in the process.
    void *data_tmp_end = data_tmp;
    if (tail <= head) {
        // Not wrapped.
        memcpy(data_tmp, data + tail, head - tail);
        data_tmp_end += head - tail;
    } else {
        // Wrapped.
        memcpy(data_tmp, data + tail, size - tail);
        data_tmp_end += size - tail;
        memcpy(data_tmp + size - tail, data, head);
        data_tmp_end += head;
    }
    atomic_store_explicit((_Atomic __u64 *) &hdr->data_tail, head_monotonic, memory_order_relaxed);

    //printf("DATA: head = %d, mono head = %d, tail= %d\n", head, head_monotonic, tail);

    void *next_sample = data_tmp;
    while (next_sample != data_tmp_end) {
        struct perf_event_header *sample_hdr = next_sample;
        struct perf_record_aux_sample *rec_aux_sample;

	//printf("HDR with type %d\n", sample_hdr->type);

        switch (sample_hdr->type) {
        case PERF_RECORD_AUX:
                // Data was written to the AUX buffer.
                rec_aux_sample = next_sample;
		//printf("AUX available: %d at %d\n", rec_aux_sample->aux_size, rec_aux_sample->aux_offset);
                // Check that the data written into the AUX buffer was not
                // truncated. If it was, then we didn't read out of the data buffer
                // quickly/frequently enough.
                if (rec_aux_sample->flags & PERF_AUX_FLAG_TRUNCATED) {
		    //perf_pt_set_err(err, perf_pt_cerror_ipt, pte_overflow);
		    printf("TRUNCATED\n");
                    return false;
                }
                if (read_aux(aux_buf, hdr, trace, err) == false) {
		    printf("CAN't read AUX\n");
                    return false;
                }
                break;
            case PERF_RECORD_LOST:
                //perf_pt_set_err(err, perf_pt_cerror_ipt, pte_overflow);
		printf("LOST\n");
                return false;
                break;
            case PERF_RECORD_LOST_SAMPLES:
                // Shouldn't happen with PT.
                errx(EXIT_FAILURE, "Unexpected PERF_RECORD_LOST_SAMPLES sample");
                break;
        }
        next_sample += sample_hdr->size;
    }

    return true;
}


volatile int recorded_cnt;

void wait_next_recorded_bunch() {

    //return ;

    int recorded_now = recorded_cnt;
    while (recorded_now == recorded_cnt && (!error_flag)) {}

    if (error_flag) {
	printf("Error flag is set. skip polling\n");
    }
}


/*
 * Read data out of the AUX buffer.
 *
 * Reads from `aux_buf` (whose meta-data is in `hdr`) into `trace`.
 */
bool
read_aux(void *aux_buf, struct perf_event_mmap_page *hdr,
         struct perf_pt_trace *trace, struct perf_pt_cerror *err)
{
  recorded_cnt += 1;


    // Use of atomics here for the same reasons as for handle_sample().
    __u64 head_monotonic =
            atomic_load_explicit((_Atomic __u64 *) &hdr->aux_head,
                                 memory_order_acquire);
    __u64 size = hdr->aux_size; // No atomic load. Constant value.
    __u64 head = head_monotonic % size; // Head must be manually wrapped.
    __u64 tail_monotonic = atomic_load_explicit((_Atomic __u64 *) &hdr->aux_tail,
                                 memory_order_relaxed);
    __u64 tail = tail_monotonic % size;

    // Figure out how much more space we need in the trace storage buffer.
    __u64 new_data_size;
    if (tail <= head) {
        // No wrap-around.
        new_data_size = head - tail;
    } else {
        // Wrap-around.
        new_data_size = (size - tail) + head;
    }

    // Reallocate the trace storage buffer if more space is required.
    __u64 required_capacity = trace->len + new_data_size;
    if (required_capacity > trace->capacity) {
	printf("REALLOC!!!! %d -> %d\n",  trace->capacity, required_capacity);
        // Over-allocate to 2x what we need, checking that the result fits in
        // the size_t argument of realloc(3).
        if (required_capacity >= SIZE_MAX / 2) {
            // We would overflow the size_t argument of realloc(3).
            perf_pt_set_err(err, perf_pt_cerror_errno, ENOMEM);
	    printf("SIZE overflow\n");
            return false;
        }
        size_t new_capacity = required_capacity * 2;
        void *new_buf = realloc(trace->buf.p, new_capacity);
        if (new_buf == NULL) {
            perf_pt_set_err(err, perf_pt_cerror_errno, errno);
	    printf("Can't realloc\n");
            return false;
        }
        trace->capacity = new_capacity;
        trace->buf.p = new_buf;
    }
    
    // Finally append the new AUX data to the end of the trace storage buffer.
    if (tail <= head) {
        memcpy(trace->buf.p + trace->len, aux_buf + tail, head - tail);
        trace->len += head - tail;
    } else {
        memcpy(trace->buf.p + trace->len, aux_buf + tail, size - tail);
        trace->len += size - tail;
        memcpy(trace->buf.p + trace->len, aux_buf, head);
        trace->len += size + head;
    }

    //printf("AUX: %d, head = %d, mono head = %d, tail= %d\n", trace->len, head, head_monotonic, tail);
    atomic_store_explicit((_Atomic __u64 *) &hdr->aux_tail, head_monotonic, memory_order_release);
    return true;
}

/*
 * Take trace data out of the AUX buffer.
 *
 * Returns true on success and false otherwise.
 */
static bool
poll_loop(int perf_fd, /*int stop_fd,*/ struct perf_event_mmap_page *mmap_hdr,
          void *aux, struct perf_pt_trace *trace, struct perf_pt_cerror *err)
{
    int n_events = 0;
    bool ret = true;
    struct pollfd pfds[2] = {
        {perf_fd,   POLLIN | POLLHUP,   0}
//,
//        {stop_fd,   POLLHUP,            0}
    };

    // Temporary space for new samples in the data buffer.
    void *data_tmp = malloc(mmap_hdr->data_size);
    if (data_tmp == NULL) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = false;
        goto done;
    }

    while (1) {
        n_events = poll(pfds, 1, INFTIM);
        if (n_events == -1) {
            perf_pt_set_err(err, perf_pt_cerror_errno, errno);
            ret = false;
            goto done;
        }

	//printf("poll with %d events\n", n_events);
	
        // POLLIN on pfds[0]: Overflow event on either the Perf AUX or data buffer.
        // POLLHUP on pfds[1]: Tracer stopped by parent.
        if ((pfds[0].revents & POLLIN) || (pfds[1].revents & POLLHUP)) {
            // Read from the Perf file descriptor.
            // We don't actually use any of what we read, but it's probably
            // best that we drain the fd anyway.
	    /*
            struct read_format fd_data;
            if (pfds[0].revents & POLLIN) {
                if (read(perf_fd, &fd_data, sizeof(fd_data)) == -1) {
		  printf("Can't read fd\n");
                    perf_pt_set_err(err, perf_pt_cerror_errno, errno);
                    ret = false;
                    break;
                }
            }
	    */

            if (!handle_sample(aux, mmap_hdr, trace, data_tmp, err)) {
                ret = false;
		printf("Can't read sample\n");
                break;
            }

            if (pfds[1].revents & POLLHUP) {
		printf("SIGHUP - 1\n");
                break;
            }
        }

        // The traced thread exited.
        if (pfds[0].revents & POLLHUP) {
	    printf("SIGHUP - 1\n");
            break;
        }
	
	if (atomic_load((_Atomic int*)&gather_thread_state) == GATHER_STATE_STOPPING) {
	    //printf("DONE!!! state = STOPPING\n");
	    break;
	}
    }

done:
    if (data_tmp != NULL) {
        free(data_tmp);
    }

    //printf(">>DONE!!!\n");

    return ret;
}

/*
 * Opens the perf file descriptor and returns it.
 *
 * Returns a file descriptor, or -1 on error.
 */
static int
open_perf(size_t aux_bufsize, struct perf_pt_cerror *err) {
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);

    int ret = -1;

    // Get the perf "type" for Intel PT.
    FILE *pt_type_file = fopen(SYSFS_PT_TYPE, "r");
    if (pt_type_file == NULL) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = -1;
        goto clean;
    }
    char pt_type_str[MAX_PT_TYPE_STR];
    if (fgets(pt_type_str, sizeof(pt_type_str), pt_type_file) == NULL) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = -1;
        goto clean;
    }
    attr.type = atoi(pt_type_str);

    // Exclude the kernel.
    attr.exclude_kernel = 1;

    // Exclude the hyper-visor.
    attr.exclude_hv = 1;

    // Start disabled.
    attr.disabled = 1;

    // No skid.
    attr.precise_ip = 3;

    // Notify for every sample.
    attr.watermark = 1;
    attr.wakeup_watermark = 1;

    // Generate a PERF_RECORD_AUX sample when the AUX buffer is almost full.
    //attr.aux_watermark = (size_t) ((double) aux_bufsize * getpagesize()) * AUX_BUF_WAKE_RATIO;
    attr.aux_watermark = (aux_bufsize * getpagesize()) >> 3;

    attr.config = 0x8c602;

    //printf("AUX Watermark is set at %d of %d\n", attr.aux_watermark, aux_bufsize * getpagesize());

    // Acquire file descriptor through which to talk to Intel PT. This syscall
    // could return EBUSY, meaning another process or thread has locked the
    // Perf device.
    struct timespec wait_time = {0, OPEN_PERF_WAIT_NSECS};
    pid_t target_tid = syscall(__NR_gettid);
    for (int tries = MAX_OPEN_PERF_TRIES; tries > 0; tries--) {
        ret = syscall(SYS_perf_event_open, &attr, target_tid, -1, -1, 0);
        if ((ret == -1) && (errno == EBUSY)) {
            nanosleep(&wait_time, NULL); // Doesn't matter if this is interrupted.
        } else {
            break;
        }
    }

    if (ret == -1) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
    }

clean:
    if ((pt_type_file != NULL) && (fclose(pt_type_file) == -1)) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = -1;
    }

    return ret;
}

/*
 * Set up Intel PT buffers and start a poll() loop for reading out the trace.
 *
 * Returns true on success and false otherwise.
 */
static void *
tracer_thread(void *arg)
{
    while (true) {

	//printf("waiting context\n");
	struct tracer_thread_args* ctx = NULL;
	while (ctx == NULL) {
	    ctx = atomic_load((_Atomic struct tracer_thread_args**)&context_ptr);
	}
	//printf("Got context\n");
	error_flag = false;

	// Copy arguments for the poll loop, as when we resume the parent thread,
	// `thr_args', which is on the parent thread's stack, will become unusable.
	int perf_fd = ctx->perf_fd;
	struct perf_pt_trace *trace = ctx->trace;
	void *aux_buf = ctx->aux_buf;
	struct perf_event_mmap_page *base_header = ctx->base_header;
	struct perf_pt_cerror *err = ctx->err;

	//context_ptr = NULL;
	atomic_store((_Atomic struct tracer_thread_args**)&context_ptr, NULL);
	atomic_store((_Atomic int*)&gather_thread_state, GATHER_STATE_WORKING);

	// Start reading out of the AUX buffer.
	if (!poll_loop(perf_fd, base_header, aux_buf, trace, err)) {
	    error_flag = true;
	    printf("Error flag from thread\n");
	}
	//printf("set unknown state\n");
	atomic_store((_Atomic int*)&gather_thread_state, GATHER_STATE_UNKNOWN);

    }

exit:
    return (void *) NULL;
}

/*
 * --------------------------------------
 * Functions exposed to the outside world
 * --------------------------------------
 */

/*
 * Initialise a tracer context.
 */
struct tracer_ctx *
perf_pt_init_tracer(struct perf_pt_config *tr_conf, struct perf_pt_cerror *err)
{
    struct tracer_ctx *tr_ctx = NULL;
    bool failing = false;

    // Allocate and initialise tracer context.
    tr_ctx = malloc(sizeof(*tr_ctx));
    if (tr_ctx == NULL) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        failing = true;
        goto clean;
    }

    // Set default values.
    memset(tr_ctx, 0, sizeof(*tr_ctx));
    tr_ctx->perf_fd = -1;

    // Obtain a file descriptor through which to speak to perf.
    tr_ctx->perf_fd = open_perf(tr_conf->aux_bufsize, err);
    if (tr_ctx->perf_fd == -1) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        failing = true;
        goto clean;
    }

    // Allocate mmap(2) buffers for speaking to perf.
    //
    // We mmap(2) two separate regions from the perf file descriptor into our
    // address space:
    //
    // 1) The base buffer (tr_ctx->base_buf), which looks like this:
    //
    // -----------------------------------
    // | header  |       data buffer     |
    // -----------------------------------
    //           ^ header->data_offset
    //
    // 2) The AUX buffer (tr_ctx->aux_buf), which is a simple array of bytes.
    //
    // The AUX buffer is where the kernel exposes control flow packets, whereas
    // the data buffer is used for all other kinds of packet.

    // Allocate the base buffer.
    //
    // Data buffer is preceded by one management page (the header), hence `1 +
    // data_bufsize'.
    int page_size = getpagesize();
    tr_ctx->base_bufsize = (1 + tr_conf->data_bufsize) * page_size;
    tr_ctx->base_buf = mmap(NULL, tr_ctx->base_bufsize, PROT_WRITE, MAP_SHARED, tr_ctx->perf_fd, 0);
    if (tr_ctx->base_buf == MAP_FAILED) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        failing = true;
        goto clean;
    }

    // Populate the header part of the base buffer.
    struct perf_event_mmap_page *base_header = tr_ctx->base_buf;
    base_header->aux_offset = base_header->data_offset + base_header->data_size;
    base_header->aux_size = tr_ctx->aux_bufsize = \
                            tr_conf->aux_bufsize * page_size;

    // Allocate the AUX buffer.
    //
    // Mapped R/W so as to have a saturating ring buffer.
    tr_ctx->aux_buf = mmap(NULL, base_header->aux_size, PROT_READ | PROT_WRITE,
        MAP_SHARED, tr_ctx->perf_fd, base_header->aux_offset);
    if (tr_ctx->aux_buf == MAP_FAILED) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        failing = true;
        goto clean;
    }

clean:
    if (failing && (tr_ctx != NULL)) {
        perf_pt_free_tracer(tr_ctx, err);
        return NULL;
    }
    return tr_ctx;
}

/*
 * Turn on Intel PT.
 *
 * `trace_bufsize` is the starting capacity of the trace buffer.
 *
 * The trace is written into `*trace_buf` which may be realloc(3)d. The trace
 * length is written into `*trace_len`.
 *
 * Returns true on success or false otherwise.
 */
bool
perf_pt_start_tracer(struct tracer_ctx *tr_ctx, struct perf_pt_trace *trace, struct perf_pt_cerror *err)
{
    int ret = true;

    // The tracer context contains an error struct for tracking any errors
    // coming from inside the thread. We initialise it to "no errors".
    tr_ctx->tracer_thread_err.kind = perf_pt_cerror_unused;
    tr_ctx->tracer_thread_err.code = 0;

    // Build the arguments struct for the tracer thread.
    struct tracer_thread_args thr_args = {
        tr_ctx->perf_fd,
        trace,
        tr_ctx->aux_buf,
        tr_ctx->base_buf, // The header is the first region in the base buf.
        &tr_ctx->tracer_thread_err,
    };

    // Spawn a thread to deal with copying out of the 
    // PT AUX buffer if it's the first run of this method
    
    if (!atomic_load_explicit((_Atomic bool*)&is_thread_started, memory_order_acquire)) {
	int rc = pthread_create(&tr_ctx->tracer_thread, NULL, tracer_thread, NULL);
	if (rc) {
	    perf_pt_set_err(err, perf_pt_cerror_errno, errno);
	    ret = false;
	    goto clean;
	} else {
	    //is_thread_started = true;
	    atomic_store(&is_thread_started, true);
	}
    }
    
    context_ptr = &thr_args;

    while (atomic_load((_Atomic int*)&gather_thread_state) != GATHER_STATE_WORKING) {}

    // Turn on tracing hardware.
    if (ioctl(tr_ctx->perf_fd, PERF_EVENT_IOC_ENABLE, 0) < 0) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = false;
        goto clean;
    }

    //wait_next_recorded_bunch();
    
clean:
    return ret;
}

/*
 * Turn off the tracer.
 *
 * Arguments:
 *   tr_ctx: The tracer context returned by perf_pt_start_tracer.
 *
 * Returns true on success or false otherwise.
 */
bool
perf_pt_stop_tracer(struct tracer_ctx *tr_ctx, struct perf_pt_cerror *err)
{
    int ret = true;

    wait_next_recorded_bunch();

/*
    {
      int rec = recorded();
      printf("Recorded: %d\n", rec);
      while (rec == recorded()) {}
      printf("Recorded done: %d\n", recorded());
    }
*/


    // Turn off tracer hardware.
    if (ioctl(tr_ctx->perf_fd, PERF_EVENT_IOC_DISABLE, 0) < 0) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = false;
    }

    atomic_store((_Atomic int*)&gather_thread_state, GATHER_STATE_STOPPING);

    //printf("1111\n");
    //printf("state = %d\n", gather_thread_state);
    while (atomic_load((_Atomic int*)&gather_thread_state) != GATHER_STATE_UNKNOWN) {}

    //printf("2222\n");

    return ret;
}

/*
 * Clean up and free a tracer_ctx and its contents.
 *
 * Returns true on success or false otherwise.
 */
bool
perf_pt_free_tracer(struct tracer_ctx *tr_ctx, struct perf_pt_cerror *err) {
    int ret = true;

    if ((tr_ctx->aux_buf) &&
        (munmap(tr_ctx->aux_buf, tr_ctx->aux_bufsize) == -1)) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = false;
    }
    if ((tr_ctx->base_buf) &&
        (munmap(tr_ctx->base_buf, tr_ctx->base_bufsize) == -1)) {
        perf_pt_set_err(err, perf_pt_cerror_errno, errno);
        ret = false;
    }

    if (tr_ctx->perf_fd >= 0) {
        close(tr_ctx->perf_fd);
        tr_ctx->perf_fd = -1;
    }
    if (tr_ctx != NULL) {
        free(tr_ctx);
    }
    return ret;
}
