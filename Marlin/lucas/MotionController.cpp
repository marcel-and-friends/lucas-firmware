#include "MotionController.h"
#include <lucas/lucas.h>
#include <lucas/cmd/cmd.h>
#include <lucas/Station.h>
#include <src/module/planner.h>

namespace lucas {
void MotionController::setup() {
    change_step_ratio(1.f);
    change_max_acceleration(5000);
}

void MotionController::travel_to_station(const Station& station, float offset) {
    travel_to_station(station.index(), offset);
}

void MotionController::travel_to_station(usize index, float offset) {
    if (m_current_location == index)
        return;

    LOG_IF(LogTravel, "viajando - [estacao = ", index, " | offset = ", offset, "]");

    const auto beginning = millis();
    const auto gcode = util::ff("G0 F25000 Y60 X%s", Station::absolute_position(index) + offset);
    cmd::execute_multiple("G90",
                          gcode,
                          "G91");
    finish_movements();
    m_current_location = index;

    LOG_IF(LogTravel, "chegou - [tempo = ", millis() - beginning, "ms]");
}

void MotionController::travel_to_sewer() {
    if (m_current_location == SEWER_LOCATION)
        return;

    LOG_IF(LogTravel, "viajando para o esgoto");

    const auto beginning = millis();
    cmd::execute_multiple("G90",
                          "G0 F5000 Y60 X0",
                          "G91");
    finish_movements();
    m_current_location = SEWER_LOCATION;

    LOG_IF(LogTravel, "chegou - [tempo = ", millis() - beginning, "ms]");
}

void MotionController::home() {
    m_current_location = INVALID_LOCATION;
    cmd::execute("G28 XY");
    finish_movements();
}

void MotionController::finish_movements() const {
    util::idle_while(&Planner::busy, core::Filter::RecipeQueue);
}

float MotionController::step_ratio_x() const {
    return planner.settings.axis_steps_per_mm[X_AXIS] / DEFAULT_STEPS_PER_MM_X;
}

float MotionController::step_ratio_y() const {
    return planner.settings.axis_steps_per_mm[Y_AXIS] / DEFAULT_STEPS_PER_MM_Y;
}

void MotionController::change_step_ratio(f32 ratio_x, f32 ratio_y) const {
    planner.settings.axis_steps_per_mm[X_AXIS] = DEFAULT_STEPS_PER_MM_X * ratio_x;
    planner.settings.axis_steps_per_mm[Y_AXIS] = DEFAULT_STEPS_PER_MM_Y * ratio_y;
    planner.refresh_positioning();
}

void MotionController::change_step_ratio(float ratio) const {
    change_step_ratio(ratio, ratio);
}

void MotionController::change_max_acceleration(f32 accel) const {
    planner.set_max_acceleration(X_AXIS, accel);
    planner.set_max_acceleration(Y_AXIS, accel);
    planner.settings.travel_acceleration = planner.settings.acceleration = planner.settings.retract_acceleration = accel;
}

void MotionController::toggle_motors_stress_test() {
    m_motor_stress_test = not m_motor_stress_test;
    while (m_motor_stress_test) {
        for (usize station = 0; station < 3; station++) {
            if (not m_motor_stress_test) {
                // Turn off all
                for (usize i = 0; i < 3; i++) {
                    digitalWrite(Station::list().at(i).led(), LOW);
                    digitalWrite(Station::list().at(i).powerled(), HIGH);
                }
                return;
            }

            auto previous_station = station == 0 ? 2 : station - 1;

            travel_to_station(station);

            // Turn off previous
            digitalWrite(Station::list().at(previous_station).led(), LOW);
            digitalWrite(Station::list().at(previous_station).powerled(), HIGH);

            // Turn off current
            digitalWrite(Station::list().at(station).led(), HIGH);
            digitalWrite(Station::list().at(station).powerled(), LOW);

            cmd::execute("L0 D8 R1 N4");
        }

        // Home after finished with the round
        cmd::execute("G28");
    }
}
}
