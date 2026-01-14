#pragma once

#include "ifs/tlm_analysis_if.hpp"

#include <set>

namespace simforge::uvm::tlm
{
    template <typename T>
    class TLMAnalysisPort
    {
    public:
        void connect(ITLMAnalysisIf<T> *iface)
        {
            subs_.insert(iface);
        }

        void write(const T &item)
        {
            for (auto *s : subs_)
                s->write(item);
        }

    private:
        std::set<ITLMAnalysisIf<T> *> subs_;
    };
} // namespace simforge::uvm::tlm
