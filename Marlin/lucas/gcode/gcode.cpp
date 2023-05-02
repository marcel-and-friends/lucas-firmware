#include "gcode.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <src/gcode/gcode.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {

void injetar(std::string_view gcode) {
    queue.injected_commands_P = gcode.data();
}

void injetar_imediato(const char* gcode) {
    queue.injected_commands_P = gcode;
}

std::string_view proxima_instrucao(std::string_view gcode) {
    auto c_str = gcode.data();
    // como uma sequência de gcodes é separada pelo caractere de nova linha basta procurar um desse para encontrar o fim
    auto fim_do_proximo_gcode = strchr(c_str, '\n');
    if (!fim_do_proximo_gcode) {
        // se a nova linha não existe então estamos no último
        return gcode;
    }

    return std::string_view{ c_str, static_cast<size_t>(fim_do_proximo_gcode - c_str) };
}

bool e_ultima_instrucao(std::string_view gcode) {
    return strchr(gcode.data(), '\n') == nullptr;
}

static const char* g_fila_roubada = nullptr;
static const char* g_fila_backup = nullptr;

void parar_fila() {
    queue.injected_commands_P = g_fila_backup = g_fila_roubada = nullptr;
}

void roubar_fila(const char* gcode) {
    g_fila_roubada = gcode;
    g_fila_backup = queue.injected_commands_P;
    queue.injected_commands_P = g_fila_roubada;
}

void fila_roubada_terminou() {
    queue.injected_commands_P = g_fila_backup;
    g_fila_backup = g_fila_roubada = nullptr;
}

bool roubando_fila() {
    return g_fila_roubada;
}

bool tem_comandos_pendentes() {
    return queue.injected_commands_P != nullptr;
}
}
