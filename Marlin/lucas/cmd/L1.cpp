#include <lucas/cmd/cmd.h>
#include <lucas/Spout.h>
#include <lucas/RecipeQueue.h>
#include <lucas/MotionController.h>
#include <src/gcode/gcode.h>
#include <src/gcode/parser.h>
#include <src/module/planner.h>

namespace lucas::cmd {
void L1() {
    // o diametro é passado em cm, porem o marlin trabalho com mm
    const auto circle_diameter = (parser.floatval('D') / MotionController::the().step_ratio_x()) * 10.f;
    if (circle_diameter == 0.f)
        return;

    const auto circle_radius = circle_diameter / 2.f;
    const auto number_of_repetitions = parser.intval('N');
    if (number_of_repetitions == 0)
        return;

    // cada circulo é composto por 2 arcos
    const auto number_of_arcs = number_of_repetitions * 2;
    const auto duration = parser.ulongval('T');

    if (CFG(GigaMode) and duration) {
        LOG_IF(LogLn, "iniciando L1 em modo giga");
        util::idle_for(chrono::milliseconds{ duration });
        LOG_IF(LogLn, "L1 finalizado");
        return;
    }

    const auto volume_of_water = parser.floatval('G');
    const auto should_pour = duration and volume_of_water;
    const bool associated_with_station = RecipeQueue::the().is_executing_recipe();
    bool dip = false;

    const auto initial_position = current_position;
    auto final_position = initial_position;

    execute_ff("G0 F10000 X%s", -circle_radius);
    MotionController::the().finish_movements();

    const float total_to_move = (2.f * std::numbers::pi_v<float> * circle_radius) * number_of_repetitions;
    const float steps_por_mm_ratio = duration ? MotionController::MS_PER_MM / (duration / total_to_move) : 1.f;

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

    for (s32 i = 0; i < number_of_arcs; i++) {
        float diameter = circle_diameter;
        if (i % 2 != 0)
            diameter = -diameter;

        float radius = diameter / 2.f;

        final_position.x = initial_position.x + radius;

        char buffer_diameter[16] = {};
        dtostrf(diameter / steps_por_mm_ratio, 0, 2, buffer_diameter);

        char buffer_radius[16] = {};
        dtostrf(radius / steps_por_mm_ratio, 0, 2, buffer_radius);

        execute_fmt("G2 F5000 X%s I%s", buffer_diameter, buffer_radius);

        if (associated_with_station and not RecipeQueue::the().is_executing_recipe()) {
            dip = true;
            if (should_pour)
                Spout::the().end_pour();
            break;
        }
    }

    MotionController::the().finish_movements();

    current_position = final_position;
    soft_endstop._enabled = true;
    MotionController::the().change_step_ratio(1.f);

    if (dip) {
        sync_plan_position();
        return;
    }

    const auto offset = initial_position.x - final_position.x;
    execute_ff("G0 F10000 X%s", offset);
    MotionController::the().finish_movements();

    current_position = initial_position;
    sync_plan_position();

    if (should_pour)
        util::idle_while([] { return Spout::the().pouring(); });
}
}
