#pragma once

#include <lucas/TickFilter.h>
#include <src/core/serial.h>

namespace lucas::core {
class TemporaryFilter {
public:
    TemporaryFilter(TickFilter filters) {
        m_backup = current_filters();
        apply_filters(filters | m_backup);
    }

    TemporaryFilter(auto... filters) {
        m_backup = current_filters();
        apply_filters((filters | ...) | m_backup);
    }

    ~TemporaryFilter() {
        apply_filters(m_backup);
    }

private:
    TickFilter m_backup;
};
}
