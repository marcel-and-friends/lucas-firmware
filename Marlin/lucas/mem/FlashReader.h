#pragma once

#include <cstddef>
#include <cstdint>
#include <lucas/mem/mem.h>

namespace lucas::mem {
class FlashReader {
public:
    static FlashReader at_offset(size_t offset) {
        return FlashReader{ offset };
    }

    template<typename T>
    requires std::is_integral_v<T>
    T read(size_t index) {
        return mem::buffered_read_flash<T>(m_offset + index);
    }

private:
    constexpr FlashReader(size_t offset)
        : m_offset{ offset } {
        mem::fill_flash_buffer();
    }

    size_t m_offset = 0;
};
}
