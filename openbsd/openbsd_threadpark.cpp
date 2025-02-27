#include "threadpark.h"

#include <atomic>
#include <cerrno>
#include <iostream>

#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*
 * Declarations for the private __thrsleep and __thrwakeup APIs.
 * In some OpenBSD versions they may not be exported, or may require
 * special flags. See also thr(2).
 */
extern "C" {
    int __thrsleep(const volatile void *chan,
                   clockid_t clock_id,
                   const struct timespec *timeout,
                   int flags);

    int __thrwakeup(const volatile void *chan,
                    int n);
}

struct alignas(4) tpark_handle_t {
    // 1 => "parked (should block)"
    // 0 => "not parked"
    // We force alignment to 4 bytes (alignas(4)) in case the system
    // demands it for __thrsleep to work properly.
    std::atomic<int> state{0};
};

tpark_handle_t* tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkPark(tpark_handle_t* handle) {
    // Indicate we want to park
    handle->state.store(1, std::memory_order_release);

    while (true) {
        // Check if still 1 (meaning we really do want to block)
        int val = handle->state.load(std::memory_order_acquire);
        if (val != 1) {
            // Another thread changed it => unparked
            return;
        }

        // __thrsleep: indefinite wait until __thrwakeup or a signal
        //
        // - chan     = &handle->state
        // - clock_id = -1      (means "no timeout")
        // - timeout  = nullptr (since we have no timeout)
        // - flags    = 0
        //
        // Returns 0 if woken up via __thrwakeup,
        // or -1 on error (errno set).
        int rc = __thrsleep(&handle->state, clockid_t(-1), nullptr, 0);
        if (rc == 0) {
            // Woken by __thrwakeup
            return;
        } else {
            // rc < 0 => check errno
            if (errno == EINTR) {
                // Interrupted by a signal => try again
                continue;
            }
            // EINVAL, EFAULT, EBUSY, etc. => just stop
            std::cerr << "Unexpected error in tparkPark: " << std::strerror(errno) << std::endl;
            std::abort();
        }
    }
}

void tparkWake(tpark_handle_t* handle) {
    // Unpark
    handle->state.store(0, std::memory_order_release);

    // __thrwakeup: wake up to `n` threads sleeping on &handle->state
    // Typically, n=1 is enough if you only expect one waiter
    __thrwakeup(&handle->state, 1);
}

void tparkDestroyHandle(const tpark_handle_t* handle) {
    delete handle;
}
