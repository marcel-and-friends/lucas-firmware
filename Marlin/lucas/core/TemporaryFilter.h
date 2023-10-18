#pragma once

#include <lucas/core/Filter.h>
#include <src/core/serial.h>

namespace lucas::core {
class TemporaryFilter {
public:
    TemporaryFilter(core::Filter filters) {
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
    core::Filter m_backup;
};
}
