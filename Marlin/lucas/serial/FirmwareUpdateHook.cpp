#include "FirmwareUpdateHook.h"
#include <lucas/lucas.h>
#include <lucas/info/info.h>

namespace lucas::serial {
void FirmwareUpdateHook::think() {
    if (m_receive_timer >= 1s) {
        info::send(
            info::Event::Firmware,
            [](JsonObject o) {
                o["updateFailedCode"] = 1;
            });

        deactivate();
    }

    while (SERIAL_IMPL.available()) {
        m_receive_timer.start();
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
