#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>

#include <memory>
#include <cstdint>
#include <string>

namespace simforge::uvm::components
{
    class Subscriber : public Component
    {
    public:
        explicit Subscriber(Component *parent, std::string name) : Component(name, parent) {}

        virtual ~Subscriber() = default;

        void write(std::shared_ptr<InputData> inData, std::shared_ptr<OutputData> outData)
        {
            input_data.push(std::move(inData));
            output_data.push(std::move(outData));

            run_phase();
        }

    protected:
        TLMQueue<InputData> input_data;
        TLMQueue<OutputData> output_data;

        template <typename I, typename O>
        void displayValueMismatch(const I &data_in, const O &expected_out, const O &actual_out)
        {
            log_->info(R"(
                {} Subscriber: value mismatch
                    Input: {}
                    Expected Output: {}
                    Actual Output: {}
                    Simtime: {}
                )",
                       name(), data_in, expected_out, actual_out, _sim_time());
        }

    private:
        uint64_t _sim_time();
    };
}
