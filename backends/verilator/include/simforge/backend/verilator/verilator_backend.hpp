#pragma once

#include <simforge/uvm/backend.hpp>
#include "concepts.hpp"

#include <verilated.h>
#include <verilated_vcd_c.h>

#include <memory>

namespace simforge::backend::verilator
{
    template <VerilatedDUT DUT>
    class VerilatorBackend : public simforge::uvm::Backend
    {
    public:
        void pre_run() override
        {
        }

        void post_run() override
        {
        }

        void setup() override
        {
            Verilated::traceEverOn(true);
            trace = std::make_unique<VerilatedVcdC>();
        }

        void eval() override
        {
            dut->eval();
        }

        void tick() override
        {
            _time++;
        }

        uint64_t time() const override { return _time; }

        void start_trace(int level) override
        {
            dut->trace(trace.get(), level);
            trace->open("waveform.vcd");
        }

        void stop_trace() override
        {
            trace->close();
        }

        void dump_trace() override
        {
            trace->dump(_time);
        }

        template <typename... Args>
        void create_dut(Args &&...args)
        {
            dut = std::make_unique<DUT>(std::forward<Args>(args)...);
        }

        DUT &get_dut() const noexcept { return dut; }

    private:
        std::unique_ptr<DUT> dut;
        std::unique_ptr<VerilatedVcdC> trace;

        uint64_t _time = 0;
    };
} // namespace simforge::backend::verilator
