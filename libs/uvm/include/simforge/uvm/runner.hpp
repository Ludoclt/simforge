#include <simforge/uvm/backend.hpp>
#include <simforge/uvm/components/env.hpp>

#include <simforge/uvm/runner_context.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <stdexcept>
#include <set>

namespace simforge::uvm
{
    template <typename B = Backend, typename E = components::Env>
    class Runner
    {
    public:
        template <typename... Args>
        explicit Runner(spdlog::level::level_enum log_level, Args &&...args)
            : backend_(std::make_unique<B>()), env_(std::make_unique<E>(*backend_, std::forward<Args>(args)...))
        {
            if (tls_current)
                throw std::runtime_error("Runner already exists in this thread");
            tls_current = this;

            spdlog::set_level(log_level);
            spdlog::set_pattern("[%^[%n]%$] %v");

            const std::string name = "RUNNER";
            log_ = spdlog::get(name);
            if (!log_)
                log_ = spdlog::stdout_color_mt(name);
        }

        ~Runner()
        {
            if (tls_current == this)
                tls_current = nullptr;
        }

        void run()
        {
            if (has_run_)
                throw std::runtime_error("Runner::run() called twice");

            has_run_ = true;

            backend_->setup();

            backend_->pre_run();

            env_->build_phase();

            // TODO bottom-up
            env_->connect_phase();

            log_->info("Simulation started");
            do
            {
                backend_->clk().toggle();
                backend_->eval();

                env_->run_phase(backend_->time());
                for (auto &c : env_->components)
                    c->run_phase(backend_->time());

                backend_->dump_trace();
                backend_->tick();

            } while (!objections.empty());

            backend_->stop_trace();
            log_->info("Trace closed. Simulation finished at t={}", backend_->time());

            env_->report_phase();

            backend_->post_run();
        }

        B &backend() noexcept { return *backend_; }
        E &env() noexcept { return *env_; }

        static Runner &current()
        {
            if (!tls_current)
                throw std::runtime_error("No active Runner in this thread");

            return *static_cast<Runner *>(tls_current);
        }

        static bool has_current() noexcept { return tls_current != nullptr; }

        void raise_objection(components::Component *c)
        {
            objections.insert(c);
        }

        void drop_objection(components::Component *c)
        {
            objections.erase(c);
        }

    private:
        std::unique_ptr<B> backend_;
        std::unique_ptr<E> env_;

        std::set<components::Component *> objections;

        bool has_run_ = false;

        std::shared_ptr<spdlog::logger> log_;
    };

} // namespace simforge::uvm
