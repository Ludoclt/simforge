#include <simforge/uvm/components/scoreboard.hpp>
#include <simforge/uvm/components/env.hpp>

void simforge::uvm::components::Scoreboard::writeOut(std::shared_ptr<OutputData> outData)
{
    if (in_q.empty())
        throw std::runtime_error("PANIC: Empty input data queue in Scoreboard");

    auto in_data = in_q.pop();

    log_->info("writeOut: dispatching to {} subscribers", subscribers.size());

    for (auto *sub : subscribers)
    {
        sub->write(in_data, outData);
        log_->debug("Scoreboard: output data registered in '{}' Subscriber at time {}", sub->name(), env().sim_time());
    }
}
