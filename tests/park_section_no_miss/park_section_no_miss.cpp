#include <threadpark.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <thread>

static constexpr int NUM_ITERATIONS = 15;         // total iterations; each has a scenario
static constexpr auto MAX_BLOCK_MS  = 5000;       // 5s => generous for slow/VM systems

// Shared data for consumer/producer synchronization
static std::mutex              g_mutex;
static std::condition_variable g_cv;

// Let consumer tell producer "I just set the park bit"
static bool g_consumerReady = false;

// Let producer tell consumer "I have woken you (or tried to wake you)"
static bool g_wakeDone = false;

// We’ll store test failures if we detect a lost wake
static std::atomic<bool> g_testFailed{false};

int main()
{
    // 1) Create our thread parking handle
    tpark_handle_t* handle = tparkCreateHandle();

    // 2) Producer: wakes the consumer under different timing scenarios
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITERATIONS && !g_testFailed.load(); i++)
        {
            // Acquire lock and wait until consumer sets the park bit
            std::unique_lock<std::mutex> lk(g_mutex);
            g_cv.wait(lk, [] { return g_consumerReady || g_testFailed.load(); });
            if (g_testFailed.load()) break;

            // Decide how long to wait before calling wake
            // scenario 0 = immediate
            // scenario 1 = 100 ms
            // scenario 2 = 500 ms
            int scenario = i % 3;
            auto sleepMs = (scenario == 0) ? 0
                          : (scenario == 1) ? 100
                                            : 500;

            // Sleep some time to let the consumer possibly call parkWait
            lk.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            lk.lock();

            // Now call wake
            tparkWake(handle);

            // Indicate to the consumer "we're done calling wake"
            g_wakeDone = true;
            g_consumerReady = false;
            g_cv.notify_one();
        }
    });

    // 3) Consumer: sets the park bit, signals producer, then calls parkWait
    std::thread consumer([&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++)
        {
            // 3.1) "Begin Park": set bit=1
            {
                std::unique_lock<std::mutex> lk(g_mutex);
                tparkBeginPark(handle);

                // Indicate "ready" so producer knows to do a wake
                g_consumerReady = true;
                g_wakeDone      = false;
                g_cv.notify_one();

                // Wait until producer has definitely done a wake attempt
                g_cv.wait(lk, [] { return g_wakeDone || g_testFailed.load(); });
                if (g_testFailed.load()) return;
            } // unlock the mutex

            // 3.2) Now measure how long we actually block in parkWait
            // Because the producer may have woken us "too early," we might skip real blocking
            auto t1 = std::chrono::steady_clock::now();
            tparkWait(handle, /*unlocked=*/true);
            auto t2 = std::chrono::steady_clock::now();

            auto msBlocked = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
            if (msBlocked > MAX_BLOCK_MS)
            {
                std::cerr << "[Iteration " << i << "] BLOCKED too long ("
                          << msBlocked << " ms). Potential lost wake or deadlock.\n";
                g_testFailed.store(true);
                return;
            }

            // Optionally reset the bit (harmless if tparkWake already set it to 0)
            tparkEndPark(handle);
        }
    });

    producer.join();
    consumer.join();

    tparkDestroyHandle(handle);

    if (g_testFailed.load()) {
        std::cerr << "TEST FAILED: Lost wake or indefinite blocking scenario encountered.\n";
        return EXIT_FAILURE;
    }
    std::cout << "TEST PASSED: No lost wakes in " << NUM_ITERATIONS
            << " iterations with varying timing.\n";
    return EXIT_SUCCESS;
}
