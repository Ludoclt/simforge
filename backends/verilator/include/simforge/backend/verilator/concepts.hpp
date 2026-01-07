#pragma once

#include <concepts>
#include <ostream>
#include <verilated.h>

namespace simforge::backend::verilator
{
    template <typename T>
    concept VerilatedDUT =
        std::derived_from<T, VerilatedModel> &&
        requires(T t, VerilatedTraceBaseC *tfp, int lvl) {
            { t.eval() } -> std::same_as<void>;
            { t.trace(tfp, lvl) } -> std::same_as<void>;
        };
}
