export module threadpool;

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

export class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency()) : stop_(false), active_(0) {
        if (thread_count == 0) {
            thread_count = 1;
        }
        workers_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using R            = std::invoke_result_t<F, Args...>;
        auto task          = std::make_shared<std::packaged_task<R()>>(make_task_functor(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace_back([task] { (*task)(); });
        }
        cv_.notify_one();
        return fut;
    }

    void wait_idle() {
        std::unique_lock<std::mutex> lock(mutex_);
        idle_cv_.wait(lock, [this] { return tasks_.empty() && active_ == 0; });
    }

    std::size_t size() const noexcept {
        return workers_.size();
    }

private:
    using Task = std::function<void()>;

    template <class F, class... Args>
    static auto make_task_functor(F&& f, Args&&... args) {
        using Fn  = std::decay_t<F>;
        using Tup = std::tuple<std::decay_t<Args>...>;
        return [fn = Fn(std::forward<F>(f)), tup = Tup(std::forward<Args>(args)...)]() mutable { return std::apply(std::move(fn), std::move(tup)); };
    }

    void worker_loop() {
        for (;;) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
                ++active_;
            }

            task();

            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_;
                if (tasks_.empty() && active_ == 0) {
                    idle_cv_.notify_all();
                }
            }
        }
    }

    std::vector<std::thread> workers_;
    std::deque<Task> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idle_cv_;
    bool stop_;
    std::size_t active_;
};
