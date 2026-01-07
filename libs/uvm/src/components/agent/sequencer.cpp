#include <simforge/uvm/components/agent/sequencer.hpp>
#include <simforge/uvm/components/env.hpp>

void simforge::uvm::components::agent::Sequencer::next(simforge::uvm::components::agent::Driver &driver)
{
    auto peek = sequence.peek_wait();
    if (peek->time_offset <= 0)
    {
        log_->debug("Driving next transaction at t={}", env().sim_time());
        driver.drive(std::move(sequence.pop()));
    }
    else
        peek->time_offset--;
}
