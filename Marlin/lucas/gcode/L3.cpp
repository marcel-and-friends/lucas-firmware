
#include <lucas/Estacao.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>

namespace lucas::gcode {
void L3() {
    static std::string buffer(512, '\0');
    buffer.clear();

    // o diametro é passado em cm, porem o marlin trabalho com mm
    const auto diametro = parser.floatval('D') * 10.f;
    const auto raio = diametro / 2.f;
    const auto num_circulos = parser.intval('N');
    const auto num_semicirculos = num_circulos * 2;
    const auto offset_por_semicirculo = raio / static_cast<float>(num_circulos);
    const auto repeticoes = parser.intval('R', 0);
    const auto series = repeticoes + 1;
    const auto comecar_na_borda = parser.seen_test('B');

    if (!Estacao::ativa())
        adicionar_instrucao(buffer, "G90\nG0 F50000 X80 Y60\nG91\n");

    if (comecar_na_borda)
        adicionar_instrucao_float(buffer, "G0 F50000 X%s\n", -raio);

    for (auto i = 0; i < series; i++) {
        const bool fora_pra_dentro = i % 2 != comecar_na_borda;
        for (auto j = 0; j < num_semicirculos; j++) {
            const auto offset_semicirculo = offset_por_semicirculo * j;
            float offset_x = offset_semicirculo;
            if (fora_pra_dentro)
                offset_x = fabs(offset_x - diametro);
            else
                offset_x += offset_por_semicirculo;

            if (j % 2)
                offset_x = -offset_x;

            const auto offset_centro = offset_x / 2;

            static char buffer_offset_x[16] = {};
            dtostrf(offset_x, 0, 2, buffer_offset_x);
            static char buffer_offset_centro[16] = {};
            dtostrf(offset_centro, 0, 2, buffer_offset_centro);
            adicionar_instrucao(buffer, "G2 F10000 X%s I%s\n", buffer_offset_x, buffer_offset_centro);
        }
    }

    if (series % 2 != comecar_na_borda)
        adicionar_instrucao_float(buffer, "G0 F50000 X%s", raio);

    LOG("buffer = ", buffer.c_str());

    roubar_fila(buffer.c_str());
}
}
