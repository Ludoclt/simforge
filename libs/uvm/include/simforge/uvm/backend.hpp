#pragma once

#include <cstdint>
#include <memory>

#include <simforge/uvm/signals/signals.hpp>

namespace simforge::uvm
{
    class Backend
    {
    public:
        virtual ~Backend() = default;

        virtual void pre_run() = 0;
        virtual void post_run() = 0;

        virtual void setup() = 0;
        virtual void eval() = 0;

        virtual void tick() = 0;
        virtual uint64_t time() const = 0;

        virtual void start_trace(int level) = 0;
        virtual void stop_trace() = 0;
        virtual void dump_trace() = 0;

        signals::ClockSignal &clk() const { return *_clk; }
        signals::ResetSignal &rst() const { return *_rst; }

    protected:
        std::unique_ptr<signals::ClockSignal> _clk;
        std::unique_ptr<signals::ResetSignal> _rst;
    };

} // namespace simforge::uvm
