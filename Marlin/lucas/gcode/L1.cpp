#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>
#include <algorithm>

namespace lucas::gcode {
void L1() {
    Bico::the().ativar(millis(), parser.ulongval('T'), parser.floatval('G'));

    while (Bico::the().ativo())
        idle();
}
}
