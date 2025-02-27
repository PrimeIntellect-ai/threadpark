#include "threadpark.h"
#include <mutex>
#include <condition_variable>

/// The handle structure keeps track of a condition variable, a mutex, and
/// a flag indicating whether the thread is currently parked or not.
struct tpark_handle_t {
    std::mutex m;
    std::condition_variable cv;
    bool parked = false;
};

tpark_handle_t *tparkCreateHandle() {
    return new tpark_handle_t();
}

void tparkPark(tpark_handle_t *handle) {
    if (!handle) return;
    std::unique_lock lock(handle->m);

    handle->parked = true;
    handle->cv.wait(lock, [handle]() {
        return !handle->parked;
    });
}

void tparkWake(tpark_handle_t *handle) {
    if (!handle) return;
    std::lock_guard lock(handle->m);

    handle->parked = false;
    handle->cv.notify_one();
}

void tparkDestroyHandle(const tpark_handle_t *handle) {
    delete handle;
}
