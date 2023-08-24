#include "Hook.h"
#include <src/core/serial.h>
#include <src/MarlinCore.h>
#include <lucas/lucas.h>

namespace lucas::serial {
bool Hook::is_valid() const {
    return m_callback and m_buffer_size;
}

void Hook::dispatch() {
    ok_to_receive();
    const auto buffer_size = m_buffer_size;
    reset();
    if (m_callback and buffer_size) {
        if (CFG(LogSerial)) {
            if (buffer_size == MAX_BUFFER_SIZE) {
                char null_terminated_buffer[MAX_BUFFER_SIZE + 1];
                memcpy(null_terminated_buffer, m_buffer, MAX_BUFFER_SIZE);
                null_terminated_buffer[MAX_BUFFER_SIZE] = '\0';
                LOG_IF(LogSerial, "", null_terminated_buffer);
            } else {
                m_buffer[buffer_size] = '\0';
                LOG_IF(LogSerial, "", m_buffer);
            }
        }
    }
    m_callback({ m_buffer, buffer_size });
}

void Hook::reset() {
    m_counter = 0;
    m_buffer_size = 0;
}

void Hook::ok_to_receive() {
    SERIAL_ECHOLN(OK_CODE);
}

void Hook::add_to_buffer(char c) {
    if (m_buffer_size >= MAX_BUFFER_SIZE) {
        LOG_ERR("BUFFER NAO TANKOU! ADEUS!");
        kill();
    }

    m_buffer[m_buffer_size++] = c;
    if (++m_counter >= MAX_BUFFER_SLICE) {
        m_counter = 0;
        ok_to_receive();
    }
}
}
