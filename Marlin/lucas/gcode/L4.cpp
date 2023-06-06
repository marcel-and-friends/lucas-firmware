
#include <lucas/gcode/gcode.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L4() {
    Estacao::for_each([](auto& estacao) {
        if (estacao.livre()) {
            estacao.enviar_receita(gcode::RECEITA_PADRAO, gcode::RECEITA_PADRAO_ID);
            return util::Iter::Stop;
        }
        return util::Iter::Continue;
    });
}
}
