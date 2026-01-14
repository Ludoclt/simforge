#pragma once

#include "ifs/tlm_blocking_if.hpp"
#include "ifs/tlm_nonblocking_if.hpp"

namespace simforge::uvm::tlm
{
    template <typename T>
    class TLMPort
    {
    public:
        void connect(
            ITLMBlockingPutGetIf<T> *blk,
            ITLMNonBlockingPutGetIf<T> *nb)
        {
            blk_ = blk;
            nb_ = nb;
        }

        // Blocking
        void put(std::shared_ptr<T> item)
        {
            check();
            blk_->put(item);
        }

        std::shared_ptr<T> get()
        {
            check();
            return blk_->get();
        }

        std::shared_ptr<T> peek() const
        {
            check();
            return blk_->peek();
        }

        // Non-blocking
        bool try_put(std::shared_ptr<T> item)
        {
            check();
            return nb_->try_put(item);
        }

        bool try_get(std::shared_ptr<T> &item)
        {
            check();
            return nb_->try_get(item);
        }

        bool try_peek(std::shared_ptr<T> &item) const
        {
            check();
            return nb_->try_peek(item);
        }

    private:
        void check() const
        {
            if (!blk_ || !nb_)
                throw std::runtime_error("TLMPort not connected");
        }

        ITLMBlockingPutGetIf<T> *blk_ = nullptr;
        ITLMNonBlockingPutGetIf<T> *nb_ = nullptr;
    };
} // namespace simforge::uvm::tlm
