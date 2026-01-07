#pragma once

#include <simforge/uvm/signals/signal_base.hpp>

namespace simforge::uvm::signals
{
    class ResetSignal : public SignalBase
    {
    public:
        ResetSignal(uint8_t *sig_ptr, bool active_high = true)
            : SignalBase(sig_ptr), active_high(active_high)
        {
        }

        void assert_reset() { set(active_high ? 1 : 0); }
        void release() { set(active_high ? 0 : 1); }

        bool is_asserted() const { return active_high ? get() : !get(); }
        bool polarity() const { return active_high; }

    private:
        bool active_high;
    };
} // namespace simforge::uvm::signals
