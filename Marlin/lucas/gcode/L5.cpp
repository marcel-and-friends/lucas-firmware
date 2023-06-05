#include <lucas/Bico.h>
#include <src/gcode/parser.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L5() {
    Bico::the().viajar_para_estacao(parser.longval('N'));
}
}
