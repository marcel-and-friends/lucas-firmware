#include "gcode.h"
#include <cstring>
#include <src/gcode/parser.h>
#include <src/gcode/queue.h>
#include <src/gcode/gcode.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void injetar(const char* gcode) {
    queue.injected_commands_P = gcode;
}

std::string_view proxima_instrucao(const char* gcode) {
    // como uma sequência de gcodes é separada pelo caractere de nova linha basta procurar um desse para encontrar o fim
    auto fim_do_proximo_gcode = strchr(gcode, '\n');
    if (!fim_do_proximo_gcode) {
        // se a nova linha não existe então estamos no último
        return gcode;
    }

    return std::string_view{ gcode, static_cast<size_t>(fim_do_proximo_gcode - gcode) };
}

bool ultima_instrucao(const char* gcode) {
    return strchr(gcode, '\n') == nullptr;
}

void parar_fila() {
    queue.injected_commands_P = nullptr;
}
}
