#pragma once

#include <lucas/types.h>

namespace lucas::core {
enum class Filter : u8 {
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

constexpr Filter operator|(const Filter a, const Filter b) {
    return Filter(u8(a) | u8(b));
}

constexpr Filter operator&(const Filter a, const Filter b) {
    return Filter(u8(a) & u8(b));
}

constexpr Filter operator~(const Filter a) {
    return Filter(~u8(a));
}

void apply_filters(Filter);

void reset_filters();

bool is_filtered(const Filter);

Filter current_filters();
}
