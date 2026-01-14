#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/components/env.hpp>
#include <simforge/uvm/runner.hpp>

namespace sfuvm = simforge::uvm;
namespace sfuvmc = simforge::uvm::components;
namespace sfuvmsig = simforge::uvm::signals;

using BaseRunner = sfuvm::Runner<sfuvm::Backend, sfuvmc::Env>;

sfuvmc::Env &sfuvmc::Component::env()
{
    Component *p = this;
    while (p->parent_)
        p = p->parent_;
    return static_cast<Env &>(*p);
}

sfuvmsig::ClockSignal &sfuvmc::Component::clk() const
{
    return BaseRunner::current().backend().clk();
}

sfuvmsig::ResetSignal &sfuvmc::Component::rst() const
{
    return BaseRunner::current().backend().rst();
}

void sfuvmc::Component::raise_objection()
{
    BaseRunner::current().raise_objection(this);
}

void sfuvmc::Component::drop_objection()
{
    BaseRunner::current().drop_objection(this);
}
