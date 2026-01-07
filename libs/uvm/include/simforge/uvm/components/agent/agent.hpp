#pragma once

#include <simforge/uvm/components/component.hpp>
#include "driver.hpp"
#include "sequencer.hpp"
#include "monitor.hpp"

namespace simforge::uvm::components::agent
{
    class Agent : public Component
    {
    public:
        explicit Agent(uint32_t reset_cycles, uint8_t verif_edge, Component *parent, std::string name) : reset_cycles(reset_cycles), verif_edge(verif_edge), Component(std::move(name), parent) {}

        void build_phase() override
        {
            if (!sequencer)
                throw std::runtime_error("No sequencer attached!");
            if (!driver)
                throw std::runtime_error("No driver attached!");
            if (!input_monitor)
                throw std::runtime_error("Input monitor not attached!");
            if (!output_monitor)
                throw std::runtime_error("Output monitor not attached!");

            sequencer->build_phase();
            driver->build_phase();
            input_monitor->build_phase();
            output_monitor->build_phase();
        }

        void connect_phase() override
        {
            sequencer->connect_phase();
            driver->connect_phase();
            input_monitor->connect_phase();
            output_monitor->connect_phase();
        }

        void run_phase() override
        {
            sequencer->next(*driver);

            input_monitor->run_phase();
            output_monitor->run_phase();
        }

        void report() override
        {
            sequencer->report();
            driver->report();
            input_monitor->report();
            output_monitor->report();
        }

        bool finished()
        {
            return sequencer->empty_sequence();
        }

        const uint32_t reset_cycles;
        const uint8_t verif_edge;

        Sequencer *sequencer = nullptr;
        Driver *driver = nullptr;
        Monitor *input_monitor = nullptr;
        Monitor *output_monitor = nullptr;
    };
}
