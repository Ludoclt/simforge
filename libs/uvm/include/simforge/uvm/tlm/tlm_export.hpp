#pragma once

#include "ifs/tlm_blocking_if.hpp"
#include "ifs/tlm_nonblocking_if.hpp"

#include <stdexcept>

namespace simforge::uvm::tlm
{
    template <typename T>
    class TLMExport : public ITLMBlockingPutGetIf<T>,
                      public ITLMNonBlockingPutGetIf<T>
    {
    public:
        TLMExport(
            ITLMBlockingPutGetIf<T> *blk,
            ITLMNonBlockingPutGetIf<T> *nb) : blk_(blk), nb_(nb)
        {
            if (!blk_ || !nb_)
                throw std::runtime_error("Invalid TLMExport binding");
        }

        // Blocking
        void put(std::shared_ptr<T> item) override
        {
            blk_->put(item);
        }

        std::shared_ptr<T> get() override
        {
            return blk_->get();
        }

        std::shared_ptr<T> peek() const override
        {
            return blk_->peek();
        }

        // Non-blocking
        bool try_put(std::shared_ptr<T> item) override
        {
            return nb_->try_put(item);
        }

        bool try_get(std::shared_ptr<T> &item) override
        {
            return nb_->try_get(item);
        }

        bool try_peek(std::shared_ptr<T> &item) const override
        {
            return nb_->try_peek(item);
        }

    private:
        ITLMBlockingPutGetIf<T> *blk_;
        ITLMNonBlockingPutGetIf<T> *nb_;
    };
} // namespace simforge::uvm::tlm
