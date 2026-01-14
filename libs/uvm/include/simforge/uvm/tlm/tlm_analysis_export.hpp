#pragma once

#include "ifs/tlm_analysis_if.hpp"

namespace simforge::uvm::tlm
{
    template <typename T>
    class TLMAnalysisExport : public ITLMAnalysisIf<T>
    {
    public:
        explicit TLMAnalysisExport(ITLMAnalysisIf<T> *impl) : impl_(impl) {}

        void write(const T &item) override
        {
            impl_->write(item);
        }

    private:
        ITLMAnalysisIf<T> *impl_;
    };
} // namespace simforge::uvm::tlm
