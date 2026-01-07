#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/signals/signals.hpp>
#include <simforge/uvm/components/agent/agent.hpp>
#include <simforge/uvm/components/scoreboard.hpp>
#include <simforge/uvm/backend.hpp>

#include <memory>

namespace simforge::uvm::components
{
    class Env : public Component
    {
    public:
        Env(Backend &backend,
            std::string name = "TB",
            int trace_level = 3)
            : Component(name, nullptr),
              backend(backend),
              trace_level(trace_level),
              scb(Scoreboard(this))
        {
        }

        template <typename T, typename... Args>
        T &emplace_component(Args &&...args)
        {
            auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
            ptr->parent_ = this;
            T &ref = *ptr;

            if (auto *comp = dynamic_cast<agent::Agent *>(ptr.get()))
                agents.push_back(comp);

            components.push_back(std::move(ptr));
            return ref;
        }

        void build_phase() override
        {
            spdlog::set_level(spdlog::level::debug);
            spdlog::set_pattern("[%^[%n]%$] %v");

            log_->info("Setup start");

            backend.setup();

            backend.start_trace(trace_level);

            log_->info("Trace opened (level {})", trace_level);

            for (auto *agent : agents)
                agent->build_phase();

            log_->info("Build phase completed");
        }

        void connect_phase() override
        {
            log_->info("Connect phase");

            for (auto *agent : agents)
                agent->connect_phase();

            log_->info("Connect phase completed");
        }

        void run_phase() override
        {
            log_->info("Simulation started");

            for (int i = 0; i < agents.size(); i++)
            {
                log_->info("Agent number {}", i);

                auto *agent = agents.at(i);

                if (agent->reset_cycles > 0)
                    pulseReset(agent->reset_cycles);

                while (!agent->finished())
                {
                    // Tick clock and evaluate DUT
                    backend.clk().toggle();
                    backend.eval();

                    // Sequence driving phase
                    if (!backend.rst().is_asserted() && backend.clk().level() == agent->verif_edge)
                        agent->run_phase();

                    backend.dump_trace();
                    backend.incr_time();
                }
            }

            backend.stop_trace();
            log_->info("Trace closed. Simulation finished at t={}", sim_time());

            scb.report();
        }

        uint64_t sim_time() const { return backend.time(); }

        int trace_level;

        Backend &backend;

        std::vector<std::unique_ptr<Component>> components;
        std::vector<agent::Agent *> agents;

        Scoreboard scb;

        void pulseReset(uint32_t cycles)
        {
            log_->info("Applying reset for {} cycles (active_{})",
                       cycles, backend.rst().polarity() ? "high" : "low");
            backend.rst().assert_reset();
            for (uint32_t i = 0; i < cycles; ++i)
            {
                backend.clk().toggle();
                backend.eval();
                backend.dump_trace();
                backend.incr_time();
            }
            backend.rst().release();
            backend.eval();
            backend.dump_trace();
            backend.incr_time();
            log_->info("Reset released at t={}", sim_time());
        }
    };
}
