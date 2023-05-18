#include <lucas/Bico.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
void L5() {
    auto numero = std::clamp(parser.longval('N'), 1l, 5l);
    Bico::the().viajar_para_estacao(numero);
}
}
