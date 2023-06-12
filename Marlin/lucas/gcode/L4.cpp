
#include <lucas/gcode/gcode.h>
#include <lucas/Estacao.h>
#include <lucas/Fila.h>

namespace lucas::gcode {
void L4() {
    Estacao::for_each([](auto& estacao) {
        if (estacao.livre()) {
            Fila::the().agendar_receita(estacao.index(), Receita::padrao());
            return util::Iter::Stop;
        }
        return util::Iter::Continue;
    });
}
}
