// 04_let_value.cpp
// Composición dinámica con let_value

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <random>

namespace ex = stdexec;

int main() {
    std::println("=== Let Value Examples ===\n");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // 1. let_value básico - retorna un nuevo sender
    {
        // then: value -> value
        // let_value: value -> sender (más poderoso!)

        auto work = ex::just(5)
            | ex::let_value([](int x) {
                // Retornamos un SENDER, no un valor
                return ex::just(x * 10);
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("let_value(5 -> just(50)) = {}", result);
    }

    // 2. let_value para decisiones dinámicas
    {
        auto work = ex::just(42)
            | ex::let_value([](int x) {
                if (x > 20) {
                    return ex::just(std::string("big number"));
                } else {
                    return ex::just(std::string("small number"));
                }
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("42 is a '{}'", result);
    }

    // 3. let_value para encadenar operaciones condicionales
    {
        auto fetch_user = [](int id) {
            return ex::just(id)
                | ex::then([](int id) {
                    return "User_" + std::to_string(id);
                });
        };

        auto fetch_profile = [](std::string user) {
            return ex::just(std::move(user))
                | ex::then([](std::string u) {
                    return u + "_profile";
                });
        };

        auto work = ex::just(123)
            | ex::let_value(fetch_user)
            | ex::let_value(fetch_profile);

        auto [result] = ex::sync_wait(work).value();
        std::println("Chained fetches: {}", result);
    }

    // 4. let_value con when_all interno
    {
        auto work = ex::just(10)
            | ex::let_value([&sched](int base) {
                // Spawn parallel work based on input
                auto t1 = ex::schedule(sched)
                    | ex::then([=] { return base + 1; });
                auto t2 = ex::schedule(sched)
                    | ex::then([=] { return base + 2; });

                return ex::when_all(std::move(t1), std::move(t2))
                    | ex::then([](int a, int b) { return a + b; });
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("Dynamic parallel: 11 + 12 = {}", result);
    }

    // 5. Retry pattern con let_value
    {
        int attempts = 0;

        // Simula operación que falla las primeras 2 veces
        auto unreliable_op = [&attempts]() {
            return ex::just()
                | ex::then([&]() -> int {
                    ++attempts;
                    if (attempts < 3) {
                        throw std::runtime_error("temporary failure");
                    }
                    return 42;
                });
        };

        // Simple retry (sin manejo de errores real por ahora)
        auto work = unreliable_op();

        // Nota: retry real requiere manejo de errores (ver ejemplo 06)
        try {
            auto [result] = ex::sync_wait(work).value();
            std::println("Got result after {} attempts: {}", attempts, result);
        } catch (const std::exception& e) {
            std::println("Failed after {} attempts: {}", attempts, e.what());
        }
    }

    // 6. let_value vs then comparison
    {
        std::println("\n--- then vs let_value ---");

        // then: transforma el valor directamente
        auto with_then = ex::just(5)
            | ex::then([](int x) {
                return x * 2;  // retorna VALUE
            });

        // let_value: retorna un nuevo sender (permite composición)
        auto with_let = ex::just(5)
            | ex::let_value([](int x) {
                return ex::just(x * 2);  // retorna SENDER
            });

        auto [r1] = ex::sync_wait(with_then).value();
        auto [r2] = ex::sync_wait(with_let).value();

        std::println("then result: {}", r1);
        std::println("let_value result: {}", r2);
        std::println("Same result, but let_value allows returning complex senders");
    }

    std::println("\n=== Key Concepts ===");
    std::println("- let_value: function returns a SENDER (dynamic composition)");
    std::println("- then: function returns a VALUE (simple transformation)");
    std::println("- Use let_value when you need to decide what work to do next");
    std::println("- Enables patterns like: retry, conditional branching, dynamic parallelism");

    return 0;
}
