#pragma once

#include "ifs/tlm_blocking_if.hpp"
#include "ifs/tlm_nonblocking_if.hpp"

#include <queue>
#include <mutex>
#include <condition_variable>

namespace simforge::uvm::tlm
{
    template <typename T>
    class TLMFifo : public ITLMBlockingPutGetIf<T>,
                    public ITLMNonBlockingPutGetIf<T>
    {
    public:
        void put(std::shared_ptr<T> item) override
        {
            std::unique_lock lock(m_);
            q_.push(item);
            cv_.notify_one();
        }

        std::shared_ptr<T> get() override
        {
            std::unique_lock lock(m_);
            cv_.wait(lock, [&]
                     { return !q_.empty(); });
            auto v = q_.front();
            q_.pop();
            return v;
        }

        std::shared_ptr<T> peek() const override
        {
            std::lock_guard lock(m_);
            if (q_.empty())
                throw std::runtime_error("FIFO empty");
            return q_.front();
        }

        bool try_put(std::shared_ptr<T> item) override
        {
            std::lock_guard lock(m_);
            q_.push(item);
            return true;
        }

        bool try_get(std::shared_ptr<T> &item) override
        {
            std::lock_guard lock(m_);
            if (q_.empty())
                return false;
            item = q_.front();
            q_.pop();
            return true;
        }

        bool try_peek(std::shared_ptr<T> &item) const override
        {
            std::lock_guard lock(m_);
            if (q_.empty())
                return false;
            item = q_.front();
            return true;
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_);
            return q_.empty();
        }

    private:
        mutable std::mutex m_;
        std::condition_variable cv_;
        std::queue<std::shared_ptr<T>> q_;
    };
}
