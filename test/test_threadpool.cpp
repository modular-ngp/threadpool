#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

import threadpool;

namespace {

    using namespace std::chrono_literals;

    //------------------------------------------------------------------------------
    // Helper: measure milliseconds since a time point
    //------------------------------------------------------------------------------
    [[nodiscard]] auto milliseconds_since(const std::chrono::steady_clock::time_point& start) noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    }

    //------------------------------------------------------------------------------
    // TEST 1: Basic task returns correct value
    //------------------------------------------------------------------------------
    void test_basic_single_task() {
        ThreadPool pool{2};

        auto fut = pool.enqueue([] { return 42; });

        [[maybe_unused]] const int value = fut.get();
        assert(value == 42);

        pool.wait_idle();
    }

    //------------------------------------------------------------------------------
    // TEST 2: Many tasks increment atomic counter
    //------------------------------------------------------------------------------
    void test_multiple_tasks_and_wait_idle() {
        ThreadPool pool{4};

        constexpr int kTaskCount = 1000;
        std::atomic<int> counter{0};

        std::vector<std::future<void>> futures;
        futures.reserve(kTaskCount);

        for (int i = 0; i < kTaskCount; ++i) {
            futures.emplace_back(pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
        }

        for (auto& fut : futures) {
            fut.get();
        }

        pool.wait_idle();

        [[maybe_unused]] const int final_count = counter.load(std::memory_order_relaxed);

        assert(final_count == kTaskCount);
    }

    //------------------------------------------------------------------------------
    // TEST 3: Exception propagation from task
    //------------------------------------------------------------------------------
    void test_exception_propagation() {
        ThreadPool pool{2};

        auto fut = pool.enqueue([]() -> int { throw std::runtime_error{"test error"}; });

        bool caught = false;

        try {
            (void) fut.get();
        } catch (const std::runtime_error& ex) {
            caught                                     = true;
            [[maybe_unused]] const std::string message = ex.what();
            assert(message == "test error");
        } catch (...) {
            assert(false && "Unexpected exception type");
        }

        assert(caught);

        pool.wait_idle();
    }

    //------------------------------------------------------------------------------
    // TEST 4: Calling wait_idle with no tasks
    //------------------------------------------------------------------------------
    void test_wait_idle_no_tasks() {
        ThreadPool pool{2};
        pool.wait_idle();
    }

    //------------------------------------------------------------------------------
    // TEST 5: Destructor blocks until tasks complete
    //------------------------------------------------------------------------------
    void test_destruction_waits_for_tasks() {
        std::atomic<bool> started{false};
        std::atomic<bool> finished{false};

        {
            ThreadPool pool{2};
            pool.enqueue([&] {
                started.store(true, std::memory_order_release);
                std::this_thread::sleep_for(100ms);
                finished.store(true, std::memory_order_release);
            });
        } // pool destructor must wait

        assert(started.load(std::memory_order_acquire));
        assert(finished.load(std::memory_order_acquire));
    }

    //------------------------------------------------------------------------------
    // TEST 6: Parallel vs serial timing smoke test
    //------------------------------------------------------------------------------
    void test_parallel_behavior_smoke() {
        constexpr int kTaskCount = 8;
        constexpr auto kSleep    = 100ms;

        // Serial
        const auto serial_start = std::chrono::steady_clock::now();
        for (int i = 0; i < kTaskCount; ++i) {
            std::this_thread::sleep_for(kSleep);
        }
        const auto serial_ms = milliseconds_since(serial_start);

        // Parallel
        ThreadPool pool{4};

        std::vector<std::future<void>> futures;
        futures.reserve(kTaskCount);

        const auto parallel_start = std::chrono::steady_clock::now();
        for (int i = 0; i < kTaskCount; ++i) {
            futures.emplace_back(pool.enqueue([&] { std::this_thread::sleep_for(kSleep); }));
        }

        for (auto& fut : futures) {
            fut.get();
        }
        pool.wait_idle();

        const auto parallel_ms = milliseconds_since(parallel_start);

        std::cout << "[timing] serial   = " << serial_ms << " ms\n";
        std::cout << "[timing] parallel = " << parallel_ms << " ms\n";

        assert(parallel_ms < serial_ms);
    }

} // namespace

//------------------------------------------------------------------------------
// MAIN
//------------------------------------------------------------------------------
int main() {
    std::cout << "Running ThreadPool tests...\n";

    test_basic_single_task();
    std::cout << "  [OK] basic_single_task\n";

    test_multiple_tasks_and_wait_idle();
    std::cout << "  [OK] multiple_tasks_and_wait_idle\n";

    test_exception_propagation();
    std::cout << "  [OK] exception_propagation\n";

    test_wait_idle_no_tasks();
    std::cout << "  [OK] wait_idle_no_tasks\n";

    test_destruction_waits_for_tasks();
    std::cout << "  [OK] destruction_waits_for_tasks\n";

    test_parallel_behavior_smoke();
    std::cout << "  [OK] parallel_behavior_smoke\n";

    std::cout << "All tests passed.\n";
    return 0;
}
