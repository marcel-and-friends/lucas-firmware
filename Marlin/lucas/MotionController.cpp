#include "MotionController.h"
#include <lucas/lucas.h>
#include <lucas/cmd/cmd.h>
#include <lucas/Station.h>
#include <src/module/planner.h>

namespace lucas {
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
                          "G0 F5000 Y60 X5",
                          "G91");
    finish_movements();
    m_current_location = SEWER_LOCATION;

    LOG_IF(LogTravel, "chegou - [tempo = ", millis() - beginning, "ms]");
}

void MotionController::home() {
    m_current_location = INVALID_LOCATION;
    cmd::execute("G28 XY");
}

static constexpr float MS_PER_MM = 12.41f;
static constexpr float DEFAULT_STEPS_PER_MM_X = 44.5f;
static constexpr float DEFAULT_STEPS_PER_MM_Y = 22.5f;

void MotionController::finish_movements() const {
    util::idle_while(&Planner::busy, core::Filter::RecipeQueue);
}

float MotionController::step_ratio_x() const {
    return planner.settings.axis_steps_per_mm[X_AXIS] / DEFAULT_STEPS_PER_MM_X;
}

float MotionController::step_ratio_y() const {
    return planner.settings.axis_steps_per_mm[Y_AXIS] / DEFAULT_STEPS_PER_MM_Y;
}
}
