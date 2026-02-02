// 05_thread_pool.cpp
// Uso del thread pool y schedulers

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <thread>
#include <chrono>

namespace ex = stdexec;
using namespace std::chrono_literals;

int main() {
    std::println("=== Thread Pool & Scheduler Examples ===\n");
    std::println("Main thread: {}", std::this_thread::get_id());

    // 1. Crear thread pool
    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // 2. schedule() - el punto de entrada a un scheduler
    {
        std::println("\n--- schedule() basics ---");

        auto work = ex::schedule(sched)
            | ex::then([] {
                std::println("Running on thread: {}", std::this_thread::get_id());
                return 42;
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("Result: {}", result);
    }

    // 3. Múltiples tareas en el pool
    {
        std::println("\n--- Multiple tasks on pool ---");

        auto make_task = [&](int id) {
            return ex::schedule(sched)
                | ex::then([id] {
                    std::println("Task {} on thread {}", id, std::this_thread::get_id());
                    std::this_thread::sleep_for(50ms);
                    return id * 10;
                });
        };

        auto work = ex::when_all(
            make_task(1),
            make_task(2),
            make_task(3),
            make_task(4)
        );

        auto [r1, r2, r3, r4] = ex::sync_wait(work).value();
        std::println("Results: {}, {}, {}, {}", r1, r2, r3, r4);
    }

    // 4. transfer() - mover ejecución a otro scheduler
    {
        std::println("\n--- transfer() between contexts ---");

        exec::static_thread_pool pool2(2);
        auto sched2 = pool2.get_scheduler();

        auto work = ex::schedule(sched)
            | ex::then([] {
                std::println("First on thread {}", std::this_thread::get_id());
                return 100;
            })
            | ex::transfer(sched2)  // cambiar de contexto
            | ex::then([](int x) {
                std::println("After transfer on thread {}", std::this_thread::get_id());
                return x + 1;
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("Final result: {}", result);
    }

    // 5. on() - ejecutar un sender en un scheduler específico
    {
        std::println("\n--- on() for scoped execution ---");

        // inline_scheduler ejecuta en el thread actual
        auto cpu_work = ex::just(10)
            | ex::then([](int x) {
                std::println("CPU work on thread {}", std::this_thread::get_id());
                return x * 2;
            });

        // Ejecutar todo el sender en el pool
        auto on_pool = ex::on(sched, std::move(cpu_work));

        auto [result] = ex::sync_wait(on_pool).value();
        std::println("Result: {}", result);
    }

    // 6. Patrón productor-consumidor simple
    {
        std::println("\n--- Producer-Consumer pattern ---");

        exec::static_thread_pool producers(2);
        exec::static_thread_pool consumers(2);

        auto produce = [&](int id) {
            return ex::schedule(producers.get_scheduler())
                | ex::then([id] {
                    std::println("Producing {} on thread {}", id, std::this_thread::get_id());
                    std::this_thread::sleep_for(30ms);
                    return id * 100;
                });
        };

        auto consume = [&](int value) {
            return ex::schedule(consumers.get_scheduler())
                | ex::then([value] {
                    std::println("Consuming {} on thread {}", value, std::this_thread::get_id());
                    return value + 1;
                });
        };

        auto work = produce(1)
            | ex::let_value([&](int val) { return consume(val); });

        auto [result] = ex::sync_wait(work).value();
        std::println("Final: {}", result);
    }

    // 7. Bulk operations (si disponible)
    {
        std::println("\n--- Bulk operations ---");

        std::vector<int> results(8, 0);

        auto work = ex::schedule(sched)
            | ex::bulk(8, [&results](std::size_t i) {
                results[i] = static_cast<int>(i * i);
                std::println("bulk[{}] on thread {}", i, std::this_thread::get_id());
            });

        ex::sync_wait(work);

        std::print("Results: ");
        for (int r : results) std::print("{} ", r);
        std::println("");
    }

    std::println("\n=== Key Concepts ===");
    std::println("- static_thread_pool: manages a fixed number of threads");
    std::println("- scheduler: abstraction for 'where' work runs");
    std::println("- schedule(sched): creates a sender that starts on sched");
    std::println("- transfer(sched): moves execution to sched");
    std::println("- on(sched, sender): runs entire sender on sched");
    std::println("- bulk(n, fn): parallel for-each pattern");

    return 0;
}
