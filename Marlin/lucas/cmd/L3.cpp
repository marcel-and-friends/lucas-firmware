#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/cmd/cmd.h>
#include <src/module/planner.h>
#include <numbers>

namespace lucas::cmd {
void L3() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    const auto diametro = (parser.floatval('D') / util::step_ratio_x()) * 10.f;
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

    const auto tempo = parser.ulongval('T');
    const auto fluxo_agua = parser.floatval('G');

    auto comeco = millis();
    LOG("comecando L3 - ", comeco);

    auto for_each_semicirculo = [&](auto&& callback) {
        for (auto i = 0; i < series; i++) {
            const bool fora_pra_dentro = i % 2 != comecar_na_borda;
            for (auto j = 0; j < num_semicirculos; j++) {
                const auto offset_semicirculo = offset_por_semicirculo * j;
                float diametro_semicirculo = offset_semicirculo;
                if (fora_pra_dentro)
                    diametro_semicirculo = fabs(diametro_semicirculo - diametro);
                else
                    diametro_semicirculo += offset_por_semicirculo;

                if (j % 2)
                    diametro_semicirculo = -diametro_semicirculo;

                const auto raio_semicirculo = diametro_semicirculo / 2.f;

                callback(diametro_semicirculo, raio_semicirculo);
            }
        }
    };

    float total_percorrido = 0.f;
    for_each_semicirculo([&total_percorrido](float, float raio) {
        total_percorrido += (2.f * std::numbers::pi_v<float> * std::abs(raio)) / 2.f;
    });

    char buffer_raio[16] = {};
    dtostrf(raio, 0, 2, buffer_raio);

    if (comecar_na_borda) {
        LOG("comecando na borda");
        executar_fmt("G0 F50000 X-%s", velocidade, buffer_raio);
        planner.synchronize();
    }

    if (tempo) {
        const auto ratio = util::MS_POR_MM / (tempo / total_percorrido);
        planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X * ratio;
        planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y * ratio;
        planner.refresh_positioning();
    }

    if (fluxo_agua && tempo)
        Bico::the().ativar(tempo, fluxo_agua);

    for_each_semicirculo([velocidade](float diametro, float raio) {
        // que BOSTA
        diametro /= util::step_ratio_x();
        raio /= util::step_ratio_y();

        char buffer_diametro[16] = {};
        dtostrf(diametro, 0, 2, buffer_diametro);
        char buffer_raio[16] = {};
        dtostrf(raio, 0, 2, buffer_raio);

        executar_fmt("G2 F%d X%s I%s", velocidade, buffer_diametro, buffer_raio);
    });

    LOG("terminando despejo - delta = ", millis() - comeco);

    Bico::the().desligar();

    planner.synchronize();

    if (tempo) {
        planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X;
        planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y;
        planner.refresh_positioning();
    }

    if (series % 2 != comecar_na_borda) {
        LOG("voltando pro meio");
        executar_fmt("G0 F50000 X%s", velocidade, buffer_raio);
        planner.synchronize();
    }

    LOG("terminando L3 - delta = ", millis() - comeco);
}
}
