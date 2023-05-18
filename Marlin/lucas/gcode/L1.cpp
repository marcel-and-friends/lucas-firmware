#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>
#include <algorithm>

namespace lucas::gcode {
void L1() {
    if (Bico::the().ativo())
        return;

    Bico::the().ativar(millis(), parser.intval('T'), parser.floatval('G'));
}
}
