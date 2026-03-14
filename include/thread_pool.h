#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <thread>
#include <functional>
#include <atomic>

namespace ThreadPool {

// Get number of hardware threads
inline int get_num_threads() {
    int n = std::thread::hardware_concurrency();
    return (n > 0) ? n : 4;  // Default to 4 if detection fails
}

// Parallel for loop: executes func(i) for i in [start, end)
template<typename Func>
void parallel_for(int start, int end, Func func, int num_threads = 0) {
    if (num_threads <= 0) {
        num_threads = get_num_threads();
    }

    // If work is small, just run serially
    int work_size = end - start;
    if (work_size <= num_threads || num_threads == 1) {
        for (int i = start; i < end; i++) {
            func(i);
        }
        return;
    }

    // Parallel execution
    std::vector<std::thread> threads;
    std::atomic<int> next_index(start);

    auto worker = [&]() {
        while (true) {
            int i = next_index.fetch_add(1);
            if (i >= end) break;
            func(i);
        }
    };

    // Launch worker threads
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
}

// Parallel for with static scheduling (for balanced workloads)
template<typename Func>
void parallel_for_static(int start, int end, Func func, int num_threads = 0) {
    if (num_threads <= 0) {
        num_threads = get_num_threads();
    }

    int work_size = end - start;
    if (work_size <= num_threads || num_threads == 1) {
        for (int i = start; i < end; i++) {
            func(i);
        }
        return;
    }

    std::vector<std::thread> threads;
    int chunk_size = (work_size + num_threads - 1) / num_threads;

    for (int t = 0; t < num_threads; t++) {
        int thread_start = start + t * chunk_size;
        int thread_end = std::min(thread_start + chunk_size, end);

        if (thread_start >= end) break;

        threads.emplace_back([=]() {
            for (int i = thread_start; i < thread_end; i++) {
                func(i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

// Parallel for with 2D iteration (collapse(2) equivalent)
template<typename Func>
void parallel_for_2d(int rows, int cols, Func func, int num_threads = 0) {
    parallel_for(0, rows * cols, [&](int idx) {
        int i = idx / cols;
        int j = idx % cols;
        func(i, j);
    }, num_threads);
}

} // namespace ThreadPool

#endif // THREAD_POOL_H
