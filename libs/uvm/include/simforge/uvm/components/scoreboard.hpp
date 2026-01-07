#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/components/subscriber.hpp>
#include <simforge/uvm/transaction.hpp>

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

        void writeIn(std::shared_ptr<InputData> inData)
        {
            log_->debug("writeIn: push input transaction");
            in_q.push(std::move(inData));
        }

        void writeOut(std::shared_ptr<OutputData> outData);

        void build_phase() override {}
        void connect_phase() override {}
        void run_phase() override {}

        void report() override
        {
            log_->info("###### Scoreboard Report ######");
            for (auto *sub : subscribers)
            {
                log_->info("Subscriber '{}' report:", sub->name());
                sub->report();
            }
            log_->info("###### End Scoreboard Report ######");
        }

        std::vector<Subscriber *> subscribers;

    private:
        TLMQueue<InputData> in_q;
    };
}
