#pragma once

#include <src/gcode/queue.h>

namespace kofy {
    
#define DBG SERIAL_ECHO_MSG

void setup();

void event_handler();

void idle();

namespace marlin {

inline bool apertado(int pino) {
    return READ(pino) == false;
}

inline void parar_fila_de_gcode() {
    queue.injected_commands_P = nullptr;
}

inline void injetar_gcode(std::string_view gcode) {
    queue.injected_commands_P = gcode.data();
}

}

}