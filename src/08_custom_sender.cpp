// 08_custom_sender.cpp
// Crear senders personalizados

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <print>
#include <chrono>
#include <thread>
#include <optional>

namespace ex = stdexec;
using namespace std::chrono_literals;

// ============================================================================
// Ejemplo 1: Sender simple usando las facilidades de stdexec
// ============================================================================

// Un sender que produce un valor fijo después de un delay
auto delayed_value(int value, std::chrono::milliseconds delay) {
    return ex::just(value, delay)
        | ex::then([](int v, std::chrono::milliseconds d) {
            std::this_thread::sleep_for(d);
            return v;
        });
}

// ============================================================================
// Ejemplo 2: Sender personalizado completo (para entender la mecánica)
// ============================================================================

// Un sender que genera un rango de números
template <typename Receiver>
struct range_operation {
    int start_;
    int end_;
    Receiver receiver_;

    friend void tag_invoke(ex::start_t, range_operation& self) noexcept {
        // Generar números y completar
        std::vector<int> values;
        for (int i = self.start_; i < self.end_; ++i) {
            values.push_back(i);
        }
        ex::set_value(std::move(self.receiver_), std::move(values));
    }
};

struct range_sender {
    using sender_concept = ex::sender_t;

    int start_;
    int end_;

    // Declara qué tipo de completación produce este sender
    using completion_signatures = ex::completion_signatures<
        ex::set_value_t(std::vector<int>),
        ex::set_error_t(std::exception_ptr)
    >;

    template <typename Receiver>
    friend auto tag_invoke(ex::connect_t, range_sender self, Receiver receiver) {
        return range_operation<Receiver>{self.start_, self.end_, std::move(receiver)};
    }
};

auto range(int start, int end) {
    return range_sender{start, end};
}

// ============================================================================
// Ejemplo 3: Adaptador de callback a sender
// ============================================================================

// Simula una API async basada en callbacks
template <typename Callback>
void legacy_async_api(int input, Callback&& callback) {
    // Simula trabajo asíncrono
    std::thread([input, cb = std::forward<Callback>(callback)]() mutable {
        std::this_thread::sleep_for(50ms);
        cb(input * 2);  // Llama callback con resultado
    }).detach();
}

// Wrapper que convierte callback API a sender
auto async_multiply(int value) {
    return ex::just(value)
        | ex::let_value([](int v) {
            // Para una conversión real, usarías algo más sofisticado
            // Por ahora, simulamos con un delay
            return ex::just(v)
                | ex::then([](int x) {
                    std::this_thread::sleep_for(50ms);
                    return x * 2;
                });
        });
}

// ============================================================================
// Ejemplo 4: Sender que puede fallar
// ============================================================================

auto divide(int numerator, int denominator) {
    return ex::just(numerator, denominator)
        | ex::then([](int num, int denom) -> int {
            if (denom == 0) {
                throw std::invalid_argument("division by zero");
            }
            return num / denom;
        });
}

// ============================================================================
// Ejemplo 5: Sender retry genérico
// ============================================================================

template <typename SenderFactory>
auto with_retry(SenderFactory factory, int max_attempts) {
    return ex::just(0)
        | ex::let_value([factory = std::move(factory), max_attempts](int attempt) mutable {
            return factory()
                | ex::let_error([factory, max_attempts, attempt](std::exception_ptr ep) mutable {
                    if (attempt + 1 < max_attempts) {
                        std::println("  Attempt {} failed, retrying...", attempt + 1);
                        // Recursión sería ideal pero complicada
                        // Simplificamos con un intento más
                        return factory();
                    }
                    std::rethrow_exception(ep);
                    return factory();  // unreachable, para tipo
                });
        });
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::println("=== Custom Sender Examples ===\n");

    exec::static_thread_pool pool(4);
    auto sched = pool.get_scheduler();

    // 1. Usar delayed_value
    {
        std::println("--- delayed_value ---");
        auto start = std::chrono::steady_clock::now();

        auto [result] = ex::sync_wait(delayed_value(42, 100ms)).value();

        auto elapsed = std::chrono::steady_clock::now() - start;
        std::println("Got {} after {}ms",
            result,
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
    }

    // 2. Usar range sender personalizado
    {
        std::println("\n--- custom range sender ---");

        auto work = range(1, 6)
            | ex::then([](std::vector<int> nums) {
                int sum = 0;
                for (int n : nums) sum += n;
                return sum;
            });

        auto [result] = ex::sync_wait(work).value();
        std::println("Sum of 1..5 = {}", result);
    }

    // 3. Callback API convertida
    {
        std::println("\n--- callback to sender ---");

        auto [result] = ex::sync_wait(async_multiply(21)).value();
        std::println("21 * 2 = {}", result);
    }

    // 4. División con error handling
    {
        std::println("\n--- divide with errors ---");

        // Caso exitoso
        auto [ok] = ex::sync_wait(divide(10, 2)).value();
        std::println("10 / 2 = {}", ok);

        // Caso con error
        auto bad = divide(10, 0)
            | ex::upon_error([](std::exception_ptr ep) {
                try {
                    std::rethrow_exception(ep);
                } catch (const std::exception& e) {
                    std::println("Error: {}", e.what());
                }
                return -1;
            });

        auto [err] = ex::sync_wait(bad).value();
        std::println("Error result: {}", err);
    }

    // 5. Retry pattern
    {
        std::println("\n--- retry pattern ---");

        int call_count = 0;
        auto flaky_operation = [&call_count]() {
            return ex::just()
                | ex::then([&call_count]() -> int {
                    ++call_count;
                    if (call_count < 3) {
                        throw std::runtime_error("transient failure");
                    }
                    return 42;
                });
        };

        try {
            auto [result] = ex::sync_wait(with_retry(flaky_operation, 5)).value();
            std::println("Success after {} calls: {}", call_count, result);
        } catch (const std::exception& e) {
            std::println("Failed: {}", e.what());
        }
    }

    std::println("\n=== Key Concepts ===");
    std::println("- Senders can be created by composing existing ones");
    std::println("- Custom senders need: sender_concept, completion_signatures, connect");
    std::println("- Operations need: start() to begin execution");
    std::println("- Completion via: set_value, set_error, set_stopped");
    std::println("- Prefer composition over custom senders when possible");

    return 0;
}
