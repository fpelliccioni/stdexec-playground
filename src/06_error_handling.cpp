// 06_error_handling.cpp
// Manejo de errores con senders

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <stdexcept>
#include <system_error>

namespace ex = stdexec;

int main() {
    std::println("=== Error Handling Examples ===\n");

    // 1. Errores se propagan automáticamente
    {
        std::println("--- Error propagation ---");

        auto work = ex::just(10)
            | ex::then([](int x) -> int {
                if (x > 5) {
                    throw std::runtime_error("Value too large!");
                }
                return x * 2;
            })
            | ex::then([](int x) {
                std::println("This won't run if error occurred");
                return x + 1;
            });

        try {
            auto result = ex::sync_wait(work);
            if (result) {
                std::println("Got: {}", std::get<0>(*result));
            }
        } catch (const std::exception& e) {
            std::println("Caught exception: {}", e.what());
        }
    }

    // 2. just_error - crear un sender que falla inmediatamente
    {
        std::println("\n--- just_error ---");

        auto failing_sender = ex::just_error(std::make_exception_ptr(
            std::runtime_error("intentional error")
        ));

        try {
            ex::sync_wait(failing_sender);
        } catch (const std::exception& e) {
            std::println("Caught: {}", e.what());
        }
    }

    // 3. upon_error - manejar errores
    {
        std::println("\n--- upon_error ---");

        auto work = ex::just(100)
            | ex::then([](int x) -> int {
                throw std::runtime_error("oops");
                return x;  // never reached
            })
            | ex::upon_error([](std::exception_ptr ep) {
                try {
                    if (ep) std::rethrow_exception(ep);
                } catch (const std::exception& e) {
                    std::println("Handling error: {}", e.what());
                }
                return -1;  // default value on error
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("Result after error handling: {}", result);
    }

    // 4. let_error - recuperación dinámica
    {
        std::println("\n--- let_error (dynamic recovery) ---");

        auto work = ex::just(42)
            | ex::then([](int) -> int {
                throw std::runtime_error("network error");
            })
            | ex::let_error([](std::exception_ptr) {
                std::println("Primary failed, trying fallback...");
                return ex::just(999);  // fallback sender
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("Recovered with: {}", result);
    }

    // 5. Diferentes tipos de errores
    {
        std::println("\n--- Different error types ---");

        // Sistema que puede fallar de diferentes maneras
        enum class ErrorCode { NetworkError, ParseError, Timeout };

        auto might_fail = [](bool should_fail) {
            return ex::just(should_fail)
                | ex::then([](bool fail) -> int {
                    if (fail) {
                        throw std::runtime_error("operation failed");
                    }
                    return 42;
                });
        };

        // Caso exitoso
        auto [success] = ex::sync_wait(might_fail(false)).value();
        std::println("Success: {}", success);

        // Caso fallido
        try {
            ex::sync_wait(might_fail(true));
        } catch (const std::exception& e) {
            std::println("Failed: {}", e.what());
        }
    }

    // 6. Patrón retry con let_error
    {
        std::println("\n--- Retry pattern ---");

        int attempt = 0;
        const int max_retries = 3;

        // Función que crea un sender con retry logic
        // Nota: en stdexec real, hay algoritmos de retry más sofisticados
        auto with_single_retry = [&]() {
            return ex::just()
                | ex::then([&]() -> int {
                    ++attempt;
                    std::println("Attempt {}", attempt);
                    if (attempt < 3) {
                        throw std::runtime_error("transient error");
                    }
                    return 42;
                })
                | ex::let_error([&](std::exception_ptr) {
                    if (attempt < max_retries) {
                        std::println("Retrying...");
                        return ex::just()
                            | ex::then([&]() -> int {
                                ++attempt;
                                std::println("Attempt {}", attempt);
                                if (attempt < 3) {
                                    throw std::runtime_error("transient error");
                                }
                                return 42;
                            });
                    }
                    // Re-throw after max retries
                    return ex::just()
                        | ex::then([]() -> int {
                            throw std::runtime_error("max retries exceeded");
                        });
                });
        };

        try {
            auto [result] = ex::sync_wait(with_single_retry()).value();
            std::println("Eventually succeeded: {}", result);
        } catch (const std::exception& e) {
            std::println("Gave up: {}", e.what());
        }
    }

    // 7. stopped vs error
    {
        std::println("\n--- stopped vs error ---");
        std::println("Senders have THREE completion channels:");
        std::println("  - set_value: success with value(s)");
        std::println("  - set_error: failure with error");
        std::println("  - set_stopped: cancellation (no error, just stopped)");

        // just_stopped creates a sender that completes with 'stopped'
        auto stopped_sender = ex::just_stopped();

        auto result = ex::sync_wait(
            stopped_sender
            | ex::upon_stopped([] {
                std::println("Work was cancelled/stopped");
                return 0;
            })
        );

        if (result) {
            std::println("Got result: {}", std::get<0>(*result));
        } else {
            std::println("sync_wait returned nullopt (stopped)");
        }
    }

    std::println("\n=== Key Concepts ===");
    std::println("- Errors propagate through the chain (like exceptions)");
    std::println("- upon_error: transform error to value");
    std::println("- let_error: recover with a new sender");
    std::println("- Three channels: value, error, stopped");
    std::println("- sync_wait returns optional (nullopt if stopped)");

    return 0;
}
