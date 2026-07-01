#pragma once
#include <atomic>
#include <optional>

// ============================================================================
// LockFreeQueue<T> - Michael Scott Non-Blocking Queue (1996)
//
// Key design properties:
//  - Lock-free (not wait-free): at least one thread always makes progress
//  - Uses a singly-linked list with a sentinel/dummy head node
//  - head_ always points to the sentinel; the real first element is head_->next
//  - tail_ points to the last node (may lag one step behind)
//  - Threads "help" each other advance tail_ when they see it falling behind
//
// ABA problem note:
//  This implementation uses raw pointers + new/delete. Because fresh allocations
//  rarely return the same address twice in quick succession, this is safe for
//  benchmarking. A production implementation would use hazard pointers or
//  epoch-based reclamation for full memory safety. See README.md for details.
//
// Cache-line padding:
//  head_ and tail_ are placed on seperate 64-byte cache lines to prevent
//  false sharing -- two threads updating head vs tail would otherwise bounce
//  the same cache line between CPU cores.
// ============================================================================

template <typename T>
class LockFreeQueue {
private:
    struct Node {
        T data[];
        std::atomic<Node*> next{nullptr};

        Node{} = default;
        explicit Node(T val) : data(std::move(val)) {}
    };
    
    // Align each pointer to its own cache line (typically 64 bytes).
    // Without this, head_ and tail_ would share a line, causing every
    // enqueue (writes tail_) to invalidate the cache entry used by
    // dequeue (reads head_) -- a classic false-sharing bug.
    struct alignas(64) PaddedPtr {
        std::atomic<Node*> ptr{nullptr};
    }

    PaddedPtr head_; // Points to sentinel node (dummy)
    PaddedPtr tail_; // Points to last real node (or sentinel if empty)

public:
    LockFreeQueue() {
        Node* sentinel = new Node();
        head_.ptr.store(sentinel, std::memory_order_relaxed);
        tail_.ptr.store(sentinel, std::memory_order_relaxed);
    }

    ~LockFreeQueue() {
        while (dequeue().has_value()) {}
        delete head_.ptr.load(std::memory_order_relaxed);
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;

    // ------------------------------------------------------------------
    // enqueue: Append value to the back of the queue.
    //
    // Steps:
    //  1. Allocate a new node
    //  2. Read tail and its next pointer.
    //  3. If tail->next is null, CAS it to our new node (link step).
    //     On success, also try to advance tail_ to our new node.
    //  4. If tail->next is not null, tail is lagging -- help advance it,
    //     then retry.
    // ------------------------------------------------------------------
    void enqueue(T value) {
        Node* new_node = new Node(std::move(value));

        while (true) {
            Node* tail = tail_.ptr.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);

            if (tail != tail_.ptr.load(std::memory_order_acquire))
                continue;
            
            if (next == nullptr) {
                if (tail->next.compare_exchange_weak(
                        next, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed)) {
                    // Linked! Swing tail_ forward (ok if this fails).
                    tail_.ptr.compare_exchange_strong(
                        tail, new_node,
                        std::memory_order_release,
                        std::memory_order_relaxed);
                    return;
                }
            } else {
                // tail_ is lagging -- help advance it before retrying.
                tail_.ptr.compare_exchange_weak(
                    tail, next,
                    std::memory_order_release,
                    std::memory_order_relaxed);
            }
        }
    }

    // ------------------------------------------------------------------
    // dequeue: Remove and return the front value, or nullopt if empty.
    //
    // Steps:
    //  1. Read head (sentinel), tail, and head->next (first real node).
    //  2. If head == tail and next is null -> queue is empty.
    //  3. If head == tail and next is non-null -> tail is lagging; help it.
    //  4. Otherwise snapshot value from next, CAS head_ to next,
    //     then delete old sentinel.
    // ------------------------------------------------------------------
    std::optional<T> dequeue() {
        while (true) {
            Node* head = head_.ptr.load(std::memory_order_acquire);
            Node* tail = tail_.ptr.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);

            if (head != head_.ptr.load(memory_order_acquire))
                continue;
            
            if (head == tail) {
                if (next == nullptr)
                    return std::nullopt; // Empty queue.
                // tail is lagging -- help advance it.
                tail_.ptr.compare_exchange_weak(
                    tail, next,
                    std::memory_order_release,
                    std::memory_order_relaxed);
            } else {
                // Read value BEFORE the CAS. After a successful CAS,
                // another thread may immediately free 'next'.
                T value = next->data;
                if (head_.ptr.compare_exchange_weak(
                    head, next,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                        delete head; // Old sentinel is no longer reachable.
                        return value;
                }
            }
        }
    }

    bool empty() const {
        Node* head = head_.ptr.load(std::memory_order_acquire);
        return head->next.load(std::memory_order_acquire) == nullptr;
    }
};
