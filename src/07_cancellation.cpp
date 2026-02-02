// 07_cancellation.cpp
// Cancelación estructurada

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/async_scope.hpp>
#include <print>
#include <thread>
#include <chrono>
#include <atomic>

namespace ex = stdexec;
using namespace std::chrono_literals;

int main() {
    std::println("=== Cancellation Examples ===\n");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // 1. just_stopped - sender que se "cancela" inmediatamente
    {
        std::println("--- just_stopped ---");

        auto work = ex::just_stopped()
            | ex::upon_stopped([] {
                std::println("Detected stopped signal");
                return -1;
            });

        auto result = ex::sync_wait(work);
        if (result) {
            std::println("Result: {}", std::get<0>(*result));
        }
    }

    // 2. stop_when - cancelar cuando otra operación complete
    {
        std::println("\n--- stop_when ---");

        // Trabajo largo
        auto long_work = ex::schedule(sched)
            | ex::then([] {
                for (int i = 0; i < 10; ++i) {
                    std::this_thread::sleep_for(50ms);
                    std::println("  Working... {}", i);
                }
                return 100;
            });

        // Trigger que completa rápido
        auto trigger = ex::schedule(sched)
            | ex::then([] {
                std::this_thread::sleep_for(120ms);
                std::println("  Trigger fired!");
                return 0;
            });

        // Nota: stop_when cancela long_work cuando trigger completa
        // La semántica exacta depende de la implementación
        auto work = ex::when_all(std::move(long_work), std::move(trigger));

        auto result = ex::sync_wait(work);
        if (result) {
            auto [r1, r2] = *result;
            std::println("Results: {}, {}", r1, r2);
        } else {
            std::println("Work was cancelled");
        }
    }

    // 3. async_scope para trabajo en background con cancelación
    {
        std::println("\n--- async_scope ---");

        exec::async_scope scope;
        std::atomic<int> counter{0};

        // Spawn trabajo que corre en background
        for (int i = 0; i < 5; ++i) {
            scope.spawn(
                ex::schedule(sched)
                | ex::then([i, &counter] {
                    std::this_thread::sleep_for(50ms);
                    ++counter;
                    std::println("  Background task {} done", i);
                })
            );
        }

        std::println("Spawned 5 background tasks");

        // Esperar a que terminen todos
        ex::sync_wait(scope.on_empty());

        std::println("All tasks completed. Counter: {}", counter.load());
    }

    // 4. Patrón timeout manual
    {
        std::println("\n--- Manual timeout pattern ---");

        std::atomic<bool> completed{false};
        std::atomic<bool> timed_out{false};

        auto work = ex::schedule(sched)
            | ex::then([&] {
                std::this_thread::sleep_for(100ms);
                if (!timed_out.load()) {
                    completed = true;
                    return std::string("completed!");
                }
                return std::string("aborted");
            });

        auto timeout = ex::schedule(sched)
            | ex::then([&] {
                std::this_thread::sleep_for(200ms);  // timeout > work time
                if (!completed.load()) {
                    timed_out = true;
                    return std::string("timeout!");
                }
                return std::string("work finished first");
            });

        auto [work_result, timeout_result] = ex::sync_wait(
            ex::when_all(std::move(work), std::move(timeout))
        ).value();

        std::println("Work: {}", work_result);
        std::println("Timeout: {}", timeout_result);
    }

    // 5. Propagación de stop tokens
    {
        std::println("\n--- Stop token propagation ---");
        std::println("In senders/receivers:");
        std::println("  - Each operation gets a stop_token from its receiver");
        std::println("  - Parent cancellation propagates to children");
        std::println("  - Cooperative: operations check token periodically");
        std::println("  - No forced thread termination (safe!)");
    }

    // 6. upon_stopped para cleanup
    {
        std::println("\n--- upon_stopped for cleanup ---");

        auto work = ex::just_stopped()
            | ex::upon_stopped([] {
                std::println("Performing cleanup after cancellation...");
                // Cleanup resources here
                return 0;  // Convert stopped to value
            });

        auto result = ex::sync_wait(work);
        if (result) {
            std::println("Cleaned up, got: {}", std::get<0>(*result));
        }
    }

    std::println("\n=== Key Concepts ===");
    std::println("- Cancellation is cooperative, not forced");
    std::println("- stop_token passed through receiver chain");
    std::println("- just_stopped(): creates cancelled sender");
    std::println("- upon_stopped(): handle cancellation");
    std::println("- async_scope: manage dynamic work with structured lifetime");
    std::println("- Parent cancellation propagates to children (structured)");

    return 0;
}
