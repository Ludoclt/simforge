#pragma once

#include <memory>

template <typename T>
class ITLMNonBlockingPutGetIf
{
public:
    virtual ~ITLMNonBlockingPutGetIf() = default;

    virtual bool try_put(std::shared_ptr<T>) = 0;
    virtual bool try_get(std::shared_ptr<T> &) = 0;
    virtual bool try_peek(std::shared_ptr<T> &) const = 0;
};
