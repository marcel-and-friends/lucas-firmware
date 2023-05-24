#include <lucas/Bico.h>
#include <src/gcode/parser.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L5() {
    constexpr auto MAX = static_cast<long>(Estacao::NUM_ESTACOES);
    auto numero = std::clamp(parser.longval('N'), 1l, MAX);
    Bico::the().viajar_para_estacao(numero);
}
}