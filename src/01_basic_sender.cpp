// 01_basic_sender.cpp
// Conceptos básicos: just, sync_wait

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <print>

namespace ex = stdexec;

int main() {
    std::println("=== Basic Sender Examples ===\n");

    // 1. El sender más simple: just() produce un valor inmediatamente
    {
        auto sender = ex::just(42);
        auto [value] = ex::sync_wait(sender).value();
        std::println("just(42) -> {}", value);
    }

    // 2. just() puede producir múltiples valores
    {
        auto sender = ex::just(1, 2, 3);
        auto [a, b, c] = ex::sync_wait(sender).value();
        std::println("just(1,2,3) -> {}, {}, {}", a, b, c);
    }

    // 3. just() con tipos complejos
    {
        auto sender = ex::just(std::string("hello"), 3.14);
        auto [str, num] = ex::sync_wait(sender).value();
        std::println("just(string, double) -> '{}', {}", str, num);
    }

    // 4. Sender que no produce valor (void)
    {
        auto sender = ex::just();
        ex::sync_wait(sender);
        std::println("just() completed (void)");
    }

    // 5. sync_wait retorna optional - puede fallar
    {
        auto sender = ex::just(100);
        if (auto result = ex::sync_wait(sender)) {
            auto [val] = *result;
            std::println("sync_wait returned: {}", val);
        }
    }

    std::println("\n=== Key Concepts ===");
    std::println("- Senders are lazy: nothing executes until sync_wait/start");
    std::println("- just() creates a sender that completes immediately with values");
    std::println("- sync_wait() blocks until the sender completes");

    return 0;
}
