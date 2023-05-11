#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>
#include <algorithm>

namespace lucas::gcode {
void L1() {
    if (Bico::ativo())
        return;

    const auto tick = millis();
    const auto tempo = parser.longval('T');
    auto valor = parser.longval('P');
    if (valor > 100)
        valor = 100;
    if (valor < 0)
        valor = 0;

    Bico::ativar(tick, tempo, static_cast<float>(valor));
}
}
