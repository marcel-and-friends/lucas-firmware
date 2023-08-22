#include "Spout.h"
#include <lucas/Station.h>
#include <lucas/lucas.h>
#include <lucas/cmd/cmd.h>
#include <lucas/info/info.h>
#include <lucas/mem/FlashReader.h>
#include <lucas/mem/FlashWriter.h>
#include <lucas/sec/sec.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>

namespace lucas {
enum Pin {
    SV = PA5,
    EN = PE10,
    BRK = PA6,
    FlowSensor = PA13
};

void Spout::tick() {
    if (m_pouring) {
        m_time_elapsed = millis() - m_tick_pour_began;
        if (m_time_elapsed >= m_pour_duration) {
            end_pour();
            return;
        }

        constexpr auto FLOW_CORRECTION_INTERVAL = 1000;
        if (m_time_elapsed - m_last_flow_correction_tick >= FLOW_CORRECTION_INTERVAL) {
            auto const poured_so_far = (s_pulse_counter - m_pulses_at_start_of_pour) * FlowController::ML_PER_PULSE;
            if (m_total_desired_volume and m_flow_correction == CorrectFlow::Yes) {
                auto const ideal_flow = (m_total_desired_volume - poured_so_far) / ((m_pour_duration - m_time_elapsed) / 1000);
                send_digital_signal_to_driver(FlowController::the().best_digital_signal_for_flow(ideal_flow));
                m_last_flow_correction_tick = m_time_elapsed;
            }
        }
    } else {
        // após end_pour o motor deixamos o break ativo por um tempinho e depois liberamos
        constexpr auto TIME_TO_DISABLE_BRK = 2000; // 2s
        if (m_tick_pour_ended) {
            if (util::elapsed<TIME_TO_DISABLE_BRK>(m_tick_pour_ended)) {
                digitalWrite(Pin::BRK, HIGH);
                m_tick_pour_ended = 0;
            }
        }
    }
}

void Spout::pour_with_desired_volume(millis_t duration, float desired_volume, CorrectFlow correct) {
    if (duration == 0 or desired_volume == 0)
        return;

    begin_pour(duration);
    send_digital_signal_to_driver(FlowController::the().best_digital_signal_for_flow(desired_volume / (duration / 1000)));
    m_total_desired_volume = desired_volume;
    m_flow_correction = correct;
    LOG_IF(LogPour, "iniciando despejo - [volume desejado = ", desired_volume, " | forca = ", m_digital_signal, "]");
}

float Spout::pour_with_desired_volume_and_wait(millis_t duration, float desired_volume, CorrectFlow correct) {
    pour_with_desired_volume(duration, desired_volume, correct);
    util::idle_while([] { return Spout::the().pouring(); }, Filters::Interaction);
    return (m_pulses_at_end_of_pour - m_pulses_at_start_of_pour) * FlowController::ML_PER_PULSE;
}

void Spout::pour_with_digital_signal(millis_t duration, DigitalSignal digital_signal) {
    if (duration == 0 or digital_signal == 0)
        return;

    begin_pour(duration);
    send_digital_signal_to_driver(digital_signal);
    LOG_IF(LogPour, "iniciando despejo - [forca = ", m_digital_signal, "]");
}

void Spout::end_pour() {
    send_digital_signal_to_driver(0);
    m_pouring = false;
    m_tick_pour_began = 0;
    m_tick_pour_ended = millis();
    m_pour_duration = 0;
    m_flow_correction = CorrectFlow::No;
    m_last_flow_correction_tick = 0;
    m_pulses_at_end_of_pour = s_pulse_counter;
    if (m_total_desired_volume) {
        auto const pulses = m_pulses_at_end_of_pour - m_pulses_at_start_of_pour;
        auto const poured_volume = pulses * FlowController::ML_PER_PULSE;
        // TODO: security check
        LOG_IF(LogPour, "finalizando despejo - [delta = ", m_time_elapsed, "ms | volume = ", poured_volume, " | pulses = ", pulses, "]");
        m_total_desired_volume = 0.f;
        m_time_elapsed = 0;
    }
}

void Spout::setup() {
    // nossos pinos DAC tem resolução de 12 bits
    analogWriteResolution(12);

    pinMode(Pin::SV, OUTPUT);
    pinMode(Pin::BRK, OUTPUT);
    pinMode(Pin::EN, OUTPUT);
    pinMode(Pin::FlowSensor, INPUT);

    digitalWrite(Pin::EN, LOW);
    end_pour();

    attachInterrupt(
        digitalPinToInterrupt(Pin::FlowSensor),
        +[] {
            ++s_pulse_counter;
        },
        RISING);
}

void Spout::travel_to_station(Station const& station, float offset) const {
    travel_to_station(station.index(), offset);
}

void Spout::travel_to_station(size_t index, float offset) const {
    LOG_IF(LogTravel, "viajando - [station = ", index, " | offset = ", offset, "]");

    auto const beginning = millis();
    auto const gcode = util::ff("G0 F50000 Y60 X%s", Station::absolute_position(index) + offset);
    cmd::execute_multiple("G90",
                          gcode,
                          "G91");
    finish_movements();

    LOG_IF(LogTravel, "viagem completa - [duration = ", millis() - beginning, "ms]");
}

void Spout::travel_to_sewer() const {
    LOG_IF(LogTravel, "viajando para o esgoto");

    auto const beginning = millis();
    cmd::execute_multiple("G90",
                          "G0 F5000 Y60 X5",
                          "G91");
    finish_movements();

    LOG_IF(LogTravel, "viagem completa - [duration = ", millis() - beginning, "ms]");
}

void Spout::home() const {
    cmd::execute("G28 XY");
}

void Spout::finish_movements() const {
    util::idle_while(&Planner::busy, Filters::RecipeQueue);
}

void Spout::send_digital_signal_to_driver(DigitalSignal v) {
    if (v == FlowController::INVALID_DIGITAL_SIGNAL) {
        LOG_ERR("tentando aplicar forca digital invalido, desligando");
        end_pour();
        return;
    }
    m_digital_signal = v;
    digitalWrite(Pin::BRK, m_digital_signal > 0 ? HIGH : LOW);
    analogWrite(Pin::SV, m_digital_signal);
}

void Spout::fill_hose(float desired_volume) {
    constexpr auto FALLBACK_SIGNAL = 1400;
    constexpr auto TIME_TO_FILL_HOSE = 1000 * 20; // 20 seg

    Spout::the().travel_to_sewer();

    if (desired_volume) {
        pour_with_desired_volume(TIME_TO_FILL_HOSE, desired_volume, CorrectFlow::Yes);
    } else {
        pour_with_digital_signal(TIME_TO_FILL_HOSE, FALLBACK_SIGNAL);
    }

    LOG_IF(LogCalibration, "preenchendo a mangueira com agua - [duration = ", TIME_TO_FILL_HOSE / 1000, "s]");
    util::idle_for(TIME_TO_FILL_HOSE);
}

void Spout::begin_pour(millis_t duration) {
    m_pouring = true;
    m_tick_pour_began = millis();
    m_pour_duration = duration;
    m_pulses_at_start_of_pour = s_pulse_counter;
}

void Spout::FlowController::fill_digital_signal_table() {
    constexpr auto INITIAL_DIGITAL_SIGNAL = 0;
    constexpr auto INITIAL_DIGITAL_SIGNAL_MOD = 200;
    constexpr auto DIGITAL_SIGNAL_MOD_AFTER_OBTAINING_MIN_FLOW = 25;
    constexpr auto MAX_DELTA_BETWEEN_POURS = 1.f;

    constexpr auto TIME_TO_ANALISE_POUR = 1000 * 10;
    static_assert(TIME_TO_ANALISE_POUR >= 1000, "tempo de analise deve ser no minimo 1 minuto");

    clean_digital_signal_table(SaveToFlash::No);

    auto const beginning = millis();

    // prepara o bico na posição correta e preenche a mangueira com água quente, para evitarmos resultados erroneos
    Spout::the().fill_hose();

    // valor enviado para o driver do motor
    int digital_signal = INITIAL_DIGITAL_SIGNAL;
    // modificacao aplicada nesse valor em cada iteração do nivelamento
    int digital_signal_mod = INITIAL_DIGITAL_SIGNAL_MOD;
    // o flow medio do ultimo despejo, usado para calcularmos o delta entre despejos e modificarmos o forca digital apropriadamente
    float last_pour_average_flow = 0.f;
    // flag para sabermos se ja temos o valor minimo e podemos prosseguir com o restante do nivelamento
    bool obtained_minimum_flow = false;
    // flag que diz se o ultimo despejo em busca do flow minimo foi maior ou menor que o flow minimo
    bool last_greater_minimum = false;

    // accepts normalized values between 0.f and 1.f
    auto const report_progress = [](float progress) {
        info::event("infoNivelamento", [progress](JsonObject o) {
            o["progresso"] = progress;
        });
    };

    while (true) {
        // chegamos no limite, não podemos mais aumentar a força
        if (digital_signal >= 4095)
            break;

        auto const starting_pulses = s_pulse_counter;

        LOG_IF(LogCalibration, "aplicando forca = ", digital_signal);
        Spout::the().send_digital_signal_to_driver(digital_signal);

        util::idle_for(TIME_TO_ANALISE_POUR);

        auto const pulses = s_pulse_counter - starting_pulses;
        auto const average_flow = (float(pulses) * ML_PER_PULSE) / float(TIME_TO_ANALISE_POUR / 1000);

        LOG_IF(LogCalibration, "flow estabilizou - [pulses = ", pulses, " | average_flow = ", average_flow, "]");

        { // primeiro precisamos obter o flow minimo
            if (not obtained_minimum_flow) {
                auto const delta = average_flow - float(FLOW_MIN);
                // valores entre FLOW_MIN - 0.1 e FLOW_MIN + 0.1 são aceitados
                // caso contrario, aumentamos ou diminuimos o forca digital ate chegarmos no flow minimo
                if (delta > -0.1f and delta < 0.1f) {
                    obtained_minimum_flow = true;
                    m_digital_signal_table[0][0] = digital_signal;

                    LOG_IF(LogCalibration, "flow minimo obtido - [digital_signal = ", digital_signal, "]");

                    digital_signal_mod = DIGITAL_SIGNAL_MOD_AFTER_OBTAINING_MIN_FLOW;
                    digital_signal += digital_signal_mod;

                    report_progress(0.f);
                } else {
                    bool const greater_min = average_flow > float(FLOW_MIN);
                    if (greater_min != last_greater_minimum) {
                        if (digital_signal_mod < 0)
                            digital_signal_mod = std::min(digital_signal_mod / 2, -1);
                        else
                            digital_signal_mod = std::max(digital_signal_mod / 2, 1);
                        digital_signal_mod *= -1;
                    }

                    digital_signal += digital_signal_mod;

                    LOG_IF(LogCalibration, "flow minimo nao obtido - ", greater_min ? "diminuindo" : "aumentando", " o forca digital para ", digital_signal, " - [modificacao = ", digital_signal_mod, "]");

                    last_greater_minimum = greater_min;

                    info::event("preNivelamento", [](JsonObject o) {});
                }
                continue;
            }
        }

        // se o flow diminui entre um despejo e outro o resultado é ignorado
        if (last_pour_average_flow and average_flow < last_pour_average_flow) {
            digital_signal += digital_signal_mod;
            LOG_IF(LogCalibration, "flow diminuiu?! aumentando forca digital - [last_pour_average_flow = ", last_pour_average_flow, "]");
            continue;
        }

        // se o flow ultrapassa o limite máximo, finalizamos o nivelamento
        if (average_flow >= FLOW_MAX) {
            LOG_IF(LogCalibration, "chegamos no flow maximo, finalizando nivelamento");
            break;
        }

        { // precisamos garantir que o flow não deu um pulo muito grande entre esse e o último despejo, para termos uma quantidade maior de valores salvos na tabela
            if (last_pour_average_flow) {
                auto const delta = average_flow - last_pour_average_flow;
                // o pulo foi muito grande, corta a modificacao no meio e reduz o forca digital
                if (delta > MAX_DELTA_BETWEEN_POURS) {
                    digital_signal_mod = std::max(digital_signal_mod / 2, 1);
                    digital_signal -= digital_signal_mod;
                    LOG_IF(LogCalibration, "pulo muito grande de força - [delta = ", delta, " | nova modificacao = ", digital_signal_mod, "]");
                    continue;
                }
            }
        }

        { // salvamos a forca digital, aplicamos a modificação e prosseguimos com o proximo despejo
            last_pour_average_flow = average_flow;
            auto const [rounded_volume, decimal] = decompose_flow(average_flow);
            auto const flow = float(rounded_volume) + (decimal / 10.f);
            report_progress(util::normalize(flow, FLOW_MIN, FLOW_MAX));

            auto& cell = m_digital_signal_table[rounded_volume - FLOW_MIN][decimal];
            if (cell == INVALID_DIGITAL_SIGNAL) {
                cell = digital_signal;
                LOG_IF(LogCalibration, "forca digital salvo - [valor = ", digital_signal, "]");
            }

            digital_signal += digital_signal_mod;

            LOG_IF(LogCalibration, "analise completa");
        }
    }

    Spout::the().end_pour();

    report_progress(1.f);

    save_digital_signal_table_to_flash();

    LOG_IF(LogCalibration, "tabela preenchida - [duration = ", (millis() - beginning) / 60000.f, "min | numero de celulas = ", number_of_ocuppied_cells(), "]");

    LOG_IF(LogCalibration, "resultado da tabela: ");
    for_each_occupied_cell([](DigitalSignal digital_signal, size_t i, size_t j) {
        LOG_IF(LogCalibration, "m_digital_signal_table[", i, "][", j, "] = ", digital_signal, " = ", i + FLOW_MIN, ".", j, "g/s");
        return util::Iter::Continue;
    });
}

void Spout::FlowController::try_fetching_digital_signal_table_from_flash() {
    auto reader = mem::FlashReader<DigitalSignal>(0);
    // o flow minimo (m_digital_signal_table[0][0]) é o único valor obrigatoriamente salvo durante o preenchimento
    auto const possible_stored_first_cell = reader.read(0);
    if (possible_stored_first_cell == 0 or
        possible_stored_first_cell >= INVALID_DIGITAL_SIGNAL)
        // nao temos um valor salvo
        return;

    LOG_IF(LogCalibration, "tabela de flow sera copiada da flash - [minimo = ", possible_stored_first_cell, "]");
    fetch_digital_signal_table_from_flash();
}

Spout::DigitalSignal Spout::FlowController::best_digital_signal_for_flow(float flow) const {
    auto const [rounded_flow, decimal] = decompose_flow(flow);
    auto const flow_index = rounded_flow - FLOW_MIN;
    auto& cells = m_digital_signal_table[flow_index];
    if (cells[decimal] != INVALID_DIGITAL_SIGNAL)
        // ótimo, já temos um valor salvo para esse flow
        return cells[decimal];

    // caso contrário, interpolamos entre os dois mais próximos
    auto const [flow_below, digital_signal_below] = first_flow_info_below_flow(flow_index, decimal);
    auto const [flow_above, digital_signal_above] = first_flow_info_above_flow(flow_index, decimal);

    if (digital_signal_below == INVALID_DIGITAL_SIGNAL) {
        return digital_signal_above;
    } else if (digital_signal_above == INVALID_DIGITAL_SIGNAL) {
        return digital_signal_below;
    }

    auto const normalized = util::normalize(flow, flow_below, flow_above);
    auto const interpolated = std::lerp(float(digital_signal_below), float(digital_signal_above), normalized);
    return DigitalSignal(std::round(interpolated));
}

void Spout::FlowController::clean_digital_signal_table(SaveToFlash salvar) {
    for (auto& t : m_digital_signal_table)
        for (auto& v : t)
            v = INVALID_DIGITAL_SIGNAL;

    if (salvar == SaveToFlash::Yes)
        save_digital_signal_table_to_flash();
}

void Spout::FlowController::save_digital_signal_table_to_flash() {
    auto writer = mem::FlashWriter<DigitalSignal>(0);
    auto const cells = reinterpret_cast<DigitalSignal*>(&m_digital_signal_table);
    for (size_t i = 0; i < NUMBER_OF_CELLS; ++i)
        writer.write(i, cells[i]);
}

void Spout::FlowController::fetch_digital_signal_table_from_flash() {
    auto reader = mem::FlashReader<DigitalSignal>(0);
    auto cells = reinterpret_cast<DigitalSignal*>(&m_digital_signal_table);
    for (size_t i = 0; i < NUMBER_OF_CELLS; ++i)
        cells[i] = reader.read(i);
}

Spout::FlowController::FlowInfo Spout::FlowController::first_flow_info_below_flow(int flow_index, int decimal) const {
    for (; flow_index >= 0; --flow_index, decimal = 9) {
        auto& cells = m_digital_signal_table[flow_index];
        for (; decimal >= 0; --decimal) {
            if (cells[decimal] != INVALID_DIGITAL_SIGNAL) {
                auto const flow = float(flow_index + FLOW_MIN) + float(decimal) / 10.f;
                return { flow, cells[decimal] };
            }
        }
    }

    return { 0.f, INVALID_DIGITAL_SIGNAL };
}

Spout::FlowController::FlowInfo Spout::FlowController::first_flow_info_above_flow(int flow_index, int decimal) const {
    for (; flow_index < RANGE_FLOW; ++flow_index, decimal = 0) {
        auto& cells = m_digital_signal_table[flow_index];
        for (; decimal < 10; ++decimal) {
            if (cells[decimal] != INVALID_DIGITAL_SIGNAL) {
                auto const flow = float(flow_index + FLOW_MIN) + float(decimal) / 10.f;
                return { flow, cells[decimal] };
            }
        }
    }

    return { 0.f, INVALID_DIGITAL_SIGNAL };
}

Spout::FlowController::DecomposedFlow Spout::FlowController::decompose_flow(float flow) const {
    auto const rounded = std::round(flow * 10.f) / 10.f;
    auto const clamped_flow = std::clamp(rounded, float(FLOW_MIN), float(FLOW_MAX) - 0.1f);
    return { int(clamped_flow), int(clamped_flow * 10) % 10 };
}
}
