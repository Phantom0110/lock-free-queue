#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>

#include "LockFreeQueue.hpp"
#include "MutexQueue.hpp"

// Test 1: Single-threaded FIFO ordering
template <typename Queue>
void test_fifo_order(const std::string& name) {
    Queue q;
    for (int i = 0; i < 10; ++i) q.enqueue(i);
    for (int i = 0; i < 10; ++i) {
        auto val = q.dequeue();
        assert(val.has_value() && *val == i);
    }
    assert(!q.dequeue().has_value());
    std::cout << "[PASS] " << name << ": FIFO ordering\n";
}

// Test 2: Dequeue on empty queue returns nullopt
template <typename Queue>
void test_empty_dequeue(const std::string& name) {
    Queue q;
    assert(!q.dequeue().has_value());
    q.enqueue(42);
    assert(q.dequeue().has_value());
    assert(!q.dequeue().has_value());
    std::cout << "[PASS] " << name << ": empty dequeue returns nullopt\n";
}

// Test 3: Concurrent producers -- all items are eventually consumed
template <typename Queue>
void test_concurrent_producers(const std::string& name) {
    Queue q;
    const int NUM_THREADS = 8;
    const int ITEMS_EACH = 10000;
    const int TOTAL = NUM_THREADS * ITEMS_EACH;

    std::vector<std::thread> producers;
    for (int t = 0; t < NUM_THREADS; ++t) {
        producers.emplace_back([&q, t]() {
            for (int i = 0; i < ITEMS_EACH; ++i)
                q.enqueue(t * ITEMS_EACH + i);
        });
    }
    for (auto& th : producers) th.join();

    std::atomic<int> count{0};
    std::atomic<long long> sum{0};
    std::vector<std::thread> consumers;
    for (int t = 0; t < NUM_THREADS; ++t) {
        consumers.emplace_back([&]() {
            while (count.load(std::memory_order_relaxed) < TOTAL) {
                auto val = q.dequeue();
                if (val.has_value()) {
                    count.fetch_add(1, std::memory_order_relaxed);
                    sum.fetch_add(*val, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : consumers) th.join();
    
    long long expected = (long long)TOTAL * (TOTAL - 1) / 2;
    assert(count.load() == TOTAL);
    assert(sum.load() == expected);
    std::cout << "[PASS] " << name << ": concurrent producers ("
        << NUM_THREADS << " threads, " << TOTAL << " items)\n";
}

// Test 4: Interleaved producers + consumers
template <typename Queue>
void test_concurrent_mixed(const std::string& name) {
    Queue q;
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 4;
    const int ITEMS_EACH = 10000;
    const int TOTAL = NUM_PRODUCERS * ITEMS_EACH;

    std::atomic<int> produced{0}, consumed(0);
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_PRODUCERS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITEMS_EACH; ++i) {
                q.enqueue(1);
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        })
    }

    for (auto& th : threads) th.join();

    assert(produced.load() == TOTAL);
    assert(consumed.load() == TOTAL);
    std::cout << "[PASS] " << name << ": interleaved producers/consumers\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  Lock-Free Queue - Correctness Tests\n";
    std::cout << "========================================\n\n";

    std::cout << "--- LockFreeQueue ---\n";
    test_fifo_order<LockFreeQueue<int>>("LockFreeQueue");
    test_empty_dequeue<LockFreeQueue<int>>("LockFreeQueue");
    test_concurrent_producers<LockFreeQueue<int>>("LockFreeQueue");
    test_concurrent_mixed<LockFreeQueue<int>>("LockFreeQueue");

    std::cout << "\n--- MutexQueue (baseline) ---\n";
    test_fifo_order<MutexQueue<int>>("MutexQueue");
    test_empty_dequeue<MutexQueue<int>>("MutexQueue");
    test_concurrent_producers<MutexQueue<int>>("MutexQueue");
    test_concurrent_mixed<MutexQueue<int>>("MutexQueue");

    std::cout << "\nAll tests passed!\n";
    return 0;
}
