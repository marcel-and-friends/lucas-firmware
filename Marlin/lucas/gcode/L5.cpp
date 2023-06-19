#include <lucas/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <algorithm>

namespace lucas::cmd {
void L5() {
    constexpr auto max = static_cast<long>(Estacao::NUM_MAX_ESTACOES);
    auto num = std::clamp(parser.longval('N'), 0L, max);
    Bico::the().viajar_para_estacao(num);
}
}
