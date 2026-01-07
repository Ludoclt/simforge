#include <simforge/uvm/backend.hpp>
#include <simforge/uvm/components/env.hpp>

#include <stdexcept>

namespace simforge::uvm
{
    template <typename B = Backend, typename E = components::Env>
    class Runner
    {
    public:
        template <typename... Args>
        explicit Runner(Args &&...args)
            : backend_(), env_(backend_, std::forward<Args>(args)...)
        {
            if (tls_current)
                throw std::runtime_error("Runner already exists in this thread");
            tls_current = this;
        }

        ~Runner()
        {
            if (tls_current != this)
                throw std::runtime_error("Destroying non-current Runner");

            tls_current = nullptr;
        }

        void run()
        {
            if (has_run_)
                throw std::runtime_error("Runner::run() called twice");

            has_run_ = true;

            backend_.pre_run();
            env_.build_phase();
            env_.connect_phase();
            env_.run_phase();
            backend_.post_run();
        }

        B &backend() noexcept { return backend_; }
        E &env() noexcept { return env_; }

        static Runner &current()
        {
            if (!tls_current)
                throw std::runtime_error("No active Runner in this thread");

            return *static_cast<Runner *>(tls_current);
        }

        template <typename B, typename E>
        static Runner<B, E> &current_as()
        {
            return static_cast<Runner<B, E> &>(current());
        }

        static bool has_current() noexcept { return tls_current != nullptr; }

    private:
        B backend_;
        E env_;

        static inline thread_local void *tls_current = nullptr;

        bool has_run_ = false;
    };

} // namespace simforge::uvm
