#include "threadpark.h"
#include <windows.h>
#include <atomic>

struct tpark_handle_t {
    // The atomic state for parking:
    //  - 1 indicates "parked"
    //  - 0 indicates "not parked"
    std::atomic<ULONG> state{0};
};

tpark_handle_t *tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkPark(tpark_handle_t *handle) {
    // Set the state to 1 to indicate we want to park
    handle->state.store(1, std::memory_order_release);

    // Wait until 'state' changes from 1 to something else.
    // If the call fails or returns (e.g. spurious wake), we re-check the state.
    ULONG expected = 1;
    while (handle->state.load(std::memory_order_acquire) == expected) {
        BOOL success = WaitOnAddress(
            /* Address        = */ &handle->state,
            /* CompareAddress = */ &expected,
            /* AddressSize    = */ sizeof(expected),
            /* dwMilliseconds = */ INFINITE // wait forever
        );
        if (!success) {
            // WaitOnAddress can fail due to various reasons (e.g. spurious wake).
            // Commonly you’d check GetLastError() for debugging or handle signals, etc.
            // We’ll just loop again unless the state changed.
            DWORD error = GetLastError();
            if (error == ERROR_TIMEOUT) {
                // If you provided a non-INFINITE timeout, you might handle a timeout here.
                // For INFINITE, this shouldn't normally happen unless forcibly canceled.
            }
        }
    }
}

void tparkWake(tpark_handle_t *handle) {
    // Set the state to 0, signaling that any parked thread should wake.
    handle->state.store(0, std::memory_order_release);

    // Wake exactly one waiter waiting on &handle->state with the compare-value=1
    WakeByAddressSingle(&handle->state);
}

void tparkDestroyHandle(const tpark_handle_t *handle) {
    delete handle;
}
