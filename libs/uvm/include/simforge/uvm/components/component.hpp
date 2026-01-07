#pragma once

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

        virtual void build_phase() = 0;
        virtual void connect_phase() = 0;
        virtual void run_phase() = 0;
        virtual void report() = 0;

        const std::string &name() const { return name_; }

        Component *parent_ = nullptr;

    protected:
        std::string name_;
        std::shared_ptr<spdlog::logger> log_;

        class Env &env();
    };
}
