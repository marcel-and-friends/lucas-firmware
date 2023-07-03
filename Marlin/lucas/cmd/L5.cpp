#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <algorithm>

namespace lucas::cmd {
void L5() {
    constexpr auto max = static_cast<long>(Estacao::NUM_MAX_ESTACOES);
    auto num = parser.longval('N');
    if (num == -1)
        Bico::the().viajar_para_esgoto();
    else
        Bico::the().viajar_para_estacao(std::clamp(num, 0L, max));
}
}
