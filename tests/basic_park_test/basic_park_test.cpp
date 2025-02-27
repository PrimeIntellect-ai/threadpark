#include <iostream>
#include <thread>
#include <threadpark.h>
#include <chrono>

int main() {
    tpark_handle_t *handle = tparkCreateHandle();

    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tparkWake(handle);
    });

    const auto start = std::chrono::high_resolution_clock::now();
    tparkPark(handle);
    const auto end = std::chrono::high_resolution_clock::now();

    if (const std::chrono::duration<double> elapsed = end - start; elapsed.count() < 1) {
        std::cerr << "Thread woke up too early" << std::endl;
        std::abort();
    }

    tparkDestroyHandle(handle);
    waker.join();

    std::cout << "Thread parked and woken up" << std::endl;
    return 0;
}
