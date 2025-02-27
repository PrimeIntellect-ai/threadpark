#include "threadpark.h"

#include <iostream>
#include <atomic>
#include <cerrno>
#include <cstdint>

#include <sys/futex.h>     // for FUTEX_WAIT, FUTEX_WAKE
#include <sys/time.h>      // for struct timespec if needed
#include <unistd.h>

struct tpark_handle_t {
    // 0 => not parked
    // 1 => parked
    std::atomic<uint32_t> state{0};
};

tpark_handle_t *tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkPark(tpark_handle_t *handle) {
    // Indicate we want to park
    handle->state.store(1, std::memory_order_release);

    while (true) {
        // Check if state is still 1; if not, we can stop
        uint32_t val = handle->state.load(std::memory_order_acquire);
        if (val != 1) {
            // The state changed (someone did Wake), so we're no longer parked
            return;
        }

        /*
         * futex(volatile uint32_t *uaddr, int op, int val,
         *       const struct timespec *timeout, volatile uint32_t *uaddr2);
         *
         * If *uaddr is still == 'val', the caller blocks. If something else changed
         * it, futex() returns with EAGAIN (errno).
         */
        int rc = futex(&handle->state,
                       FUTEX_WAIT,
                       1,          // val: we wait if *uaddr == 1
                       nullptr,    // no timeout
                       nullptr);   // uaddr2 not used for FUTEX_WAIT
        if (rc == 0) {
            // We were woken up by a matching FUTEX_WAKE call
            return;
        } else {
            // rc == -1 => check errno
            if (errno == EAGAIN) {
                // *uaddr != val at call time => no need to block, so just return
                return;
            } else if (errno == EINTR) {
                // Interrupted by a signal => loop again
                continue;
            } else {
                // Some other error
                std::cerr << "Unexpected error in tparkPark: " << std::strerror(errno) << std::endl;
                std::abort();
            }
        }
    }
}

void tparkWake(tpark_handle_t *handle) {
    // Set state to 0 => unpark
    handle->state.store(0, std::memory_order_release);

    /*
     * FUTEX_WAKE unblocks up to 'val' threads sleeping on &handle->state.
     * In typical usage, 1 is enough.  If you expect multiple waiters, pass a larger
     * number (e.g. INT_MAX) to wake them all.
     */
    futex(&handle->state,
          FUTEX_WAKE,
          1,          // wake up to 1 waiter
          nullptr,
          nullptr);
}

void tparkDestroyHandle(const tpark_handle_t *handle) {
    delete handle;
}
