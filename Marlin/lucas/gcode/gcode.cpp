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

bool tem_comandos_pendentes() {
    return queue.injected_commands_P != nullptr;
}
}

/*
(
M92 X132 Y72
G28 XY
G90
G0 F15000 X90 Y110
G91
G2 F2500 X2.50 I1.25
G2 F2500 X-5.00 I-2.50
G2 F2500 X7.50 I3.75
G2 F2500 X-10.00 I-5.00
G2 F2500 X12.50 I6.25
G2 F2500 X-15.00 I-7.50
G2 F2500 X15.00 I7.50
G2 F2500 X-12.50 I-6.25
G2 F2500 X10.00 I5.00
G2 F2500 X-7.50 I-3.75
G2 F2500 X5.00 I2.50
G2 F2500 X-2.50 I-1.25
G90
G0 F15000 X10 Y110
G91
G2 F1000 X2.50 I1.25
G2 F1000 X-5.00 I-2.50
G2 F1000 X7.50 I3.75
G2 F1000 X-10.00 I-5.00
G2 F1000 X12.50 I6.25
G2 F1000 X-15.00 I-7.50
G2 F1000 X15.00 I7.50
G2 F1000 X-12.50 I-6.25
G2 F1000 X10.00 I5.00
G2 F1000 X-7.50 I-3.75
G2 F1000 X5.00 I2.50
G2 F1000 X-2.50 I-1.25
G90
G0 F15000 X100 Y110
G91
G2 F2500 X2.50 I1.25
G2 F2500 X-5.00 I-2.50
G2 F2500 X7.50 I3.75
G2 F2500 X-10.00 I-5.00
G2 F2500 X12.50 I6.25
G2 F2500 X-15.00 I-7.50
G2 F2500 X15.00 I7.50
G2 F2500 X-12.50 I-6.25
G2 F2500 X10.00 I5.00
G2 F2500 X-7.50 I-3.75
G2 F2500 X5.00 I2.50
G2 F2500 X-2.50 I-1.25
G90
G0 F15000 X50 Y110
G91
G2 F1000 X2.50 I1.25
G2 F1000 X-5.00 I-2.50
G2 F1000 X7.50 I3.75
G2 F1000 X-10.00 I-5.00
G2 F1000 X12.50 I6.25
G2 F1000 X-15.00 I-7.50
G2 F1000 X15.00 I7.50
G2 F1000 X-12.50 I-6.25
G2 F1000 X10.00 I5.00
G2 F1000 X-7.50 I-3.75
G2 F1000 X5.00 I2.50
G2 F1000 X-2.50 I-1.25
)
*/
