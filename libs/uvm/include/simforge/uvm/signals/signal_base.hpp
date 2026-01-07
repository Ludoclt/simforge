#pragma once

#include <cstdint>
#include <stdexcept>

namespace simforge::uvm::signals
{
    class SignalBase
    {
    public:
        explicit SignalBase(uint8_t *sig_ptr)
            : sig(sig_ptr)
        {
            if (!sig)
                throw std::runtime_error("SignalBase: null pointer to signal");
        }

        virtual ~SignalBase() = default;

        virtual void set(uint8_t val) { *sig = val; }
        virtual uint8_t get() const { return *sig; }

    protected:
        uint8_t *sig;
    };
} // namespace simforge::uvm::signals
