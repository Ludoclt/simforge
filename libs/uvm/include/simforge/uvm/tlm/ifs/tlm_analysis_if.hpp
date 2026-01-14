#pragma once

namespace simforge::uvm::tlm
{
    template <typename T>
    class ITLMAnalysisIf
    {
    public:
        virtual ~ITLMAnalysisIf() = default;
        virtual void write(const T &item) = 0;
    };
} // namespace simforge::uvm::tlm
