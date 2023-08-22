#pragma once

#include <concepts>
#include <utility/stm32_eeprom.h>
#include <climits>

namespace lucas::mem {
template<typename T>
requires std::is_integral_v<T>
inline T buffered_read_flash(size_t index) {
    T result{};
    for (size_t i = 0; i < sizeof(T); ++i) {
        const auto shift = i * sizeof(uint8_t) * CHAR_BIT;
        result |= eeprom_buffered_read_byte(index + i) << shift;
    }
    return result;
}

template<typename T>
requires std::is_integral_v<T>
inline void buffered_write_flash(size_t index, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        const auto shift = i * sizeof(uint8_t) * CHAR_BIT;
        const auto valor = static_cast<uint8_t>(value >> shift);
        eeprom_buffered_write_byte(index + i, valor);
    }
}

inline void flush_flash_buffer() {
    eeprom_buffer_flush();
}

inline void fill_flash_buffer() {
    eeprom_buffer_fill();
}
}
