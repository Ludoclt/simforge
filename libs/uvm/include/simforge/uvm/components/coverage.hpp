#pragma once

#include <simforge/uvm/components/component.hpp>

#include <map>
#include <ranges>

namespace simforge::uvm::components
{
    class Coverage : public Component
    {
    public:
        struct CoverageBin
        {
            uint64_t hits = 0;
            uint64_t goal = 1;
        };

        using CoverageMap = std::map<std::string, CoverageBin>;

        explicit Coverage(Component *parent, float goal = 95.0, std::string name = "COV") : Component(name, parent), _goal(goal)
        {
        }

        virtual void connect_phase() override {}
        virtual void run_phase(uint64_t sim_time) override {}

        void report_phase() override
        {
            logger()->info("");
            logger()->info("========================================");
            logger()->info("         {} FUNCTIONAL COVERAGE         ", name_);
            logger()->info("========================================");

            uint64_t covered = 0;
            uint64_t total = coverage_bins.size();

            for (const auto &[name, bin] : std::views::reverse(coverage_bins))
            {
                if (bin.hits == 0)
                {
                    logger()->warn("UNCOVERED: {}", name);
                    continue;
                }

                double pct = percent(bin.hits, bin.goal);
                bool done = pct >= 100.0;

                if (done)
                    covered++;

                logger()->info(
                    "{:<35} {} {:6.2f}%",
                    name,
                    progress_bar(pct / 100.0),
                    pct);
            }

            double global = total ? (100.0 * covered / total) : 100.0;

            logger()->info("----------------------------------------");
            logger()->info(
                "TOTAL COVERAGE: {}/{} bins ({:.2f}%)",
                covered,
                total,
                global);

            logger()->info("========================================");

            if (global < _goal)
                throw std::runtime_error("Coverage goal not met");
        }

        virtual void on_reset() override {}

    protected:
        CoverageMap coverage_bins;

        static std::string progress_bar(double ratio, int width = 30)
        {
            int filled = static_cast<int>(ratio * width);
            std::string bar = "[";

            bar += std::string(filled, '#');
            bar += std::string(width - filled, '-');
            bar += "]";

            return bar;
        }

        static double percent(uint64_t hit, uint64_t goal)
        {
            if (goal == 0)
                return 100.0;
            return std::min(100.0, 100.0 * hit / goal);
        }

    private:
        float _goal;
    };
}
