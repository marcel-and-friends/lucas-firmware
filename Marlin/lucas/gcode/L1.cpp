#include <lucas/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Estacao.h>

namespace lucas::cmd {
void L1() {
    SET_INPUT(MT_DET_1_PIN);
    Bico::the().ativar(millis(), parser.ulongval('T'), parser.floatval('G'));

    while (Bico::the().ativo()) {
        idle();
        if (millis() % 500) {
            LOG("leitura = ", analogRead(MT_DET_1_PIN));
        }
    }
}
}
