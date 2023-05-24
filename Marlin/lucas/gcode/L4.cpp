
#include <lucas/gcode/gcode.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L4() {
    for (auto& estacao : Estacao::lista()) {
        if (estacao.livre()) {
            estacao.enviar_receita(gcode::RECEITA_PADRAO, gcode::RECEITA_PADRAO_ID);
            return;
        }
    }
}
}
