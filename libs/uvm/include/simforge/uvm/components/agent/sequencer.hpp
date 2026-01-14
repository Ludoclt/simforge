#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>
#include <simforge/uvm/tlm/tlm_fifo.hpp>
#include <simforge/uvm/tlm/tlm_export.hpp>

#include <random>

namespace simforge::uvm::components::agent
{
    class Sequencer : public Component
    {
    public:
        explicit Sequencer(Component *parent, std::string name = "SEQ") noexcept
            : Component(name, parent) {}

        virtual ~Sequencer() = default;

        virtual void connect_phase() override {}
        virtual void run_phase(uint64_t sim_time) override {}
        virtual void report_phase() override {}

        tlm::TLMExport<InputData> seq_export{&sequence, &sequence};

    protected:
        tlm::TLMFifo<InputData> sequence;

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
