#include "Filters.h"

namespace lucas {
static Filters s_filters = Filters::None;

void apply_filters(Filters filter) {
    s_filters = filter;
}

void reset_filters() {
    s_filters = Filters::None;
}

bool filtered(const Filters filter) {
    return (s_filters & filter) == filter;
}

Filters current_filters() {
    return s_filters;
}
}
