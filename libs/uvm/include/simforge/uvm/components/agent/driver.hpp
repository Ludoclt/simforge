#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>

namespace simforge::uvm::components::agent
{
    class Driver : public Component
    {
    public:
        explicit Driver(Component *parent, std::string name = "DRV")
            : Component(name, parent)
        {
        }
        virtual ~Driver() = default;

        virtual void drive(std::shared_ptr<InputData> data) = 0;
    };
}
