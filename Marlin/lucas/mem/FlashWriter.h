#pragma once

#include <cstddef>
#include <lucas/mem/mem.h>
#include <concepts>

namespace lucas::mem {
template<typename T = void>
class FlashWriter {
public:
    constexpr explicit FlashWriter(usize offset)
        : m_offset{ offset } {
    }

    ~FlashWriter() {
        mem::flush_flash_buffer();
    }

    template<typename U = T>
    requires std::is_integral_v<U>
    void write(usize index, U value) {
        if constexpr (std::same_as<T, void>)
            mem::buffered_write_flash(m_offset + index, value);
        else
            mem::buffered_write_flash(m_offset + (index * sizeof(T)), value);
    }

private:
    usize m_offset = 0;
};
}
