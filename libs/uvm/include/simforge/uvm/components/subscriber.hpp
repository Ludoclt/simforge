#pragma once

#include <simforge/uvm/components/component.hpp>
#include <simforge/uvm/transaction.hpp>
#include <simforge/uvm/tlm/tlm_analysis_export.hpp>

#include <memory>
#include <cstdint>
#include <string>

namespace simforge::uvm::components
{
    template <typename T = Transaction>
    class Subscriber : public Component, public tlm::ITLMAnalysisIf<std::shared_ptr<T>>
    {
    public:
        using Rx = std::shared_ptr<T>;

        explicit Subscriber(Component *parent, std::string name) : Component(name, parent) {}

        virtual ~Subscriber() = default;
    };
}
