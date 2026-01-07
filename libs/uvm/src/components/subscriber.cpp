#include <simforge/uvm/components/subscriber.hpp>
#include <simforge/uvm/components/env.hpp>

uint64_t simforge::uvm::components::Subscriber::_sim_time()
{
    return env().sim_time();
}
