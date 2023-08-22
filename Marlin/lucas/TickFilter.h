#pragma once

#include <cstdint>

namespace lucas {
enum class TickFilter : uint32_t {
    None = 0,

    RecipeQueue = 1 << 0,
    Station = 1 << 1,
    Spout = 1 << 2,
    Info = 1 << 3,
    SerialHooks = 1 << 4,
    Boiler = 1 << 5,

    Interaction = RecipeQueue | Station | SerialHooks,
    All = RecipeQueue | Station | Spout | Info | SerialHooks | Boiler,
};

constexpr TickFilter operator|(const TickFilter a, const TickFilter b) {
    return TickFilter(uint32_t(a) | uint32_t(b));
}

constexpr TickFilter operator&(const TickFilter a, const TickFilter b) {
    return TickFilter(uint32_t(a) & uint32_t(b));
}

constexpr TickFilter operator~(const TickFilter a) {
    return TickFilter(~uint32_t(a));
}

void apply_filters(TickFilter);

void reset_filters();

bool filtered(const TickFilter);

TickFilter current_filters();
}
