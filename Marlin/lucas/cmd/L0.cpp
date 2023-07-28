#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/cmd/cmd.h>
#include <src/module/planner.h>
#include <numbers>

namespace lucas::cmd {
void L0() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    auto const diametro_total = (parser.floatval('D') / util::step_ratio_x()) * 10.f;
    if (diametro_total == 0.f)
        return;

    auto const raio = diametro_total / 2.f;
    auto const num_circulos = parser.intval('N');
    if (num_circulos == 0)
        return;

    // cada circulo é composto por 2 arcos
    auto const num_arcos = num_circulos * 2;
    auto const offset_por_arco = raio / static_cast<float>(num_circulos);
    auto const repeticoes = parser.intval('R', 0);
    auto const series = repeticoes + 1;
    auto const comecar_na_borda = parser.seen_test('B');

    auto const duracao = parser.ulongval('T');

    if (CFG(ModoGiga) and duracao) {
        LOG("iniciando L0 em modo giga");
        util::aguardar_por(duracao);
        LOG("L0 finalizado");
        return;
    }

    auto const volume_agua = parser.floatval('G');
    auto const despejar_agua = duracao and volume_agua;

    bool const associado_a_estacao = Fila::the().executando();
    bool vaza = false;

    auto const posicao_inicial = planner.get_axis_positions_mm();
    auto posicao_final = posicao_inicial;

    auto for_each_arco = [&](util::IterFn<float, float, int> auto&& callback) {
        for (auto serie = 0; serie < series; serie++) {
            const bool fora_pra_dentro = serie % 2 != comecar_na_borda;
            for (auto arco = 0; arco < num_arcos; arco++) {
                const auto offset_arco = offset_por_arco * arco;
                float diametro_arco = fora_pra_dentro ? std::abs(offset_arco - diametro_total) : offset_arco + offset_por_arco;
                if (arco % 2)
                    diametro_arco = -diametro_arco;

                const auto raio_arco = diametro_arco / 2.f;
                if (callback(diametro_arco, raio_arco, arco) == util::Iter::Break)
                    return;
            }
        }
    };

    char buffer_raio[16] = {};
    dtostrf(raio, 0, 2, buffer_raio);

    if (comecar_na_borda) {
        executar_fmt("G0 F50000 X-%s", buffer_raio);
        Bico::the().aguardar_viagem_terminar();
    }

    float total_percorrido = 0.f;
    for_each_arco([&total_percorrido](float, float raio, int) {
        total_percorrido += (2.f * std::numbers::pi_v<float> * std::abs(raio)) / 2.f;
        return util::Iter::Continue;
    });

    auto const steps_por_mm_ratio = duracao ? util::MS_POR_MM / (duracao / total_percorrido) : 1.f;

    soft_endstop._enabled = false;
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X * steps_por_mm_ratio;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y * steps_por_mm_ratio;
    planner.refresh_positioning();

    if (despejar_agua)
        Bico::the().despejar_volume(duracao, volume_agua, Bico::CorrigirFluxo::Sim);

    for_each_arco([&](float diametro, float raio, int arco) {
        char buffer_diametro[16] = {};
        dtostrf(diametro / steps_por_mm_ratio, 0, 2, buffer_diametro);

        char buffer_raio[16] = {};
        dtostrf(raio / steps_por_mm_ratio, 0, 2, buffer_raio);

        executar_fmt("G2 F5000 X%s I%s", buffer_diametro, buffer_raio);

        auto offset_x = float(offset_por_arco + ((arco / 2) * offset_por_arco));
        if (arco % 2)
            offset_x = -offset_x;

        posicao_final.x = posicao_inicial.x + offset_x;

        if (associado_a_estacao and not Fila::the().executando()) {
            vaza = true;
            return util::Iter::Break;
        }

        return util::Iter::Continue;
    });

    if (despejar_agua)
        Bico::the().desligar();

    Bico::the().aguardar_viagem_terminar();

    current_position = posicao_final;

    soft_endstop._enabled = true;
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y;
    planner.refresh_positioning();

    if (vaza)
        return;

    if (series % 2 != comecar_na_borda) {
        executar_fmt("G0 F50000 X%s", buffer_raio);
        Bico::the().aguardar_viagem_terminar();
    }

    current_position = posicao_inicial;
    sync_plan_position();
}
}
