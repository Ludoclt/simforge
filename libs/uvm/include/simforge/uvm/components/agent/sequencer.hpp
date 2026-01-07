#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>
#include "driver.hpp"

#include <random>

namespace simforge::uvm::components::agent
{
    class Sequencer : public Component
    {
    public:
        explicit Sequencer(Component *parent, std::string name = "SEQ") noexcept
            : Component(name, parent) {}

        virtual ~Sequencer() = default;

        void next(Driver &driver);

        bool empty_sequence()
        {
            return sequence.empty();
        }

    protected:
        TLMQueue<InputData> sequence;

        uint32_t random_stimuli_range(uint32_t start, uint16_t end)
        {
            static thread_local std::random_device dev;
            static thread_local std::mt19937 rng(dev());
            std::uniform_int_distribution<int> dist(start, end);
            return static_cast<uint32_t>(dist(rng));
        }

        uint32_t random_stimuli(uint16_t bit_length)
        {
            return bit_length >= 32 ? 0xFFFFFFFFu : random_stimuli_range(0, (1u << bit_length) - 1u);
        }
    };
}
