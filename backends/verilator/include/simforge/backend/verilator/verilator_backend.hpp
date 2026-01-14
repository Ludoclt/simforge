#pragma once

#include <simforge/uvm/backend.hpp>
#include "concepts.hpp"

#include <verilated.h>
#include <verilated_vcd_c.h>

#include <memory>
#include <filesystem>

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

            const std::string path = std::filesystem::canonical("/proc/self/exe").string() + "/waveform.vcd";

            trace->open(path.c_str());
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

        void bind_signal(const simforge::uvm::signals::ClockDesc &desc)
        {
            if (!dut)
                throw std::logic_error("bind_signal: DUT not created");

            _clk = std::make_unique<simforge::uvm::signals::ClockSignal>(
                desc.sig,
                desc.period);
        }

        void bind_signal(const simforge::uvm::signals::ResetDesc &desc)
        {
            if (!dut)
                throw std::logic_error("bind_signal: DUT not created");

            _rst = std::make_unique<simforge::uvm::signals::ResetSignal>(
                desc.sig,
                desc.active_lvl);
        }

        DUT &get_dut() const noexcept { return *dut; }

    private:
        std::unique_ptr<DUT> dut;
        std::unique_ptr<VerilatedVcdC> trace;

        uint64_t _time = 0;
    };
} // namespace simforge::backend::verilator
