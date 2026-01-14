#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>
#include <simforge/uvm/tlm/tlm_port.hpp>

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

        virtual void build_phase() override {}
        virtual void connect_phase() override {}
        virtual void report_phase() override {}

        virtual void drive(std::shared_ptr<InputData> data) = 0;

        tlm::TLMPort<InputData> seq_port;
    };
}
