#include "serial.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/info/info.h>
#include <lucas/serial/DelimitedHook.h>
#include <lucas/serial/FirmwareUpdateHook.h>
#include <src/gcode/gcode.h>
#include <src/gcode/queue.h>
#include <src/core/serial.h>

namespace lucas::serial {
void setup() {
    DelimitedHook::make(
        '#',
        [](std::span<char> buffer) {
            info::interpret_json(buffer);
        });

    DelimitedHook::make(
        '$',
        [](std::span<char> buffer) {
            if (buffer.size() == Hook::MAX_BUFFER_SIZE)
                return;

            // hold my hand and tell me everything will be okay
            *buffer.end() = '\0';
            GcodeSuite::process_subcommands_now(buffer.data());
        });

    clean_serial();
}

void hooks() {
    if (FirmwareUpdateHook::active()) {
        FirmwareUpdateHook::the()->think();
    } else {
        DelimitedHook::think();
    }
}

void clean_serial() {
    while (SERIAL_IMPL.available())
        SERIAL_IMPL.read();
}
}
