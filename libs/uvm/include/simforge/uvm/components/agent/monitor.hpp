#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/components/scoreboard.hpp>
#include <simforge/uvm/transaction.hpp>

#include <stdexcept>

namespace simforge::uvm::components::agent
{
    class Monitor : public Component
    {
    public:
        explicit Monitor(Component *parent, std::string name = "MON")
            : Component(name, parent)
        {
        }

        virtual ~Monitor() = default;

        void run_phase() override
        {
            if (!scb)
                throw std::runtime_error("PANIC: The monitor is not connected to a Scoreboard!");

            log_->debug("Sampling DUT signals...");
            auto data = sample();

            if (data == nullptr)
            {
                log_->debug("Null data sampled");
                return;
            }

            if (data->isInput())
            {
                auto in = std::dynamic_pointer_cast<InputData>(data);
                if (!in)
                {
                    log_->error("Monitor returned a transaction marked as input but cast failed!");
                    throw std::runtime_error("Bad transaction type");
                }
                scb->writeIn(in);
            }
            else
            {
                auto out = std::dynamic_pointer_cast<OutputData>(data);
                if (!out)
                {
                    log_->error("Monitor returned a transaction marked as output but cast failed!");
                    throw std::runtime_error("Bad transaction type");
                }
                scb->writeOut(out);
            }

            log_->debug("Transaction sent to Scoreboard.");
        }

    protected:
        Scoreboard *scb = nullptr;

        virtual std::shared_ptr<Transaction> sample() = 0;
    };
}
