#include <lucas/Bico.h>
#include <src/gcode/parser.h>
#include <lucas/Estacao.h>

namespace lucas::gcode {
void L5() {
    constexpr auto max = static_cast<long>(Estacao::NUM_MAX_ESTACOES);
    auto num = std::clamp(parser.longval('N'), 1L, max);
    Bico::the().viajar_para_estacao(num);
}
}
