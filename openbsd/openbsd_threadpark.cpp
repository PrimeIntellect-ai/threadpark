#include "threadpark.h"
#include <atomic>
#include <cerrno>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/**
 * _thrsleep(2) and _thrwakeup(2) are declared in sys/syscall.h or sys/thr.h,
 * but in many OpenBSD versions they are only documented (sparingly) in thr(2).
 * We can declare them manually here if not included by default headers.
 */
extern "C" {
    int __thrsleep(const volatile void *identifier,
                  clockid_t clock_id,
                  const struct timespec *timeout,
                  int flags);
    int __thrwakeup(const volatile void *identifier,
                   int n);
}

struct tpark_handle_t {
    // Same idea:
    //   1 => "parked" (thread should block)
    //   0 => "not parked" (thread can proceed)
    std::atomic<int> state{0};
};

tpark_handle_t* tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkPark(tpark_handle_t* handle) {
    // Indicate we want to park
    handle->state.store(1, std::memory_order_release);

    while (true) {
        // Check if we still need to park
        int val = handle->state.load(std::memory_order_acquire);
        if (val != 1) {
            // The state changed (e.g., someone called tparkWake), so stop parking.
            return;
        }

        // _thrsleep():
        //   Blocks the calling thread on the given 'identifier' pointer.
        //   It returns 0 if woken by _thrwakeup, or -1 if interrupted or error.
        int rc = __thrsleep(/* identifier */ &handle->state,
                           /* clock_id   */ CLOCK_REALTIME,
                           /* timeout    */ nullptr,
                           /* flags      */ 0);

        if (rc == 0) {
            // We were woken up by _thrwakeup
            // or the state changed after we started sleeping.
            return;
        } else {
            // rc < 0 => check errno
            if (errno == EINTR) {
                // Interrupted by a signal; retry
                continue;
            }
            // If there's another error (e.g. EFAULT, EBUSY),
            // we just break out or return directly.
            return;
        }
    }
}

void tparkWake(tpark_handle_t* handle) {
    // Indicate we are unparking the thread
    handle->state.store(0, std::memory_order_release);

    // Wake up to 1 thread sleeping on &handle->state
    __thrwakeup(&handle->state, 1);
}

void tparkDestroyHandle(const tpark_handle_t* handle) {
    delete handle;
}
