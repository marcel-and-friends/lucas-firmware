
#include <lucas/gcode/gcode.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L4() {
    for (auto& estacao : Estacao::lista()) {
        if (estacao.livre()) {
            estacao.set_livre(false);
            estacao.set_receita(gcode::RECEITA_PADRAO);
            estacao.aguardar_input();
            return;
        }
    }
}
}
