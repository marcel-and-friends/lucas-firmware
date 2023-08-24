#include "serial.h"
#include <lucas/lucas.h>
#include <lucas/core/core.h>
#include <lucas/info/info.h>
#include <lucas/serial/DelimitedHook.h>
#include <lucas/serial/FirmwareUpdateHook.h>
#include <lucas/cmd/cmd.h>
#include <src/core/serial.h>

namespace lucas::serial {
void setup() {
    DelimitedHook::make('#', &info::interpret_command_from_host);
    DelimitedHook::make('$', &cmd::interpret_gcode_from_host);
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
