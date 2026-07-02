#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <string>

#include "LockFreeQueue.hpp"
#include "MutexQueue.hpp"

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
    std::string label;
    int num_producers;
    int num_consumers;
    long long total_ops;
    double duration_ms;
    double throughput_mops;
};

template <typename Queue>
BenchmarkResult run_benchmark(
    const std::string& label,
    int num_producers,
    int num_consumers,
    int items_per_producer)
{
    Queue q;
    const long long total = (long long)num_producers * items_per_producer;

    std::atomic<bool> start_flag{false};
    std::atomic<long long> consumed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_producers; ++t) {
        threads.emplace_back([&, t]() {
            while (!start_flag.load(std::memory_order_acquire)) {}
            for (int i = 0; i < items_per_producer; ++i)
                q.enqueue(t * items_per_producer + i);
        })
    }

    for (int t = 0; t < num_consumers; ++t) {
        threads.emplace_back([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {}
            while (consumed.load(std::memory_order_relaxed) < total)
                if (q.dequeue().has_value())
                    consumed.fetch_add(1, std::memory_order_relaxed);
        })
    }

    auto t0 = Clock::now();
    start_flag.store(true, std::memory_order_release);
    for (auto& th : threads) th.join();
    auto t1 = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mops = (double)total / ms / 1000.0;
    return {label, num_producers, num_consumers, total, ms, mops};
}

void print_header() {
    std::cout << std::left
        << std::setw(14) << "Queue"
        << std::setw(10) << "Prod"
        << std::setw(10) << "Cons"
        << std::setw(14) << "Total Ops"
        << std::setw(12) << "Time (ms)"
        << std::setw(16) << "Throughput(Mop/s)"
        << "\n" << std::string(76, '-') << "\n";
}

void print_result(const BenchmarkResult& r) {
    std::cout << std::left
        << std::setw(14) << r.label
        << std::setw(10) << r.num_producers
        << std::setw(10) << r.num_consumers
        << std::setw(14) << r.total_ops
        << std::setw(12) << std::fixed << std::setprecision(1) << r.duration_ms
        << std::setw(16) << std::fixed << std::setprecision(2) << r.throughput_mops
        << "\n";
}

void save_csv(const std::vector<BenchmarkResult>& results, const std::string& path) {
    std::ofstream f(path);
    f << "queue,producers,consumers,total_ops,duration_ms,throughput_mops\n";
    for (const auto& r : results)
        f << r.label << "," << r.num_producers << "," << r.num_consumers 
            << "," << r.total_ops << "," << r.duration_ms << r.throughput_mops << "\n";
    
    std::cout << "\nResults saved to: " << path << "\n";
}

int main() {
    const int ITEMS_PER_PRODUCER = 500000;
    const std::vector<int> thread_counts = {1, 2, 4, 8};

    std::vector<BenchmarkResult> all_results;

    std::cout << "\n========================================\n";
    std::cout << " Lock-Free Queue - Benchmark\n";
    std::cout << " Hardware threads: " << std::thread::hardware_concurrency() << "\n";
    std::cout << " Items per producer: " << ITEMS_PER_PRODUCER << "\n";
    std::cout << "========================================\n";

    std::cout << "\n[ N producers / 1 consumer ]\n";
    print_header();
    for (int n : thread_counts) {
        auto r1 = run_benchmark<LockFreeQueue<int>>("LockFree", n, 1, ITEMS_PER_PRODUCER);
        auto r2 = run_benchmark<MutexQueue<int>>("Mutex", n, 1, ITEMS_PER_PRODUCER);
        print_result(r1); print_result(r2);
        all_results.push_back(r1); all_results.push_back(r2);
    }

    std::cout << "\n[ 1 producer / N consumers ]\n";
    print_header();
    for (int n : thread_counts) {
        auto r1 = run_benchmark<LockFreeQueue<int>>("LockFree", 1, n, ITEMS_PER_PRODUCER);
        auto r2 = run_benchmark<MutexQueue<int>>("Mutex", 1, n, ITEMS_PER_PRODUCER);
        print_result(r1); print_result(r2);
        all_results.push_back(r1); all_results.push_back(r2);
    }

    std::cout << "\n[ N producers / N consumers ]\n";
    print_header();
    for (int n : thread_counts) {
        auto r1 = run_benchmark<LockFreeQueue<int>>("LockFree", n, n, ITEMS_PER_PRODUCER);
        auto r2 = run_benchmark<MutexQueue<int>>("Mutex", n, n, ITEMS_PER_PRODUCER);
        print_result(r1); print_result(r2);
        all_results.push_back(r1); all_results.push_back(r2);
    }

    save_csv(all_results, "benchmark_results.csv");
    return 0;
}
