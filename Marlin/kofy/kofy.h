#pragma once

#include <src/gcode/queue.h>
#include "opcoes.h"

namespace kofy {
    
#define DBG SERIAL_ECHO_MSG

inline bool g_mudando_boca_ativa = false;
inline std::string g_gcode = "";
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

void conectar_wifi(std::string_view nome_rede, std::string_view senha_rede);

}

}