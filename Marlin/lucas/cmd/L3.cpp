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
    const auto diametro_total = (parser.floatval('D') / util::step_ratio_x()) * 10.f;
    if (diametro_total == 0.f)
        return;

    const auto raio = diametro_total / 2.f;
    const auto num_circulos = parser.intval('N');
    if (num_circulos == 0)
        return;

    const auto num_arcos = num_circulos * 2;
    const auto offset_por_arco = raio / static_cast<float>(num_circulos);
    const auto repeticoes = parser.intval('R', 0);
    const auto series = repeticoes + 1;
    const auto comecar_na_borda = parser.seen_test('B');

    const auto duracao = parser.ulongval('T');
    const auto volume_agua = parser.floatval('G');
    const auto despejar_agua = duracao && volume_agua;

    auto for_each_arco = [&](auto&& callback) {
        for (auto serie = 0; serie < series; serie++) {
            const bool fora_pra_dentro = serie % 2 != comecar_na_borda;
            for (auto arco = 0; arco < num_arcos; arco++) {
                const auto offset_arco = offset_por_arco * arco;

                float diametro_arco = offset_arco;
                if (fora_pra_dentro)
                    diametro_arco = std::abs(diametro_arco - diametro_total);
                else
                    diametro_arco += offset_por_arco;

                if (arco % 2)
                    diametro_arco = -diametro_arco;

                const auto raio_arco = diametro_arco / 2.f;

                callback(diametro_arco, raio_arco);
            }
        }
    };

    char buffer_raio[16] = {};
    dtostrf(raio, 0, 2, buffer_raio);

    const auto posicao_no_comeco = planner.get_axis_positions_mm();

    if (comecar_na_borda) {
        executar_fmt("G0 F50000 X-%s", buffer_raio);
        planner.synchronize();
    }

    float total_percorrido = 0.f;
    for_each_arco([&total_percorrido](float, float raio) {
        total_percorrido += (2.f * std::numbers::pi_v<float> * std::abs(raio)) / 2.f;
    });
    LOG("total percorrido = ", total_percorrido);

    const auto steps_por_mm_ratio = util::MS_POR_MM / (duracao / total_percorrido);
    LOG("ratio = ", steps_por_mm_ratio);

    if (duracao) {
        soft_endstop._enabled = false;
        planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X * steps_por_mm_ratio;
        planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y * steps_por_mm_ratio;
        planner.refresh_positioning();
    }

    auto comeco = millis();
    LOG("comecando L3 - ", comeco);

    if (despejar_agua)
        Bico::the().despejar_volume(duracao, volume_agua);

    for_each_arco([&](float diametro, float raio) {
        if (duracao) {
            diametro /= steps_por_mm_ratio;
            raio /= steps_por_mm_ratio;
        }

        char buffer_diametro[16] = {};
        dtostrf(diametro, 0, 2, buffer_diametro);
        char buffer_raio[16] = {};
        dtostrf(raio, 0, 2, buffer_raio);

        executar_fmt("G2 F5000 X%s I%s", buffer_diametro, buffer_raio);
    });

    if (despejar_agua)
        Bico::the().desligar();

    planner.synchronize();

    LOG("terminado L3 - delta = ", millis() - comeco);

    if (duracao) {
        soft_endstop._enabled = true;
        planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X;
        planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y;
        planner.refresh_positioning();
    }

    if (series % 2 != comecar_na_borda) {
        executar_fmt("G0 F50000 X%s", buffer_raio);
        planner.synchronize();
    }

    planner.set_position_mm(posicao_no_comeco);
}
}
