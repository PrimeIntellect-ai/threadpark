#include "threadpark.h"

#include <iostream>
#include <atomic>
#include <cerrno>

#include <sys/umtx.h>
#include <unistd.h>

struct tpark_handle_t {
    /// The atomic state:
    ///  - 1 => thread is parked / should block
    ///  - 0 => thread is not parked / can proceed
    std::atomic<int> state{0};
};

/**
 * Thin wrappers around the _umtx_op() system call for clarity.
 */
static int umtx_wait(std::atomic<int>* addr, int expected)
{
    // _umtx_op(void *obj, int op, u_long val, void *uaddr, void *uaddr2)
    // For UMTX_OP_WAIT_UINT:
    //   obj   = address to wait on
    //   op    = UMTX_OP_WAIT_UINT
    //   val   = expected value
    //   uaddr = timeout (optional, pass nullptr for infinite)
    //   uaddr2= unused
    return _umtx_op(reinterpret_cast<void*>(addr),
                    UMTX_OP_WAIT_UINT,
                    static_cast<unsigned long>(expected),
                    nullptr,
                    nullptr);
}

static int umtx_wake(std::atomic<int>* addr, int count)
{
    // For UMTX_OP_WAKE:
    //   obj   = address to wake
    //   op    = UMTX_OP_WAKE
    //   val   = number of waiters to wake
    //   uaddr/uaddr2 = unused
    return _umtx_op(reinterpret_cast<void*>(addr),
                    UMTX_OP_WAKE,
                    static_cast<unsigned long>(count),
                    nullptr,
                    nullptr);
}

tpark_handle_t* tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkWait(tpark_handle_t *handle, const bool unlocked) {
    if (!unlocked) {
        // Indicate we want to park
        handle->state.store(1, std::memory_order_release);
    }
    while (true) {
        int val = handle->state.load(std::memory_order_acquire);
        if (val != 1) {
            // If it's not 1 anymore, we're done
            return;
        }

        // Block if state is still 1
        int rc = umtx_wait(&handle->state, 1);
        if (rc == 0) {
            // check for spurious wakeups
            if (handle->state.load(std::memory_order_acquire) != 1) {
                return;
            }
        } else {
            // Error or spurious wake up => check errno
            if (errno == EINTR) {
                // Interrupted by a signal, retry
                continue;
            } else if (errno == EWOULDBLOCK) {
                // The state changed before we called WAIT,
                // or changed while we were about to block. Just exit.
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
    // Set state to 0 => unpark
    handle->state.store(0, std::memory_order_release);

    // Wake up to 1 thread waiting on address
    // (could be >1 if multiple waiters, but typically 1 is enough)
    umtx_wake(&handle->state, 1);
}

void tparkDestroyHandle(const tpark_handle_t* handle) {
    delete handle;
}
