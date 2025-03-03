#include "threadpark.h"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <iostream>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

struct tpark_handle_t {
    /// The atomic state:
    ///  - 1 => "thread is parked / should block"
    ///  - 0 => "thread is not parked / free to proceed"
    std::atomic<int> state{0};
};

static int futex_wait(std::atomic<int> *addr, int expected) {
    return syscall(SYS_futex, reinterpret_cast<int *>(addr), FUTEX_WAIT, expected,
                   nullptr, // no timeout
                   nullptr, // no addr2
                   0);
}

static int futex_wake(std::atomic<int> *addr, int num_wakes) {
    return syscall(SYS_futex, reinterpret_cast<int *>(addr), FUTEX_WAKE, num_wakes,
                   nullptr, // no timeout
                   nullptr, // no addr2
                   0);
}

tpark_handle_t *tparkCreateHandle() { return new tpark_handle_t(); }

void tparkWait(tpark_handle_t *handle, const bool unlocked) {
    if (!unlocked) {
        // Indicate we want to park
        handle->state.store(1, std::memory_order_seq_cst);
    }
    while (true) {
        // Double-check the state before actually blocking
        if (handle->state.load(std::memory_order_seq_cst) != 1) {
            // If it's not 1 anymore, we're done (another thread likely called wake).
            return;
        }

        // Otherwise, do a futex wait for the value 1
        if (const int rc = futex_wait(&handle->state, 1); rc == 0) {
            // We were woken up, so presumably the state is now 0. We return.
            return;
        } else {
            // rc < 0 => check errno
            if (errno == EAGAIN) {
                // check for spurious wakeups
                if (handle->state.load(std::memory_order_seq_cst) != 1) {
                    return;
                }
            } else if (errno == EINTR) {
                // Interrupted by a signal; retry
                continue;
            } else {
                std::cerr << "Unexpected error in tparkPark: " << std::strerror(errno) << std::endl;
                std::abort();
            }
        }
    }
}

void tparkBeginPark(tpark_handle_t *handle) {
    handle->state.exchange(1, std::memory_order_seq_cst);
}

void tparkEndPark(tpark_handle_t *handle) {
    handle->state.store(0, std::memory_order_seq_cst);
}

void tparkWake(tpark_handle_t *handle) {
    if (handle->state.load(std::memory_order_seq_cst) == 0) {
        // No need to wake up, the thread is not parked
        return;
    }

    // Set the state to 0 => unpark
    handle->state.store(0, std::memory_order_seq_cst);

    // Wake one thread waiting on the futex
    futex_wake(&handle->state, 1);
}

void tparkDestroyHandle(const tpark_handle_t *handle) { delete handle; }
