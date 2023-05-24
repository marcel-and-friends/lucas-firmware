#include <lucas/Estacao.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
void L2() {
    if (!Estacao::ativa())
        return;

    Estacao::ativa()->pausar(parser.ulongval('T'));
}
}