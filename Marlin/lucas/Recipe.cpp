#include "Recipe.h"
#include <lucas/cmd/cmd.h>
#include <lucas/lucas.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>
#include <src/gcode/gcode.h>

namespace lucas {
JsonObjectConst Recipe::standard() {
    // calculado com o Assistant do ArduinoJson
    constexpr usize BUFFER_SIZE = 384;
    static StaticJsonDocument<BUFFER_SIZE> doc;

    static char json[] = R"({
  "station": 0,
  "recipe": {
    "id": 61680,
    "finalizationTime": 60000,
    "scald": {
        "duration": 6000,
        "gcode": "L0 D9 N3 R1 T6000 G80"
    },
    "attacks": [
        {
        "duration": 6000,
        "gcode": "L0 D7 N3 R1 T6000 G60",
        "interval": 24000
        },
        {
        "duration": 9000,
        "gcode": "L0 D7 N5 R1 T9000 G90",
        "interval": 30000
        },
        {
        "duration": 10000,
        "gcode": "L0 D7 N5 R1 T10000 G100",
        "interval": 35000
        },
        {
        "duration": 9000,
        "gcode": "L0 D7 N5 R1 T9000 G100"
        }
    ]
    }
})";

    static bool once = false;
    if (not once) {
        const auto err = deserializeJson(doc, json, sizeof(json));
        if (err) {
            LOG_ERR("DESSERIALIZACAO DA RECEITA PADRAO FALHOU - [", err.c_str(), "]");
            kill();
        }
        once = true;
    }

    return doc.as<JsonObjectConst>();
}

bool Recipe::Step::collides_with(const Step& that) const {
    if (this->starting_tick == that.starting_tick or
        this->ending_tick() == that.ending_tick())
        return true;

    if (this->starting_tick < that.starting_tick) {
        if (this->ending_tick() >= that.starting_tick)
            return true;

        if (that.starting_tick - this->starting_tick < util::TRAVEL_MARGIN)
            return true;
    } else {
        if (this->starting_tick <= that.ending_tick())
            return true;

        if (this->ending_tick() - that.ending_tick() < util::TRAVEL_MARGIN)
            return true;
    }

    return false;
}

void Recipe::build_from_json(JsonObjectConst json) {
    reset();

    m_id = json["id"].as<Id>();
    m_finalization_duration = chrono::milliseconds{ json["finalizationTime"].as<millis_t>() };

    // o escaldo é opcional
    if (json.containsKey("scald")) {
        auto scalding_obj = json["scald"].as<JsonObjectConst>();
        auto& scalding_step = m_steps[0];

        scalding_step.duration = scalding_obj["duration"].as<millis_t>();
        strcpy(scalding_step.gcode, scalding_obj["gcode"].as<const char*>());

        m_has_scalding_step = true;
    }

    auto attacks_obj = json["attacks"].as<JsonArrayConst>();
    for (usize i = 0; i < attacks_obj.size(); ++i) {
        auto attack_obj = attacks_obj[i];
        auto& attack = m_steps[i + m_has_scalding_step];

        attack.duration = attack_obj["duration"].as<millis_t>();
        attack.interval = attack_obj.containsKey("interval") ? attack_obj["interval"].as<millis_t>() : 0;
        strcpy(attack.gcode, attack_obj["gcode"].as<const char*>());
    }

    m_steps_size = attacks_obj.size() + m_has_scalding_step;
}

void Recipe::reset() {
    m_id = 0;
    m_has_scalding_step = false;
    m_finalization_duration = {};
    m_finalization_timer.stop();
    m_steps = {};
    m_steps_size = 0;
    m_current_step = 0;
}

void Recipe::execute_current_step() {
    GcodeSuite::process_subcommands_now(F(m_steps[m_current_step].gcode));
    // é muito importante esse número só incrementar APÓS a execucão do step
    m_current_step++;
}

void Recipe::map_remaining_steps(millis_t tick) {
    for_each_remaining_step([&](Step& step) {
        step.starting_tick = tick;
        tick += step.duration + step.interval;
        return util::Iter::Continue;
    });
}

void Recipe::unmap_steps() {
    for (auto& step : m_steps)
        step.starting_tick = 0;
}

bool Recipe::remaining_steps_are_mapped() const {
    bool result = false;
    for_each_remaining_step([&](auto& step) {
        if (step.starting_tick != 0) {
            result = true;
            return util::Iter::Break;
        }
        return util::Iter::Continue;
    });
    return result;
}
}
