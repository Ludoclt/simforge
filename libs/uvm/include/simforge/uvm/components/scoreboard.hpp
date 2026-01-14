#pragma once

#include <simforge/uvm/components/component.hpp>

#include <vector>
#include <stdexcept>

namespace simforge::uvm::components
{
    class Scoreboard : public Component
    {
    public:
        explicit Scoreboard(Component *parent, std::string name = "SCB") : Component(name, parent)
        {
        }

        virtual void build_phase() override {}
        virtual void connect_phase() override {}
        virtual void run_phase(uint64_t sim_time) override {}

        virtual void report_phase() override {}

        virtual void on_reset() override {}

    protected:
        template <typename I, typename O>
        void displayValueMismatch(const I &data_in, const O &expected_out, const O &actual_out, uint64_t sim_time)
        {
            log_->info(R"(
                {} Subscriber: value mismatch
                    Input: {}
                    Expected Output: {}
                    Actual Output: {}
                    Simtime: {}
                )",
                       name(), data_in, expected_out, actual_out, sim_time);
        }
    };
}
