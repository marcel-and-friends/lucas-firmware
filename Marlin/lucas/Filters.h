#pragma once

#include <cstdint>

namespace lucas {
enum class Filters : uint32_t {
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

constexpr Filters operator|(const Filters a, const Filters b) {
    return Filters(uint32_t(a) | uint32_t(b));
}

constexpr Filters operator&(const Filters a, const Filters b) {
    return Filters(uint32_t(a) & uint32_t(b));
}

constexpr Filters operator~(const Filters a) {
    return Filters(~uint32_t(a));
}

void apply_filters(Filters);

void reset_filters();

bool filtered(const Filters);

Filters current_filters();
}
