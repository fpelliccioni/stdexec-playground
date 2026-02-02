// 03_when_all.cpp
// Ejecución paralela con when_all

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <thread>
#include <chrono>

namespace ex = stdexec;
using namespace std::chrono_literals;

int main() {
    std::println("=== When All Examples ===\n");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // 1. when_all básico - espera que todos completen
    {
        auto work = ex::when_all(
            ex::just(1),
            ex::just(2),
            ex::just(3)
        );

        auto [a, b, c] = ex::sync_wait(work).value();
        std::println("when_all(1, 2, 3) -> {}, {}, {}", a, b, c);
    }

    // 2. when_all con trabajo real en paralelo
    {
        auto start = std::chrono::steady_clock::now();

        auto task1 = ex::schedule(sched)
            | ex::then([] {
                std::this_thread::sleep_for(100ms);
                return 1;
            });

        auto task2 = ex::schedule(sched)
            | ex::then([] {
                std::this_thread::sleep_for(100ms);
                return 2;
            });

        auto task3 = ex::schedule(sched)
            | ex::then([] {
                std::this_thread::sleep_for(100ms);
                return 3;
            });

        auto [r1, r2, r3] = ex::sync_wait(
            ex::when_all(std::move(task1), std::move(task2), std::move(task3))
        ).value();

        auto elapsed = std::chrono::steady_clock::now() - start;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        std::println("Parallel tasks returned: {}, {}, {}", r1, r2, r3);
        std::println("Time: {}ms (sequential would be ~300ms)", ms);
    }

    // 3. when_all con diferentes tipos
    {
        auto work = ex::when_all(
            ex::just(42),
            ex::just(std::string("hello")),
            ex::just(3.14)
        );

        auto [num, str, dbl] = ex::sync_wait(work).value();
        std::println("Mixed types: {}, '{}', {}", num, str, dbl);
    }

    // 4. when_all con transformación posterior
    {
        auto work = ex::when_all(
            ex::just(10),
            ex::just(20),
            ex::just(30)
        )
        | ex::then([](int a, int b, int c) {
            return a + b + c;
        });

        auto [sum] = ex::sync_wait(work).value();
        std::println("Sum of parallel results: {}", sum);
    }

    // 5. Nested when_all
    {
        auto group1 = ex::when_all(ex::just(1), ex::just(2))
            | ex::then([](int a, int b) { return a + b; });

        auto group2 = ex::when_all(ex::just(10), ex::just(20))
            | ex::then([](int a, int b) { return a + b; });

        auto [r1, r2] = ex::sync_wait(
            ex::when_all(std::move(group1), std::move(group2))
        ).value();

        std::println("Nested groups: {} and {} -> total {}", r1, r2, r1 + r2);
    }

    std::println("\n=== Key Concepts ===");
    std::println("- when_all() waits for ALL senders to complete");
    std::println("- Results are combined as a tuple");
    std::println("- With a thread pool, tasks run in parallel");
    std::println("- Great for fork-join patterns");

    return 0;
}
