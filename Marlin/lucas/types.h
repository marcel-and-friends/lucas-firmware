#pragma once

#include <cstdint>
#include <cstddef>

namespace lucas {
using s8 = int8_t;
using s16 = int16_t;
using s32 = int;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = unsigned int;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = size_t;
}

static_assert(sizeof(int) == sizeof(std::int32_t), "int must be 32 bits");
