#include <lucas/Estacao.h>

namespace lucas::gcode {
void L0() {
    if (!Estacao::ativa())
        return;

    Estacao::ativa()->aguardar_input();
    Estacao::procurar_nova_ativa();
}
}
