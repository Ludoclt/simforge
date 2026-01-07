#pragma once

#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace simforge::uvm
{
    enum class Direction
    {
        Input,
        Output
    };

    struct Transaction
    {
        int32_t time_offset = -1;
        virtual ~Transaction() = default;
        virtual Direction direction() const = 0;
        bool isInput() const { return direction() == Direction::Input; }
        bool isOutput() const { return direction() == Direction::Output; }
    };

    struct InputData : Transaction
    {
        Direction direction() const override { return Direction::Input; }
    };

    struct OutputData : Transaction
    {
        Direction direction() const override { return Direction::Output; }
    };

    template <typename T>
    class TLMQueue
    {
    public:
        void push(std::shared_ptr<T> item)
        {
            std::lock_guard<std::mutex> lock(m_);
            q_.push(item);
            cv_.notify_one();
        }

        std::shared_ptr<T> peek() const
        {
            std::lock_guard<std::mutex> lock(m_);
            if (q_.empty())
                return nullptr;
            return q_.front();
        }

        std::shared_ptr<T> peek_wait()
        {
            std::unique_lock<std::mutex> lock(m_);
            cv_.wait(lock, [&]
                     { return !q_.empty(); });
            return q_.front();
        }

        std::shared_ptr<T> pop()
        {
            std::unique_lock<std::mutex> lock(m_);
            cv_.wait(lock, [&]
                     { return !q_.empty(); });
            auto item = q_.front();
            q_.pop();
            return item;
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(m_);
            return q_.size();
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
