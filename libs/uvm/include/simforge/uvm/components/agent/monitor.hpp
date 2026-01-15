#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/components/scoreboard.hpp>
#include <simforge/uvm/transaction.hpp>
#include <simforge/uvm/tlm/tlm_analysis_port.hpp>
#include <simforge/uvm/signals/vif.hpp>

#include <stdexcept>

namespace simforge::uvm::components::agent
{
    class IMonitor : public Component
    {
    public:
        using Component::Component;
        virtual ~IMonitor() = default;

        virtual void set_vif(signals::Vif *vif) = 0;
    };

    template <typename T = Transaction, typename VIF = signals::Vif>
    class Monitor : public IMonitor
    {
    public:
        using Tx = std::shared_ptr<T>;

        explicit Monitor(Component *parent, std::string name = "MON")
            : IMonitor(name, parent)
        {
        }

        virtual void build_phase() override {}

        void run_phase(uint64_t sim_time) final override
        {
            if (!should_sample())
                return;

            logger()->debug("Sampling DUT signals...");
            auto data = sample();

            if (data == nullptr)
            {
                logger()->debug("Null data sampled");
                return;
            }

            analysis_port.write(data);
            logger()->debug("Transaction sent to subscribers");
        }

        virtual void report_phase() override {}

        virtual void on_reset() override {}

        void connect(tlm::ITLMAnalysisIf<Tx> *iface)
        {
            analysis_port.connect(iface);
        }

        void set_vif(signals::Vif *_vif) override
        {
            vif = dynamic_cast<const VIF *>(_vif);
            if (!vif)
                throw std::runtime_error("Wrong VIF type injected into " + name());
        }

    protected:
        virtual bool should_sample() const = 0;

        virtual Tx sample() = 0;

        tlm::TLMAnalysisPort<Tx> analysis_port;

        const VIF *vif = nullptr;
    };
}
