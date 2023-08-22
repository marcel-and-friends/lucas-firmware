#include "TickFilter.h"

namespace lucas {
static TickFilter s_filters = TickFilter::None;

void apply_filters(TickFilter filter) {
    s_filters = filter;
}

void reset_filters() {
    s_filters = TickFilter::None;
}

bool filtered(const TickFilter filter) {
    return (s_filters & filter) == filter;
}

TickFilter current_filters() {
    return s_filters;
}
}
