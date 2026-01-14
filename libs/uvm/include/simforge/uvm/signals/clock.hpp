#pragma once

#include <simforge/uvm/signals/signal_base.hpp>

namespace simforge::uvm::signals
{
    class ClockSignal : public SignalBase
    {
    public:
        ClockSignal(uint8_t *sig_ptr, uint64_t period = 2)
            : SignalBase(sig_ptr), period(period), half_period(period / 2)
        {
        }

        void toggle() { set(get() ^ 1); }
        uint8_t level() const { return get(); }

        void tick() { toggle(); }

        uint64_t get_period() const { return period; }

    private:
        uint64_t period;
        uint64_t half_period;
    };

    struct ClockDesc
    {
        uint8_t *sig;

        uint64_t period = 2;

        ClockDesc &with_period(uint64_t p)
        {
            period = p;
            return *this;
        }
    };

    inline ClockDesc clock(uint8_t *sig_ptr)
    {
        return {.sig = sig_ptr};
    }
} // namespace simforge::uvm::signals
