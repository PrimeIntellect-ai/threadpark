#include "threadpark.h"

#include <iostream>
#include <atomic>
#include <cstdint>
#include <cerrno>

#include <sys/futex.h>
#include <sys/time.h>
#include <unistd.h>

struct tpark_handle_t {
    // 0 => not parked
    // 1 => parked
    std::atomic<uint32_t> state{0};
};

tpark_handle_t* tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkWait(tpark_handle_t *handle, const _Bool unlocked) {
    if (!unlocked) {
        // Indicate we want to park
        handle->state.store(1, std::memory_order_release);
    }

    while (true) {
        // Check if state is still 1
        uint32_t val = handle->state.load(std::memory_order_acquire);
        if (val != 1) {
            // State changed => exit
            return;
        }

        // The futex call wants (volatile uint32_t *) rather than (std::atomic<uint32_t>*).
        volatile uint32_t *addr = reinterpret_cast<volatile uint32_t*>(&handle->state);

        int rc = futex(addr,
                       FUTEX_WAIT,
                       1,           // Wait if *addr == 1
                       nullptr,     // no timeout
                       nullptr);    // not used for FUTEX_WAIT

        if (rc == 0) {
            // Woken by FUTEX_WAKE
            return;
        } else {
            // rc == -1 => check errno
            if (errno == EAGAIN) {
                // check for spurious wakeups
                if (handle->state.load(std::memory_order_acquire) != 1) {
                    return;
                }
            } else if (errno == EINTR) {
                // Interrupted by a signal => retry
                continue;
            } else {
                std::cerr << "Unexpected error in tparkPark: " << std::strerror(errno) << std::endl;
                std::abort();
            }
        }
    }
}

void tparkBeginPark(tpark_handle_t *handle) { handle->state.store(1, std::memory_order_release); }

void tparkEndPark(tpark_handle_t *handle) { handle->state.store(0, std::memory_order_release); }

void tparkWake(tpark_handle_t* handle) {
    if (handle->state.load(std::memory_order_acquire) == 0) {
        // No need to wake up, the thread is not parked
        return;
    }

    // Unpark
    handle->state.store(0, std::memory_order_release);

    // The futex call wants (volatile uint32_t *).
    volatile uint32_t *addr = reinterpret_cast<volatile uint32_t*>(&handle->state);

    // Wake up to 1 waiting thread
    futex(addr,
          FUTEX_WAKE,
          1,          // number of waiters to wake
          nullptr,
          nullptr);
}

void tparkDestroyHandle(const tpark_handle_t* handle) {
    delete handle;
}
