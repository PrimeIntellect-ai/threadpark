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

void tparkWait(tpark_handle_t *handle, const bool unlocked) {
    if (!unlocked) {
        // Set the state to 1 to indicate we want to park
        handle->state.store(1, std::memory_order_seq_cst);
    }


    while (true) {
        // Double-check the state before actually blocking
        if (handle->state.load(std::memory_order_seq_cst) != 1) {
            // If it's not 1 anymore, we're done (another thread likely called wake).
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
                if (handle->state.load(std::memory_order_seq_cst) != 1) {
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

void tparkBeginPark(tpark_handle_t *handle) { handle->state.store(1, std::memory_order_seq_cst); }

void tparkEndPark(tpark_handle_t *handle) { handle->state.store(0, std::memory_order_seq_cst); }

void tparkWake(tpark_handle_t* handle) {
    if (handle->state.load(std::memory_order_acquire) == 0) {
        // No need to wake up, the thread is not parked
        return;
    }

    // Unpark
    handle->state.store(0, std::memory_order_seq_cst);

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
