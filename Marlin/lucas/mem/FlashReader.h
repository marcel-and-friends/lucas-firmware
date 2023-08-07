#pragma once

#include <cstddef>
#include <lucas/mem/mem.h>
#include <concepts>

namespace lucas::mem {
template<typename T = void>
class FlashReader {
public:
    explicit FlashReader(size_t offset)
        : m_offset{ offset } {
        mem::fill_flash_buffer();
    }

    template<typename U = T>
    requires std::is_integral_v<U>
    auto read(size_t index) {
        if constexpr (std::same_as<T, void>)
            return mem::buffered_read_flash<U>(m_offset + index);
        else
            return mem::buffered_read_flash<T>(m_offset + (index * sizeof(T)));
    }

private:
    size_t m_offset = 0;
};
}
