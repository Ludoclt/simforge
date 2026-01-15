#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/signals/signals.hpp>
#include <simforge/uvm/components/agent/agent.hpp>
#include <simforge/uvm/components/scoreboard.hpp>
#include <simforge/uvm/components/coverage.hpp>
#include <simforge/uvm/backend.hpp>

#include <memory>

namespace simforge::uvm::components
{
    class Env : public Component
    {
    public:
        Env(Backend &backend,
            std::string name = "TB",
            int trace_level = 3,
            int reset_cycles = 3)
            : Component(name, nullptr),
              backend(backend),
              trace_level(trace_level),
              reset_cycles(reset_cycles)
        {
        }

        template <typename T, typename... Args>
        T &emplace_component(Args &&...args)
        {
            auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
            ptr->parent_ = this;
            T &ref = *ptr;

            if (auto *comp = dynamic_cast<agent::IAgent *>(ptr.get()))
                agents.push_back(comp);

            if (auto *comp = dynamic_cast<Scoreboard *>(ptr.get()))
                scb = comp;

            if (auto *comp = dynamic_cast<Coverage *>(ptr.get()))
                cov = comp;

            components.push_back(std::move(ptr));
            return ref;
        }

        void build_phase() override
        {
            logger()->debug("Setup start");

            backend.start_trace(trace_level);

            logger()->info("Trace opened (level {})", trace_level);

            if (!scb)
                throw std::runtime_error("No Scoreboard instance in " + name_);

            if (!cov)
                throw std::runtime_error("No Coverage instance in " + name_);

            for (auto *agent : agents)
                agent->build_phase();

            scb->build_phase();
            cov->build_phase();

            logger()->debug("Build phase completed");
        }

        void connect_phase() override
        {
            logger()->debug("Connect phase");

            for (auto *agent : agents)
                agent->connect_phase();

            scb->connect_phase();
            cov->connect_phase();

            logger()->debug("Connect phase completed");
        }

        void run_phase(uint64_t sim_time) override
        {
            if (reset_cycles > 0)
            {
                raise_objection();

                logger()->info("Applying reset at time {} (active_{})",
                               sim_time, backend.rst().polarity() ? "high" : "low");

                backend.rst().assert_reset();

                reset_cycles--;
            }
            else
            {
                if (backend.rst().is_asserted())
                    logger()->info("Reset released at t={}", sim_time);

                backend.rst().release();

                drop_objection();
            }

            if (backend.rst().is_asserted())
                notify_reset();
        }

        void report_phase() override
        {
            for (auto *agent : agents)
                agent->report_phase();

            scb->report_phase();
            cov->report_phase();
        }

        void notify_reset()
        {
            for (auto &comp : components)
                comp->on_reset();
        }

        int trace_level;

        uint32_t reset_cycles;

        Backend &backend;

        std::vector<std::unique_ptr<Component>> components;
        std::vector<agent::IAgent *> agents;

        template <typename S = Scoreboard>
        S &get_scb() { return *dynamic_cast<S *>(scb); }

        template <typename C = Coverage>
        C &get_cov() { return *dynamic_cast<C *>(cov); }

    private:
        Scoreboard *scb;
        Coverage *cov;
    };
}
