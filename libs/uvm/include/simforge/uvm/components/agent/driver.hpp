#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>
#include <simforge/uvm/tlm/tlm_port.hpp>
#include <simforge/uvm/signals/vif.hpp>

namespace simforge::uvm::components::agent
{
    class IDriver : public Component
    {
    public:
        using Component::Component;
        virtual ~IDriver() = default;

        virtual void set_vif(signals::Vif *vif) = 0;

        tlm::TLMPort<InputData> seq_port;
    };

    template <typename VIF = signals::Vif>
    class Driver : public IDriver
    {
    public:
        explicit Driver(Component *parent, std::string name = "DRV")
            : IDriver(name, parent)
        {
        }

        virtual ~Driver() = default;

        virtual void build_phase() override {}
        virtual void connect_phase() override {}

        void run_phase(uint64_t sim_time) final override
        {
            if (rst().is_asserted())
            {
                raise_objection();
            }

            if (should_drive())
            {
                std::shared_ptr<InputData> peek;
                if (seq_port.try_peek(peek))
                {
                    raise_objection();

                    if (peek->time_offset <= 0)
                    {
                        logger()->debug("Driving next transaction at t={}", sim_time);
                        drive(std::move(seq_port.get()));
                    }
                    else
                    {
                        peek->time_offset--;
                        on_packet_waiting();
                    }
                }
                else
                    drop_objection();
            }
        }

        virtual void report_phase() override {}

        virtual void drive(std::shared_ptr<InputData> data) = 0;

        void set_vif(signals::Vif *_vif) override
        {
            vif = dynamic_cast<const VIF *>(_vif);
            if (!vif)
                throw std::runtime_error("Wrong VIF type injected into " + name());
        }

    protected:
        virtual bool should_drive() const = 0;
        virtual void on_packet_waiting() const = 0;

        const VIF *vif = nullptr;
    };
}
