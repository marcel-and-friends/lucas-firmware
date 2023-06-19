#include <lucas/Estacao.h>

namespace lucas::cmd {
void L0() {
    if (!Estacao::ativa())
        return;

    Estacao::ativa()->set_aguardando_input(true);
    Estacao::procurar_nova_ativa();
}
}
