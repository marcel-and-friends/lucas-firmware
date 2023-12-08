#include <lucas/cmd/cmd.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <lucas/MotionController.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <src/module/planner.h>

#define L0_LOG(...) LOG_IF(LogLn, "", "L0: ", __VA_ARGS__);

namespace lucas::cmd {
void L0() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    const auto total_diameter = (parser.floatval('D') / MotionController::the().step_ratio_x()) * 10.f;
    if (total_diameter == 0.f)
        return;

    const auto radius = total_diameter / 2.f;
    const auto number_of_circles = parser.intval('N');
    if (number_of_circles == 0)
        return;

    // cada circulo é composto por 2 arcos
    const auto number_of_arcs = number_of_circles * 2;
    const auto offset_per_arc = radius / static_cast<float>(number_of_circles);
    const auto repetitions = parser.intval('R', 0);
    const auto series = repetitions + 1;
    const auto start_on_border = parser.seen_test('B');
    const auto duration = parser.ulongval('T');

    L0_LOG("!opcoes!\ndiametro = ", total_diameter, "\nraio = ", radius, "\nnum_circulos = ", number_of_circles, "\nnum_arcos = ", number_of_arcs, "\noffset_por_arco = ", offset_per_arc, "\nrepeticoes = ", repetitions, "\nseries = ", series, "\ncomecar_na_borda = ", start_on_border, "\nduracao = ", duration);

    if (CFG(GigaMode) and duration) {
        L0_LOG("iniciado no modo giga");
        util::idle_for(chrono::milliseconds{ duration });
        L0_LOG("finalizado no modo giga");
        return;
    }

    const auto volume_of_water = parser.floatval('G');
    const auto should_pour = duration and volume_of_water;

    const bool associated_with_station = RecipeQueue::the().is_executing_recipe();
    bool dip = false;

    const auto initial_position = current_position;
    L0_LOG("initial_position.x: ", initial_position.x);

    auto final_position = initial_position;

    std::vector<float> diameters;
    diameters.reserve(number_of_arcs * series);

    for (auto serie = 0; serie < series; serie++) {
        const bool out_to_in = serie % 2 != start_on_border;
        for (auto arco = 0; arco < number_of_arcs; arco++) {
            const auto arc_offset = offset_per_arc * arco;
            float arc_diameter = out_to_in ? std::abs(arc_offset - total_diameter) : arc_offset + offset_per_arc;
            if (arco % 2)
                arc_diameter = -arc_diameter;

            diameters.emplace_back(arc_diameter);
        }
    }

    char buffer_radius[16] = {};
    dtostrf(radius, 0, 2, buffer_radius);

    if (start_on_border) {
        execute_fmt("G0 F10000 X-%s", buffer_radius);
        MotionController::the().finish_movements();
    }

    float total_to_move = 0.f;
    for (const auto diameter : diameters)
        total_to_move += (2.f * PI * std::abs(diameter / 2.f)) / 2.f;

    const auto steps_por_mm_ratio = duration ? MotionController::MS_PER_MM / (duration / total_to_move) : 1.f;

    soft_endstop._enabled = false;
    MotionController::the().change_step_ratio(steps_por_mm_ratio,
                                              steps_por_mm_ratio * MotionController::ANGLE_FIX);

    if (should_pour) {
        Spout::the().pour_with_desired_volume(duration, volume_of_water);

        // wait for the water to actually start pouring
        const auto flow = volume_of_water / (duration / 1000.f);
        const auto norm = util::normalize(flow, Spout::FlowController::FLOW_MIN, Spout::FlowController::FLOW_MAX);
        const auto lerp = chrono::duration<float, std::milli>(std::lerp(300.f, 100.f, norm));

        util::idle_for(lerp);
    }

    for (const auto diameter : diameters) {
        float radius = diameter / 2.f;

        char buffer_diameter[16] = {};
        dtostrf(diameter / steps_por_mm_ratio, 0, 2, buffer_diameter);

        char buffer_radius[16] = {};
        dtostrf(radius / steps_por_mm_ratio, 0, 2, buffer_radius);

        execute_fmt("G2 F5000 X%s I%s", buffer_diameter, buffer_radius);

        final_position.x += diameter;
        if (associated_with_station and not RecipeQueue::the().is_executing_recipe()) {
            L0_LOG("receita foi cancelada, abortando");
            dip = true;
            if (should_pour)
                Spout::the().end_pour();
            break;
        }
    }

    MotionController::the().finish_movements();

    current_position = final_position;
    L0_LOG("final_position.x = ", current_position.x);

    soft_endstop._enabled = true;
    MotionController::the().change_step_ratio(1.f);

    if (dip) {
        sync_plan_position();
        return;
    }

    if (series % 2 != start_on_border) {
        execute_fmt("G0 F10000 X%s", buffer_radius);
        MotionController::the().finish_movements();
    }

    current_position = initial_position;
    sync_plan_position();

    if (should_pour)
        util::idle_while([] { return Spout::the().pouring(); });
}
}
