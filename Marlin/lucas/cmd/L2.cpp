#include <lucas/cmd/cmd.h>
#include <src/gcode/parser.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <lucas/Estacao.h>

namespace lucas::cmd {
void L2() {
    if (parser.seenval('D'))
        Bico::the().despejar_valor_digital(parser.ulongval('T'), parser.ulongval('D'));
    else
        Bico::the().despejar_volume(parser.ulongval('T'), parser.floatval('G'), Bico::CorrigirFluxo::Sim);

    bool const associado_a_estacao = Fila::the().executando();
    util::aguardar_enquanto([&] {
        // receita foi cancelada por meios externos
        if (associado_a_estacao and not Fila::the().executando())
            return false;
        return Bico::the().ativo();
    });
}
}
