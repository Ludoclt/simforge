#pragma once

#include <simforge/uvm/signals/signals.hpp>

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace simforge::uvm::components
{
    class Component
    {
    public:
        explicit Component(std::string name = "COMP", Component *parent = nullptr) : name_(std::move(name)), parent_(parent)
        {
            log_ = spdlog::get(name_);
            if (!log_)
                log_ = spdlog::stdout_color_mt(name_);
        }

        virtual ~Component() = default;

        const std::string &name() const { return name_; }

        virtual void build_phase() = 0;
        virtual void connect_phase() = 0;
        virtual void run_phase(uint64_t sim_time) = 0;
        virtual void report_phase() = 0;

        virtual void on_reset() = 0;

        Component *parent_ = nullptr;

    protected:
        std::string name_;
        std::shared_ptr<spdlog::logger> log_;

        class Env &env();

        virtual signals::ClockSignal &clk() const;
        virtual signals::ResetSignal &rst() const;

        virtual void raise_objection();
        virtual void drop_objection();
    };
}
