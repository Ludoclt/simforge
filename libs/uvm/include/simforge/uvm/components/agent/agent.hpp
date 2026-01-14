#pragma once

#include <simforge/uvm/components/component.hpp>
#include "driver.hpp"
#include "sequencer.hpp"
#include "monitor.hpp"

#include <vector>

namespace simforge::uvm::components::agent
{
    class Agent : public Component
    {
    public:
        explicit Agent(Component *parent, std::string name) : Component(std::move(name), parent) {}

        void build_phase() override
        {
            if (!sequencer)
                throw std::runtime_error("No sequencer attached!");
            if (!driver)
                throw std::runtime_error("No driver attached!");
            if (monitors.empty())
                throw std::runtime_error("No monitor attached!");

            for (auto *mon : monitors)
                if (mon == nullptr)
                    throw std::runtime_error("Null monitor detected!");

            sequencer->build_phase();
            driver->build_phase();

            for (auto *mon : monitors)
                mon->build_phase();
        }

        void connect_phase() override
        {
            driver->seq_port.connect(&sequencer->seq_export, &sequencer->seq_export);

            sequencer->connect_phase();
            driver->connect_phase();

            for (auto *mon : monitors)
                mon->connect_phase();
        }

        void run_phase(uint64_t sim_time) override {}

        void on_reset() override {}

        void report_phase() override
        {
            sequencer->report_phase();
            driver->report_phase();

            for (auto *mon : monitors)
                mon->report_phase();
        }

        Sequencer *sequencer = nullptr;
        Driver *driver = nullptr;

        std::vector<IMonitor *> monitors;
    };
}
