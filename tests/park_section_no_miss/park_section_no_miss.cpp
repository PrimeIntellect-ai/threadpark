#include <threadpark.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

static constexpr int NUM_ITERATIONS = 100;

static constexpr auto MAX_WAIT_MS = 500;

int main() {
    tpark_handle_t *handle = tparkCreateHandle();
    std::atomic stop{false};

    // Producer thread: tries to wake the consumer after a short delay
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITERATIONS && !stop.load(); i++) {
            // Sleep a *tiny* bit so consumer definitely has set "state=1"
            // but maybe hasn't called tparkParkWait yet.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Attempt to wake — if consumer hasn't actually parked yet,
            // we want to verify we do *not* lose this wake.
            tparkWake(handle);
        }
    });

    bool lostWake = false;

    // Consumer: in each iteration, does a "two-phase" park
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // 1) Begin parking (set the park bit to 1)
        tparkBeginPark(handle);

        // 2) Simulate some short pause before actually calling parkWait
        //    The producer may call tparkWake() in this window.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 3) Actually park. But if the wake arrived "too early",
        //    the state is already zero and we won't block.
        //    For safety, let's measure how long we actually block.
        auto t1 = std::chrono::steady_clock::now();
        tparkWait(handle, /*unlocked=*/true);
        auto t2 = std::chrono::steady_clock::now();

        // Check we didn't block too long
        auto msBlocked = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
        if (msBlocked > MAX_WAIT_MS) {
            std::cerr << "[Iteration " << i << "] Thread blocked too long ("
                      << msBlocked << " ms). Potential lost wake-up!\n";
            lostWake = true;
            break;
        }
    }

    // Signal producer to exit if we ended early
    stop.store(true);

    producer.join();
    tparkDestroyHandle(handle);

    if (lostWake) {
        std::cerr << "TEST FAILED: A lost wake-up seems to have occurred.\n";
        return EXIT_FAILURE;
    } else {
        std::cout << "TEST PASSED: No lost wakes in " << NUM_ITERATIONS << " iterations.\n";
    }
    return EXIT_SUCCESS;
}
