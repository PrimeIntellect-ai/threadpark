#include "threadpark.h"

#include <mutex>
#include <iostream>
#include <cstring>

#include "xnu_ulock_internal.h"

struct tpark_handle_t {
    /// The atomic state for parking:
    ///  - 1 indicates "parked" (thread should block until changed),
    ///  - 0 indicates "not parked" (thread can proceed).
    std::atomic<uint32_t> state{0};
};

tpark_handle_t *tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkWait(tpark_handle_t *handle, const bool unlocked) {
    if (!unlocked) {
        // Set the state to 1 to indicate we want to park
        handle->state.store(1, std::memory_order_release);
    }
    // Repeatedly call __ulock_wait in case of spurious wakes or signals.
    // __ulock_wait(UL_COMPARE_AND_WAIT, &addr, expected_val, timeout)
    // blocks until 'state' != expected_val (or an error/spurious wake occurs).
    while (true) {
        const int rc = __ulock_wait(UL_COMPARE_AND_WAIT,
                                    &handle->state,
                                    1, // compare value
                                    0 // no timeout (wait indefinitely)
        );
        if (rc == 0) {
            // check for spurious wakeups
            if (handle->state.load(std::memory_order_acquire) != 1) {
                return;
            }
        }
        if (rc < 0) {
            // Check errno for possible causes
            if (errno == EINTR) {
                // Interrupted by a signal; retry
                continue;
            } else if (errno == EBUSY) {
                // The state wasn't 1 by the time we called __ulock_wait,
                // so we aren't actually blocked. Just return.
                return;
            } else {
                std::cerr << "Unexpected error in tparkPark: " << std::strerror(errno) << std::endl;
                std::abort();
            }
        }
    }
}

void tparkBeginPark(tpark_handle_t *handle) { handle->state.store(1, std::memory_order_release); }

void tparkEndPark(tpark_handle_t *handle) { handle->state.store(0, std::memory_order_release); }

void tparkWake(tpark_handle_t *handle) {
    if (handle->state.load(std::memory_order_acquire) == 0) {
        // No need to wake up, the thread is not parked
        return;
    }
    // Set the state to 0 to indicate "not parked"
    handle->state.store(0, std::memory_order_release);

    // Wake any threads waiting for the value '1'
    // (i.e., threads that called __ulock_wait(..., 1, ...)).
    __ulock_wake(UL_COMPARE_AND_WAIT, &handle->state, 1);
}

void tparkDestroyHandle(const tpark_handle_t *handle) {
    delete handle;
}
