#include <iostream>
#include <thread>
#include <threadpark.h>

int main() {
    tpark_handle_t *handle = tparkCreateHandle();

    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tparkWake(handle);
    });

    tparkPark(handle);
    tparkDestroyHandle(handle);
    waker.join();

    std::cout << "Thread parked and woken up" << std::endl;
    return 0;
}