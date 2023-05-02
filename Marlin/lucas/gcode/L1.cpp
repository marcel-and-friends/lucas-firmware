#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L1() {
    const auto tick = millis();
    const auto tempo = parser.longval('T');
    const auto potencia = static_cast<float>(parser.longval('P')); // PWM funciona de [0-255], então multiplicamos antes de passar pro Bico
    Bico::ativar(tick, tempo, potencia);
    if (Estacao::ativa())
        parar_fila();
}
}
