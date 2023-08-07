#pragma once

#include <cstddef>
#include <cstdint>
#include <lucas/mem/mem.h>

namespace lucas::mem {
class FlashWriter {
public:
    static FlashWriter at_offset(size_t offset) {
        return FlashWriter{ offset };
    }

    ~FlashWriter() {
        mem::flush_flash_buffer();
    }

    template<typename T>
    requires std::is_integral_v<T>
    void write(size_t index, T value) {
        return mem::buffered_write_flash(m_offset + index, value);
    }

private:
    constexpr FlashWriter(size_t offset)
        : m_offset{ offset } {
    }

    size_t m_offset = 0;
};
}
