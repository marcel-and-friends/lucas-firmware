#include <lucas/cmd/cmd.h>
#include <lucas/storage/storage.h>
#include <src/gcode/parser.h>
#include <lucas/Spout.h>
#include <lucas/MotionController.h>

namespace lucas::cmd {
void L7() {
    if (parser.seenval('I')) {
        Spout::FlowController::MIN_ML_PER_PULSE = parser.value_float() ?: 0.5525;
    }

    if (parser.seenval('A')) {
        Spout::FlowController::MAX_ML_PER_PULSE = parser.value_float() ?: 0.5f;
    }

    if (parser.seenval('W')) {
        Spout::FlowController::the().update_flow_hint_for_pulse_calculation(parser.value_float());
    }

    if (parser.seenval('E'))
        storage::purge_entry(parser.value_int());

    if (parser.seen('C'))
        MotionController::the().toggle_motors_stress_test();
}
}
