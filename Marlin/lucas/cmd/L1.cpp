#include <lucas/Estacao.h>
#include <lucas/Bico.h>
#include <lucas/Fila.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <lucas/cmd/cmd.h>
#include <src/module/planner.h>
#include <numbers>

namespace lucas::cmd {
void L1() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    auto const diametro_circulo = (parser.floatval('D') / util::step_ratio_x()) * 10.f;
    if (diametro_circulo == 0.f)
        return;

    auto const raio_circulo = diametro_circulo / 2.f;
    auto const num_repeticoes = parser.intval('N');
    if (num_repeticoes == 0)
        return;

    // cada circulo é composto por 2 arcos
    auto const num_arcos = num_repeticoes * 2;
    auto const duracao = parser.ulongval('T');

    if (CFG(ModoGiga) and duracao) {
        LOG("iniciando L1 em modo giga");
        util::aguardar_por(duracao);
        LOG("L1 finalizado");
        return;
    }

    auto const volume_agua = parser.floatval('G');
    auto const despejar_agua = duracao and volume_agua;
    bool const associado_a_estacao = Fila::the().executando();
    bool vaza = false;

    auto const posicao_inicial = planner.get_axis_positions_mm();
    auto posicao_final = posicao_inicial;

    executar_ff("G0 F10000 X%s", -raio_circulo);
    Bico::the().aguardar_viagem_terminar();

    float const total_percorrido = (2.f * std::numbers::pi_v<float> * raio_circulo) * num_repeticoes;
    float const steps_por_mm_ratio = duracao ? util::MS_POR_MM / (duracao / total_percorrido) : 1.f;

    soft_endstop._enabled = false;
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X * steps_por_mm_ratio;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y * steps_por_mm_ratio;
    planner.refresh_positioning();

    if (despejar_agua)
        Bico::the().despejar_volume(duracao, volume_agua, Bico::CorrigirFluxo::Sim);

    for (int i = 0; i < num_arcos; i++) {
        float diametro = diametro_circulo;
        if (i % 2 != 0)
            diametro = -diametro;

        float raio = diametro / 2.f;

        posicao_final.x = posicao_inicial.x + raio;

        diametro /= steps_por_mm_ratio;
        raio /= steps_por_mm_ratio;

        char buffer_diametro[16] = {};
        dtostrf(diametro, 0, 2, buffer_diametro);
        char buffer_raio[16] = {};
        dtostrf(raio, 0, 2, buffer_raio);

        executar_fmt("G2 F5000 X%s I%s", buffer_diametro, buffer_raio);

        if (associado_a_estacao and not Fila::the().executando()) {
            vaza = true;
            break;
        }
    }

    if (despejar_agua)
        Bico::the().desligar();

    Bico::the().aguardar_viagem_terminar();

    current_position = posicao_final;
    planner.settings.axis_steps_per_mm[X_AXIS] = util::DEFAULT_STEPS_POR_MM_X;
    planner.settings.axis_steps_per_mm[Y_AXIS] = util::DEFAULT_STEPS_POR_MM_Y;
    planner.refresh_positioning();
    soft_endstop._enabled = true;

    if (vaza)
        return;

    auto const offset = posicao_inicial.x - posicao_final.x;
    executar_ff("G0 F10000 X%s", offset);
    Bico::the().aguardar_viagem_terminar();

    current_position = posicao_inicial;
    sync_plan_position();
}
}
