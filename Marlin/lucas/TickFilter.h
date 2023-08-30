#pragma once

#include <lucas/types.h>

namespace lucas {
enum class TickFilter : u8 {
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
    return TickFilter(u8(a) | u8(b));
}

constexpr TickFilter operator&(const TickFilter a, const TickFilter b) {
    return TickFilter(u8(a) & u8(b));
}

constexpr TickFilter operator~(const TickFilter a) {
    return TickFilter(~u8(a));
}

void apply_filters(TickFilter);

void reset_filters();

bool filtered(const TickFilter);

TickFilter current_filters();
}
