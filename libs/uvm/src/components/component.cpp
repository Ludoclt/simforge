#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/components/env.hpp>

simforge::uvm::components::Env &simforge::uvm::components::Component::env()
{
    Component *p = this;
    while (p->parent_)
        p = p->parent_;
    return static_cast<Env &>(*p);
}
