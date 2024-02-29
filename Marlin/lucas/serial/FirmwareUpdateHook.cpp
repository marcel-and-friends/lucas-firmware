#include "FirmwareUpdateHook.h"
#include <lucas/lucas.h>
#include <lucas/info/info.h>

namespace lucas::serial {
void FirmwareUpdateHook::think() {
    if (m_receive_timer >= 5s) {
        if (m_on_error_callback)
            m_on_error_callback(1);
        return;
    }

    while (SERIAL_IMPL.available()) {
        m_receive_timer.restart();
        receive_char(SERIAL_IMPL.read());
    }
}

void FirmwareUpdateHook::receive_char(char c) {
    m_buffer[m_buffer_size++] = c;
    m_bytes_received++;
    if (m_buffer_size == MAX_BUFFER_SIZE or m_bytes_received == m_bytes_to_receive) {
        dispatch();
    } else if (++m_counter >= MAX_BUFFER_SLICE) {
        m_counter = 0;
        ok_to_receive();
    }
}
}
