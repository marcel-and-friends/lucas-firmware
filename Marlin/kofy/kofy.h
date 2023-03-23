#pragma once

#include <src/gcode/queue.h>
#include "wifi.h"
#include "opcoes.h"

namespace kofy {
    
#define DBG SERIAL_ECHO_MSG
#define INFO(tipo, valor) DBG("kofy_info:", #tipo,":",valor);

inline bool g_mudando_boca_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_inicializando = false;

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