#pragma once

#include <cstdint>

namespace simforge::uvm
{
    struct Transaction
    {
        int32_t time_offset = -1;
        uint64_t tid;

        virtual ~Transaction() = default;
    };

    struct InputData : Transaction
    {
    };

    struct OutputData : Transaction
    {
    };
}
