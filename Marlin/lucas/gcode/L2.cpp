#include <lucas/Estacao.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
void L2() {
    if (!Estacao::ativa())
        return;

    const auto tempo = parser.longval('T');
    auto& estacao = *Estacao::ativa();
    estacao.pausar(tempo);
    Estacao::procurar_nova_ativa();
}
}
