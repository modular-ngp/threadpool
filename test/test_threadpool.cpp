import threadpool;

#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <stdexcept>

using namespace std::chrono_literals;

static void test_basic_single_task() {
    ThreadPool pool(2);

    auto fut = pool.enqueue([] { return 42; });
    int value = fut.get();
    assert(value == 42);

    pool.wait_idle();
}

static void test_multiple_tasks_and_wait_idle() {
    ThreadPool pool(4);

    constexpr int N = 1000;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(N);

    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.enqueue([&counter] {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    pool.wait_idle();

    assert(counter.load(std::memory_order_relaxed) == N);
}

static void test_exception_propagation() {
    ThreadPool pool(2);

    auto fut = pool.enqueue([]() -> int {
        throw std::runtime_error("test error");
    });

    bool caught = false;
    try {
        (void)fut.get();
    } catch (const std::runtime_error& e) {
        caught = true;
        assert(std::string(e.what()) == "test error");
    } catch (...) {
        assert(false && "unexpected exception type");
    }

    assert(caught);
    pool.wait_idle();
}

static void test_wait_idle_with_no_tasks() {
    ThreadPool pool(2);
    pool.wait_idle();
}

static void test_destruction_waits_for_tasks() {
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};

    {
        ThreadPool pool(2);
        pool.enqueue([&] {
            started.store(true, std::memory_order_release);
            std::this_thread::sleep_for(100ms);
            finished.store(true, std::memory_order_release);
        });
        // 离开作用域时 ~ThreadPool() 应该等任务结束
    }

    assert(started.load(std::memory_order_acquire));
    assert(finished.load(std::memory_order_acquire));
}

static void test_parallel_behavior_smoke() {
    constexpr int N = 8;
    constexpr auto sleep_time = 100ms;

    auto serial_start = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
        std::this_thread::sleep_for(sleep_time);
    }
    auto serial_end = std::chrono::steady_clock::now();
    auto serial_ms = std::chrono::duration_cast<std::chrono::milliseconds>(serial_end - serial_start).count();

    ThreadPool pool(4);

    auto parallel_start = std::chrono::steady_clock::now();
    std::vector<std::future<void>> futures;
    futures.reserve(N);
    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.enqueue([&] {
            std::this_thread::sleep_for(sleep_time);
        }));
    }
    for (auto& f : futures) {
        f.get();
    }
    pool.wait_idle();
    auto parallel_end = std::chrono::steady_clock::now();
    auto parallel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(parallel_end - parallel_start).count();

    std::cout << "[timing] serial   = " << serial_ms   << " ms\n";
    std::cout << "[timing] parallel = " << parallel_ms << " ms\n";

    // 不做严格断言，只要求比严格串行明显快一点（留出环境噪声）
    assert(parallel_ms < serial_ms);
}

int main() {
    std::cout << "Running ThreadPool tests...\n";

    test_basic_single_task();
    std::cout << "  [OK] basic_single_task\n";

    test_multiple_tasks_and_wait_idle();
    std::cout << "  [OK] multiple_tasks_and_wait_idle\n";

    test_exception_propagation();
    std::cout << "  [OK] exception_propagation\n";

    test_wait_idle_with_no_tasks();
    std::cout << "  [OK] wait_idle_with_no_tasks\n";

    test_destruction_waits_for_tasks();
    std::cout << "  [OK] destruction_waits_for_tasks\n";

    test_parallel_behavior_smoke();
    std::cout << "  [OK] parallel_behavior_smoke\n";

    std::cout << "All tests passed.\n";
    return 0;
}
