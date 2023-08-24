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

        if (m_total_desired_volume == 0.f or m_flow_correction == CorrectFlow::No)
            return;

        constexpr auto FLOW_CORRECTION_INTERVAL = 1000;
        // not time to correct yet
        if (m_time_elapsed - m_time_of_last_correction < FLOW_CORRECTION_INTERVAL)
            return;

        const auto poured_so_far = (s_pulse_counter - m_pulses_at_start_of_pour) * FlowController::ML_PER_PULSE;
        // reached the goal too son
        if (poured_so_far >= m_total_desired_volume) {
            end_pour();
            return;
        }

        // when we've corrected at least once already, meaning the motor acceleration curve has been, theoretically, accounted-for
        // we can stat to track down discrepancies on every correction interval by comparing it to the ideal volume we should be at
        if (m_time_of_last_correction >= FLOW_CORRECTION_INTERVAL) {
            // no water? bad motor? bad sensor? bad pump? who knows!
            if (poured_so_far == 0.f)
                sec::raise_error(sec::Reason::PourVolumeMismatch, sec::NO_RETURN);

            const auto ideal_flow = m_total_desired_volume / (m_pour_duration / 1000.f);
            const auto expected_volume = (m_time_elapsed / 1000.f) * ideal_flow;
            const auto ratio = poured_so_far / expected_volume;
            // 50% is a pretty generous margin, could try to go lower
            constexpr auto POUR_ACCEPTABLE_MARGIN_OF_ERROR = 0.5f;
            if (std::abs(ratio - 1.f) >= POUR_ACCEPTABLE_MARGIN_OF_ERROR)
                sec::raise_error(sec::Reason::PourVolumeMismatch, sec::NO_RETURN);
        }

        // looks like nothing has gone wrong, calculate the ideal flow and fetch our best guess for it
        const auto ideal_flow = (m_total_desired_volume - poured_so_far) / ((m_pour_duration - m_time_elapsed) / 1000.f);
        send_digital_signal_to_driver(FlowController::the().hit_me_with_your_best_shot(ideal_flow));
        m_time_of_last_correction = m_time_elapsed;
    } else {
        // BRK is realeased after a little bit to not stress the motor too hard
        constexpr auto TIME_TO_DISABLE_BRK = 2000;
        if (m_tick_pour_ended) {
            if (util::elapsed<TIME_TO_DISABLE_BRK>(m_tick_pour_ended)) {
                digitalWrite(Pin::BRK, HIGH); // fly high 🕊️
                m_tick_pour_ended = 0;
            }
        }
    }
}

void Spout::pour_with_desired_volume(millis_t duration, float desired_volume, CorrectFlow correct) {
    if (duration == 0 or desired_volume == 0)
        return;

    begin_pour(duration);
    send_digital_signal_to_driver(FlowController::the().hit_me_with_your_best_shot(desired_volume / (duration / 1000)));
    m_total_desired_volume = desired_volume;
    m_flow_correction = correct;
    LOG_IF(LogPour, "iniciando despejo - [volume desejado = ", desired_volume, " | sinal = ", m_digital_signal, "]");
}

float Spout::pour_with_desired_volume_and_wait(millis_t duration, float desired_volume, CorrectFlow correct) {
    pour_with_desired_volume(duration, desired_volume, correct);
    util::idle_while([] { return Spout::the().pouring(); });
    return (m_pulses_at_end_of_pour - m_pulses_at_start_of_pour) * FlowController::ML_PER_PULSE;
}

void Spout::pour_with_digital_signal(millis_t duration, DigitalSignal digital_signal) {
    if (duration == 0 or digital_signal == 0)
        return;

    begin_pour(duration);
    send_digital_signal_to_driver(digital_signal);
    LOG_IF(LogPour, "iniciando despejo - [sinal = ", m_digital_signal, "]");
}

void Spout::end_pour() {
    send_digital_signal_to_driver(0);
    m_pouring = false;
    m_tick_pour_began = 0;
    m_tick_pour_ended = millis();
    m_pour_duration = 0;
    m_flow_correction = CorrectFlow::No;
    m_time_of_last_correction = 0;
    m_pulses_at_end_of_pour = s_pulse_counter;

    const auto pulses = m_pulses_at_end_of_pour - m_pulses_at_start_of_pour;
    const auto poured_volume = pulses * FlowController::ML_PER_PULSE;
    LOG_IF(LogPour, "finalizando despejo - [delta = ", m_time_elapsed, "ms | volume = ", poured_volume, " | pulsos = ", pulses, "]");
    m_total_desired_volume = 0.f;
    m_time_elapsed = 0;
}

void Spout::setup() {
    { // driver setup
        // 12 bit DAC pins!
        analogWriteResolution(12);

        pinMode(Pin::SV, OUTPUT);
        pinMode(Pin::BRK, OUTPUT);
        pinMode(Pin::EN, OUTPUT);
        // enable is permanently on,  the driver is controlled using only SV and BRK
        digitalWrite(Pin::EN, LOW);

        // let's make sure we turn everything off at startup :)
        end_pour();
    }

    { // flow sensor setup
        pinMode(Pin::FlowSensor, INPUT);
        attachInterrupt(
            digitalPinToInterrupt(Pin::FlowSensor),
            +[] {
                ++s_pulse_counter;
            },
            RISING);
    }
}

void Spout::travel_to_station(const Station& station, float offset) const {
    travel_to_station(station.index(), offset);
}

void Spout::travel_to_station(size_t index, float offset) const {
    LOG_IF(LogTravel, "viajando - [estacao = ", index, " | offset = ", offset, "]");

    const auto beginning = millis();
    const auto gcode = util::ff("G0 F50000 Y60 X%s", Station::absolute_position(index) + offset);
    cmd::execute_multiple("G90",
                          gcode,
                          "G91");
    finish_movements();

    LOG_IF(LogTravel, "chegou - [tempo = ", millis() - beginning, "ms]");
}

void Spout::travel_to_sewer() const {
    LOG_IF(LogTravel, "viajando para o esgoto");

    const auto beginning = millis();
    cmd::execute_multiple("G90",
                          "G0 F5000 Y60 X5",
                          "G91");
    finish_movements();

    LOG_IF(LogTravel, "chegou - [tempo = ", millis() - beginning, "ms]");
}

void Spout::home() const {
    cmd::execute("G28 XY");
}

void Spout::finish_movements() const {
    util::idle_while(&Planner::busy, TickFilter::RecipeQueue);
}

void Spout::send_digital_signal_to_driver(DigitalSignal v) {
    if (v == FlowController::INVALID_DIGITAL_SIGNAL) {
        LOG_ERR("sinal digital invalido, finalizando imediatamente");
        end_pour();
        return;
    }
    m_digital_signal = v;
    digitalWrite(Pin::BRK, m_digital_signal > 0 ? HIGH : LOW);
    analogWrite(Pin::SV, m_digital_signal);
}

void Spout::fill_hose(float desired_volume) {
    constexpr auto FALLBACK_SIGNAL = 1400;        // radomly picked this
    constexpr auto TIME_TO_FILL_HOSE = 1000 * 20; // 20 sec

    Spout::the().travel_to_sewer();

    if (desired_volume) {
        pour_with_desired_volume(TIME_TO_FILL_HOSE, desired_volume, CorrectFlow::Yes);
    } else {
        pour_with_digital_signal(TIME_TO_FILL_HOSE, FALLBACK_SIGNAL);
    }

    LOG_IF(LogCalibration, "preenchendo a mangueira com agua - [duracao = ", TIME_TO_FILL_HOSE / 1000, "s]");
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
    static_assert(TIME_TO_ANALISE_POUR >= 1000, "analysis time must be at least one minute");

    clean_digital_signal_table(SaveToFlash::No);

    const auto beginning = millis();
    // place the spout on the sink and fill the hose so we avoid silly errors
    Spout::the().fill_hose();

    // the signal sent to the driver on every iteration
    int digital_signal = INITIAL_DIGITAL_SIGNAL;
    // the amount by which that signal gets modified at the end of the current iteration
    // can be negative
    int digital_signal_mod = INITIAL_DIGITAL_SIGNAL_MOD;
    // the average flow of the last valid iteration
    float last_iteration_average_flow = 0.f;

    bool obtained_minimum_flow = false;
    bool was_greater_than_minimum_last_iteration = false;

    size_t number_of_occupied_cells = 0;

    // accepts normalized values between 0.f and 1.f
    const auto report_progress = [](float progress) {
        info::send(info::Event::Calibration, [progress](JsonObject o) {
            o["progress"] = progress;
        });
    };

    while (true) {
        // let's not fly too close to the sun
        if (digital_signal >= 4095)
            break;

        const auto starting_pulses = s_pulse_counter;

        LOG_IF(LogCalibration, "aplicando sinal = ", digital_signal);
        Spout::the().send_digital_signal_to_driver(digital_signal);

        util::idle_for(TIME_TO_ANALISE_POUR);

        const auto pulses = s_pulse_counter - starting_pulses;
        const auto average_flow = (pulses * ML_PER_PULSE) / (TIME_TO_ANALISE_POUR / 1000.f);

        LOG_IF(LogCalibration, "flow estabilizou - [pulsos = ", pulses, " | fluxo medio = ", average_flow, "]");

        { // 1. obtain the minimum flow
            if (not obtained_minimum_flow) {
                const auto delta = average_flow - FLOW_MIN;
                // a .1 gram delta is accepeted
                if (std::abs(average_flow - FLOW_MIN) <= 0.1) {
                    obtained_minimum_flow = true;
                    m_digital_signal_table[0][0] = digital_signal;

                    LOG_IF(LogCalibration, "flow minimo obtido - [sinal = ", digital_signal, "]");

                    digital_signal_mod = DIGITAL_SIGNAL_MOD_AFTER_OBTAINING_MIN_FLOW;
                    digital_signal += digital_signal_mod;

                    // now the iterative calibration process has properly startd
                    report_progress(0.f);
                }
                // otherwise modify the digital signal appropriately
                else {
                    const bool greater_than_minimum = average_flow > float(FLOW_MIN);
                    // this little algorithm makes us gravitates towards a specific value
                    // making it guaranteed that, even if it takes a little bit, we achieve
                    // at the correct digital signal
                    if (greater_than_minimum != was_greater_than_minimum_last_iteration) {
                        if (digital_signal_mod < 0) {
                            digital_signal_mod = std::min(digital_signal_mod / 2, -1);
                        } else {
                            digital_signal_mod = std::max(digital_signal_mod / 2, 1);
                        }
                        digital_signal_mod *= -1;
                    }

                    digital_signal += digital_signal_mod;

                    LOG_IF(LogCalibration, "flow minimo nao obtido - ", greater_than_minimum ? "diminuindo" : "aumentando", " o sinal digital para ", digital_signal, " - [modificacao = ", digital_signal_mod, "]");

                    was_greater_than_minimum_last_iteration = greater_than_minimum;

                    // before the minimum flow has been found we cannot achieve a value representing "progress"
                    // so we tell the host something else instead
                    info::send(info::Event::Calibration, [](JsonObject o) {
                        o["preparing"] = true;
                    });
                }
                continue;
            }
        }

        // if the flow decreases between iterations, the result is ignored
        if (last_iteration_average_flow and average_flow < last_iteration_average_flow) {
            digital_signal += digital_signal_mod;
            LOG_IF(LogCalibration, "flow diminuiu?! aumentando sinal digital - [last_iteration_average_flow = ", last_iteration_average_flow, "]");
            continue;
        }

        // too close to the sun!
        if (average_flow >= FLOW_MAX) {
            LOG_IF(LogCalibration, "chegamos no flow maximo, finalizando nivelamento");
            break;
        }

        { // let's make sure the jump from one iteartion to the other respects the maximum delta
            if (last_iteration_average_flow) {
                // no need to 'abs' this since we chcked that 'last_iteration_average_flow' is smaller
                const auto delta = average_flow - last_iteration_average_flow;
                // big jump! cut the modification and subtract it from the signal
                if (delta > MAX_DELTA_BETWEEN_POURS) {
                    digital_signal_mod = std::max(digital_signal_mod / 2, 1);
                    digital_signal -= digital_signal_mod;
                    LOG_IF(LogCalibration, "pulo muito grande de força - [delta = ", delta, " | nova modificacao = ", digital_signal_mod, "]");
                    continue;
                }
            }
        }

        { // it all went well in wonderland, let's report our progress and save the current signal
            const auto [rounded_volume, decimal] = decompose_flow(average_flow);
            const auto flow = float(rounded_volume) + (decimal / 10.f);
            report_progress(util::normalize(flow, FLOW_MIN, FLOW_MAX));

            m_digital_signal_table[rounded_volume - FLOW_MIN][decimal] = digital_signal;
            LOG_IF(LogCalibration, "sinal digital salvo - [valor = ", digital_signal, "]");

            // apply the modification, save the current flow and move on to the next iteration
            digital_signal += digital_signal_mod;
            last_iteration_average_flow = average_flow;
            number_of_occupied_cells++;
        }
    }

    // we're done!
    Spout::the().end_pour();
    report_progress(1.f);
    save_digital_signal_table_to_flash();

    LOG_IF(LogCalibration, "tabela preenchida - [duracao = ", (millis() - beginning) / 60000.f, "min | numero de celulas = ", number_of_occupied_cells, "]");
    LOG_IF(LogCalibration, "resultado da tabela: ");
    for_each_occupied_cell([](DigitalSignal digital_signal, size_t i, size_t j) {
        LOG_IF(LogCalibration, "m_digital_signal_table[", i, "][", j, "] = ", digital_signal, " = ", i + FLOW_MIN, ".", j, "g/s");
        return util::Iter::Continue;
    });
}

void Spout::FlowController::try_fetching_digital_signal_table_from_flash() {
    auto reader = mem::FlashReader<DigitalSignal>(0);
    // o flow minimo (m_digital_signal_table[0][0]) é o único valor obrigatoriamente salvo durante o preenchimento
    const auto possible_stored_first_cell = reader.read(0);
    if (possible_stored_first_cell == 0 or
        possible_stored_first_cell >= INVALID_DIGITAL_SIGNAL)
        // nao temos um valor salvo
        return;

    LOG_IF(LogCalibration, "tabela de flow sera copiada da flash - [minimo = ", possible_stored_first_cell, "]");
    fetch_digital_signal_table_from_flash();
}

// this takes a desired flow and tries to guess the best digital signal to achieve that
Spout::DigitalSignal Spout::FlowController::hit_me_with_your_best_shot(float flow) const {
    const auto [rounded_flow, decimal] = decompose_flow(flow);
    const auto flow_index = rounded_flow - FLOW_MIN;
    auto& stored_value = m_digital_signal_table[flow_index][decimal];
    // if we already have the right value on the table just return that, no need to get fancy
    if (stored_value != INVALID_DIGITAL_SIGNAL)
        return stored_value;

    // otherwise we get the two closest values above and below the desired flow
    // then interpolate between them
    // it works surprisignly well!
    const auto [flow_below, digital_signal_below] = first_flow_info_below_flow(flow_index, decimal);
    const auto [flow_above, digital_signal_above] = first_flow_info_above_flow(flow_index, decimal);

    // if one, or both, are invalid then we can't interpolate
    if (digital_signal_below == INVALID_DIGITAL_SIGNAL) {
        return digital_signal_above;
    } else if (digital_signal_above == INVALID_DIGITAL_SIGNAL) {
        return digital_signal_below;
    }

    const auto normalized = util::normalize(flow, flow_below, flow_above);
    const auto interpolated = std::lerp(float(digital_signal_below), float(digital_signal_above), normalized);
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
    const auto cells = reinterpret_cast<DigitalSignal*>(&m_digital_signal_table);
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
                const auto flow = float(flow_index + FLOW_MIN) + float(decimal) / 10.f;
                return { flow, cells[decimal] };
            }
        }
    }

    return { 0.f, INVALID_DIGITAL_SIGNAL };
}

Spout::FlowController::FlowInfo Spout::FlowController::first_flow_info_above_flow(int flow_index, int decimal) const {
    for (; flow_index < FLOW_RANGE; ++flow_index, decimal = 0) {
        auto& cells = m_digital_signal_table[flow_index];
        for (; decimal < 10; ++decimal) {
            if (cells[decimal] != INVALID_DIGITAL_SIGNAL) {
                const auto flow = float(flow_index + FLOW_MIN) + float(decimal) / 10.f;
                return { flow, cells[decimal] };
            }
        }
    }

    return { 0.f, INVALID_DIGITAL_SIGNAL };
}

Spout::FlowController::DecomposedFlow Spout::FlowController::decompose_flow(float flow) const {
    const auto rounded = std::round(flow * 10.f) / 10.f;
    const auto clamped = std::clamp(rounded, float(FLOW_MIN), float(FLOW_MAX) - 0.1f);
    return { int(clamped), int(clamped * 10) % 10 };
}
}
