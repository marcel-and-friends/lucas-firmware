#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/gcode/gcode.h>
#include <src/module/planner.h>

namespace lucas::cmd {
void L3() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    const auto diametro = (parser.floatval('D') / util::step_ratio()) * 10.f;
    if (diametro == 0.f)
        return;

    const auto raio = diametro / 2.f;
    const auto num_circulos = parser.intval('N');
    if (num_circulos == 0)
        return;

    const auto num_semicirculos = num_circulos * 2;
    const auto offset_por_semicirculo = raio / static_cast<float>(num_circulos);
    const auto repeticoes = parser.intval('R', 0);
    const auto series = repeticoes + 1;
    const auto comecar_na_borda = parser.seen_test('B');
    const auto velocidade = parser.intval('F', 5000);

    // controle do bico junto com o L3
    // TODO: movimento controlado pelo tempo
    const auto tempo = parser.intval('T');
    const auto despejar_agua = parser.intval('A');

    auto comeco = millis();
    LOG("comecando L3 - ", comeco);
    while (millis() - comeco < tempo)
        idle();

    LOG("terminando L3 - delta = ", millis() - comeco);

    return;

    static char buffer_raio[16] = {};
    dtostrf(raio, 0, 2, buffer_raio);

    if (comecar_na_borda)
        executar_fmt("G0 F%d X-%s", velocidade, buffer_raio);

    planner.synchronize();

    if (despejar_agua && tempo)
        Bico::the().ativar(millis(), tempo, despejar_agua);

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

            // que BOSTA
            static char buffer_offset_x[16] = {};
            dtostrf(offset_x, 0, 2, buffer_offset_x);
            static char buffer_offset_centro[16] = {};
            dtostrf(offset_centro, 0, 2, buffer_offset_centro);

            executar_fmt("G2 F%d X%s I%s", velocidade, buffer_offset_x, buffer_offset_centro);
        }
    }

    if (series % 2 != comecar_na_borda)
        executar_fmt("G0 F%d X%s", velocidade, buffer_raio);

    planner.synchronize();

    LOG("terminando L3 - delta = ", millis() - comeco);

    // teoricamente isso aqui não deve acontecer, mas caso aconteca...
    if (despejar_agua) {
        while (Bico::the().ativo())
            idle();
    }

    return;
}
}
