#include "Filter.h"

namespace lucas::core {
static core::Filter s_filters = core::Filter::None;

void apply_filters(core::Filter filter) {
    s_filters = filter;
}

void reset_filters() {
    s_filters = core::Filter::None;
}

bool is_filtered(const core::Filter filter) {
    return (s_filters & filter) == filter;
}

core::Filter current_filters() {
    return s_filters;
}
}
