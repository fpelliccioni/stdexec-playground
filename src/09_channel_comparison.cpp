// 09_channel_comparison.cpp
// Comparación con ASIO channels - patrones similares con senders

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/async_scope.hpp>
#include <print>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <optional>

namespace ex = stdexec;
using namespace std::chrono_literals;

// ============================================================================
// Simple Channel implementation para demostrar el patrón
// (En producción usarías algo más robusto)
// ============================================================================

template <typename T>
class simple_channel {
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;
    size_t capacity_;

public:
    explicit simple_channel(size_t capacity = 16) : capacity_(capacity) {}

    bool send(T value) {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return queue_.size() < capacity_ || closed_; });
        if (closed_) return false;
        queue_.push(std::move(value));
        cv_.notify_one();
        return true;
    }

    std::optional<T> receive() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || closed_; });
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        cv_.notify_one();
        return value;
    }

    void close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }
};

// ============================================================================
// Patrón 1: Producer-Consumer con channel
// ============================================================================

void channel_producer_consumer() {
    std::println("--- Channel-based Producer-Consumer ---");

    simple_channel<int> channel(4);
    std::atomic<bool> done{false};

    // Producer thread
    std::thread producer([&] {
        for (int i = 1; i <= 5; ++i) {
            std::println("  Producing: {}", i);
            channel.send(i);
            std::this_thread::sleep_for(20ms);
        }
        channel.close();
    });

    // Consumer thread
    std::thread consumer([&] {
        while (auto value = channel.receive()) {
            std::println("  Consumed: {}", *value);
        }
        std::println("  Channel closed");
    });

    producer.join();
    consumer.join();
}

// ============================================================================
// Patrón 1 alternativo: Producer-Consumer con senders
// ============================================================================

void sender_producer_consumer() {
    std::println("\n--- Sender-based Producer-Consumer ---");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // Con senders, modelamos como un pipeline de transformaciones
    // en lugar de una cola explícita

    auto produce = [](int start, int count) {
        // Genera trabajo que produce valores
        std::vector<int> values;
        for (int i = start; i < start + count; ++i) {
            values.push_back(i);
        }
        return ex::just(std::move(values));
    };

    auto consume = [](std::vector<int> values) {
        for (int v : values) {
            std::println("  Processing: {}", v);
        }
        return values.size();
    };

    auto work = produce(1, 5)
        | ex::then([](std::vector<int> v) {
            std::println("  Produced {} items", v.size());
            return v;
        })
        | ex::then(consume);

    auto [count] = ex::sync_wait(work).value();
    std::println("  Total processed: {}", count);
}

// ============================================================================
// Patrón 2: Fan-out / Fan-in con channels
// ============================================================================

void channel_fan_out_fan_in() {
    std::println("\n--- Channel-based Fan-out/Fan-in ---");

    simple_channel<int> input(8);
    simple_channel<int> output(8);

    // Llenar input
    for (int i = 1; i <= 4; ++i) {
        input.send(i * 10);
    }
    input.close();

    // Workers (fan-out)
    std::vector<std::thread> workers;
    for (int w = 0; w < 2; ++w) {
        workers.emplace_back([&, w] {
            while (auto value = input.receive()) {
                std::println("  Worker {} processing {}", w, *value);
                std::this_thread::sleep_for(30ms);
                output.send(*value * 2);
            }
        });
    }

    // Collector thread
    std::thread collector([&] {
        // Esperamos resultados conocidos
        for (int i = 0; i < 4; ++i) {
            if (auto result = output.receive()) {
                std::println("  Collected: {}", *result);
            }
        }
    });

    for (auto& w : workers) w.join();
    output.close();
    collector.join();
}

// ============================================================================
// Patrón 2 alternativo: Fan-out / Fan-in con senders
// ============================================================================

void sender_fan_out_fan_in() {
    std::println("\n--- Sender-based Fan-out/Fan-in ---");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // Con senders, fan-out/fan-in es más natural con when_all
    auto process = [&](int value) {
        return ex::schedule(sched)
            | ex::then([value] {
                std::println("  Processing {} on thread {}",
                    value, std::this_thread::get_id());
                std::this_thread::sleep_for(30ms);
                return value * 2;
            });
    };

    // Fan-out: procesar en paralelo
    // Fan-in: when_all combina resultados
    auto work = ex::when_all(
        process(10),
        process(20),
        process(30),
        process(40)
    )
    | ex::then([](int a, int b, int c, int d) {
        std::println("  Results: {}, {}, {}, {}", a, b, c, d);
        return a + b + c + d;
    });

    auto [total] = ex::sync_wait(work).value();
    std::println("  Total: {}", total);
}

// ============================================================================
// Patrón 3: Pipeline con channels
// ============================================================================

void channel_pipeline() {
    std::println("\n--- Channel-based Pipeline ---");

    simple_channel<int> stage1_out(4);
    simple_channel<int> stage2_out(4);

    // Stage 1: multiply by 2
    std::thread s1([&] {
        for (int i = 1; i <= 3; ++i) {
            int result = i * 2;
            std::println("  Stage1: {} -> {}", i, result);
            stage1_out.send(result);
        }
        stage1_out.close();
    });

    // Stage 2: add 10
    std::thread s2([&] {
        while (auto v = stage1_out.receive()) {
            int result = *v + 10;
            std::println("  Stage2: {} -> {}", *v, result);
            stage2_out.send(result);
        }
        stage2_out.close();
    });

    // Collector
    std::thread collector([&] {
        while (auto v = stage2_out.receive()) {
            std::println("  Final: {}", *v);
        }
    });

    s1.join();
    s2.join();
    collector.join();
}

// ============================================================================
// Patrón 3 alternativo: Pipeline con senders
// ============================================================================

void sender_pipeline() {
    std::println("\n--- Sender-based Pipeline ---");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // Con senders, un pipeline es simplemente composición con then/let_value
    auto process_item = [&](int i) {
        return ex::schedule(sched)
            | ex::then([i] {
                int r1 = i * 2;
                std::println("  Stage1: {} -> {}", i, r1);
                return r1;
            })
            | ex::then([](int v) {
                int r2 = v + 10;
                std::println("  Stage2: {} -> {}", v, r2);
                return r2;
            })
            | ex::then([](int v) {
                std::println("  Final: {}", v);
                return v;
            });
    };

    // Procesar múltiples items en paralelo
    auto work = ex::when_all(
        process_item(1),
        process_item(2),
        process_item(3)
    );

    ex::sync_wait(work);
}

// ============================================================================
// Comparación conceptual
// ============================================================================

void comparison_summary() {
    std::println("\n=== Comparison Summary ===\n");

    std::println("ASIO Channels:");
    std::println("  + Explicit data flow, familiar CSP model");
    std::println("  + Dynamic producers/consumers");
    std::println("  + Backpressure via bounded channels");
    std::println("  - Manual thread coordination");
    std::println("  - Harder to compose");
    std::println("  - Error handling is manual");

    std::println("\nSenders/Receivers:");
    std::println("  + Composable (|, when_all, let_value)");
    std::println("  + Type-safe error propagation");
    std::println("  + Structured concurrency (cancellation)");
    std::println("  + No heap allocation required");
    std::println("  + Lazy evaluation");
    std::println("  - Less intuitive for pure message passing");
    std::println("  - Learning curve for new concepts");

    std::println("\nWhen to use what:");
    std::println("  - Channels: Dynamic message passing, actor-like patterns");
    std::println("  - Senders: Structured async workflows, pipelines, fork-join");
    std::println("  - Both can coexist! Wrap channels in senders for best of both");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::println("=== Channel vs Sender Patterns ===\n");

    // Producer-Consumer
    channel_producer_consumer();
    sender_producer_consumer();

    // Fan-out / Fan-in
    channel_fan_out_fan_in();
    sender_fan_out_fan_in();

    // Pipeline
    channel_pipeline();
    sender_pipeline();

    // Summary
    comparison_summary();

    return 0;
}
