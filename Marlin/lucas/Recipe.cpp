#include "Recipe.h"
#include <lucas/cmd/cmd.h>
#include <lucas/lucas.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>
#include <numeric>

namespace lucas {
JsonObjectConst Recipe::standard() {
    // calculado com o Assistant do ArduinoJson
    constexpr size_t BUFFER_SIZE = 384;
    static StaticJsonDocument<BUFFER_SIZE> doc;

    static char json[] = R"({
  "id": 61680,
  "tempoFinalizacao": 60000,
  "escaldo": {
    "duracao": 6000,
    "gcode": "L0 D9 N3 R1 T6000 G80"
  },
  "ataques": [
    {
      "duracao": 6000,
      "gcode": "L0 D7 N3 R1 T6000 G60",
      "intervalo": 24000
    },
    {
      "duracao": 9000,
      "gcode": "L0 D7 N5 R1 T9000 G90",
      "intervalo": 30000
    },
    {
      "duracao": 10000,
      "gcode": "L0 D7 N5 R1 T10000 G100",
      "intervalo": 35000
    },
    {
      "duracao": 9000,
      "gcode": "L0 D7 N5 R1 T9000 G100"
    }
  ]
})";

    static bool once = false;
    if (not once) {
        auto const err = deserializeJson(doc, json, sizeof(json));
        if (err) {
            LOG_ERR("DESSERIALIZACAO DA RECEITA PADRAO FALHOU - [", err.c_str(), "]");
            kill();
        }
        once = true;
    }

    return doc.as<JsonObjectConst>();
}

bool Recipe::Step::collides_with(Step const& other) const {
    // dois ataques colidem se:

    // 1. um comeca dentro do outro
    if (this->starting_tick >= other.starting_tick and this->starting_tick <= other.ending_tick())
        return true;

    // 2. um termina dentro do outro
    if (this->ending_tick() >= other.starting_tick and this->ending_tick() <= other.ending_tick())
        return true;

    // 3. a distancia entre o comeco de um e o fim do outro é menor que o tempo de viagem de uma station à outra
    if (this->starting_tick > other.ending_tick() and this->starting_tick - other.ending_tick() < util::TRAVEL_MARGIN)
        return true;

    // 4. a distancia entre o fim de um e o comeco do outro é menor que o tempo de viagem de uma station à outra
    if (this->ending_tick() < other.starting_tick and other.starting_tick - this->ending_tick() < util::TRAVEL_MARGIN)
        return true;

    return false;
}

void Recipe::build_from_json(JsonObjectConst json) {
    reset();

    m_id = json["id"].as<uint32_t>();
    m_finalization_duration = json["tempoFinalizacao"].as<millis_t>();

    // o escaldo é opcional
    if (json.containsKey("escaldo")) {
        auto scalding_obj = json["escaldo"].as<JsonObjectConst>();
        auto& scalding_step = m_steps[0];

        scalding_step.duration = scalding_obj["duracao"].as<millis_t>();
        strcpy(scalding_step.gcode, scalding_obj["gcode"].as<char const*>());

        m_has_scalding_step = true;
    }

    auto attacks_obj = json["ataques"].as<JsonArrayConst>();
    for (size_t i = 0; i < attacks_obj.size(); ++i) {
        auto attack_obj = attacks_obj[i];
        auto& attack = m_steps[i + m_has_scalding_step];

        attack.duration = attack_obj["duracao"].as<millis_t>();
        attack.interval = attack_obj.containsKey("intervalo") ? attack_obj["intervalo"].as<millis_t>() : 0;
        strcpy(attack.gcode, attack_obj["gcode"].as<char const*>());
    }

    m_steps_size = attacks_obj.size() + m_has_scalding_step;
}

void Recipe::reset() {
    m_id = 0;

    m_has_scalding_step = false;

    m_finalization_duration = 0;
    m_start_finalization_duration = 0;

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
