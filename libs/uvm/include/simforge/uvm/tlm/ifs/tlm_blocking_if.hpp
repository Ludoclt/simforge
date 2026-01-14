#pragma once

#include <memory>

namespace simforge::uvm::tlm
{
    template <typename T>
    class ITLMBlockingPutGetIf
    {
    public:
        virtual ~ITLMBlockingPutGetIf() = default;

        virtual void put(std::shared_ptr<T>) = 0;
        virtual std::shared_ptr<T> get() = 0;
        virtual std::shared_ptr<T> peek() const = 0;
    };
} // namespace simforge::uvm::tlm
