#include <lucas/cmd/cmd.h>
#include <lucas/Fila.h>
#include <lucas/Receita.h>

namespace lucas::cmd {
void L7() {
    if (parser.seenval('P')) {
        const auto pino = parser.value_int();
        SET_INPUT(pino);
        LOG("valor = ", analogRead(pino));
    } else {
        Fila::the().for_each_receita_mapeada([](Receita& receita, size_t idx) {
            LOG("comeco atual =", receita.passo_atual().comeco_abs, " | estacao = ", idx);
            return util::Iter::Continue;
        });
    }
}
}
