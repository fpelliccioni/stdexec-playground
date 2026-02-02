// 02_then_chain.cpp
// Encadenamiento de operaciones con then()

#include <stdexec/execution.hpp>
#include <print>
#include <string>

namespace ex = stdexec;

int main() {
    std::println("=== Then Chain Examples ===\n");

    // 1. Encadenamiento simple
    {
        auto result = ex::sync_wait(
            ex::just(5)
            | ex::then([](int x) { return x * 2; })
            | ex::then([](int x) { return x + 1; })
        ).value();

        std::println("5 * 2 + 1 = {}", std::get<0>(result));
    }

    // 2. Transformación de tipos
    {
        auto result = ex::sync_wait(
            ex::just(42)
            | ex::then([](int x) { return std::to_string(x); })
            | ex::then([](std::string s) { return "Value: " + s; })
        ).value();

        std::println("{}", std::get<0>(result));
    }

    // 3. Múltiples valores de entrada
    {
        auto result = ex::sync_wait(
            ex::just(10, 20)
            | ex::then([](int a, int b) { return a + b; })
        ).value();

        std::println("10 + 20 = {}", std::get<0>(result));
    }

    // 4. Retornar múltiples valores
    {
        auto result = ex::sync_wait(
            ex::just(100)
            | ex::then([](int x) { return std::make_tuple(x, x * 2, x * 3); })
            | ex::then([](auto tuple) {
                auto [a, b, c] = tuple;
                return a + b + c;
            })
        ).value();

        std::println("100 + 200 + 300 = {}", std::get<0>(result));
    }

    // 5. Chain largo - pipeline de procesamiento
    {
        auto pipeline =
            ex::just(std::string{"  HELLO WORLD  "})
            | ex::then([](std::string s) {
                // trim leading spaces
                auto start = s.find_first_not_of(' ');
                return start != std::string::npos ? s.substr(start) : "";
            })
            | ex::then([](std::string s) {
                // trim trailing spaces
                auto end = s.find_last_not_of(' ');
                return end != std::string::npos ? s.substr(0, end + 1) : "";
            })
            | ex::then([](std::string s) {
                // to lowercase
                for (char& c : s) c = static_cast<char>(std::tolower(c));
                return s;
            })
            | ex::then([](std::string s) {
                return "processed: [" + s + "]";
            });

        auto [result] = ex::sync_wait(pipeline).value();
        std::println("{}", result);
    }

    // 6. Side effects con then (retornando void)
    {
        int side_effect = 0;

        ex::sync_wait(
            ex::just(42)
            | ex::then([&](int x) {
                side_effect = x;
                // no return = void
            })
        );

        std::println("Side effect captured: {}", side_effect);
    }

    std::println("\n=== Key Concepts ===");
    std::println("- then() transforms the value(s) from a sender");
    std::println("- Chains are composed with | operator (pipe)");
    std::println("- Type transformations happen naturally");
    std::println("- Everything is still lazy until sync_wait");

    return 0;
}
