#include "Spout.h"
#include <lucas/lucas.h>
#include <lucas/Station.h>
#include <lucas/Boiler.h>
#include <lucas/MotionController.h>
#include <lucas/RecipeQueue.h>
#include <lucas/info/info.h>

#include <lucas/sec/sec.h>
#include <src/module/temperature.h>
#include <src/module/planner.h>

namespace lucas {
void Spout::tick() {
    if (m_pouring) {
        const auto time_elapsed = [this] {
            return m_begin_pour_timer.elapsed();
        };

        if (time_elapsed() >= m_pour_duration) {
            end_pour();
            return;
        }

        // without a desired volume we can't correct the flow
        // and we only start correcting after 1 full second of pouring, to give the motor enough time to spin-up
        if (not m_total_desired_volume or time_elapsed() <= 1s)
            return;

        constexpr auto CORRECTION_INTERVAL = 500ms;
        if (m_correction_timer >= CORRECTION_INTERVAL) {
            m_correction_timer.restart();

            // reached the goal too son
            if (volume_poured_so_far() >= m_total_desired_volume) {
                end_pour();
                return;
            } else if (volume_poured_so_far() == 0.f) {
                // no water!? - bad motor? bad sensor? bad pump? who knows!
                sec::raise_error(sec::Error::PourVolumeMismatch);
            }

            const auto elapsed_seconds = time_elapsed().count() / 1000.f;
            const auto duration_seconds = m_pour_duration.count() / 1000.f;
            // when we've corrected at least once already, meaning the motor acceleration curve has been, theoretically, accounted-for
            // we can stat to track down discrepancies on every correction interval by comparing it to the ideal volume we should be at
            if (time_elapsed() >= 2s) {
                const auto ideal_flow = m_total_desired_volume / duration_seconds;
                const auto expected_volume = ideal_flow * elapsed_seconds;
                const auto ratio = volume_poured_so_far() / expected_volume;
                // 50% is a pretty generous margin, could try to go lower
                constexpr auto POUR_ACCEPTABLE_MARGIN_OF_ERROR = 0.5f;
                if (std::abs(ratio - 1.f) >= POUR_ACCEPTABLE_MARGIN_OF_ERROR)
                    sec::raise_error(sec::Error::PourVolumeMismatch);
            }

            // calculate the ideal flow and fetch our best guess for it
            const auto ideal_flow = (m_total_desired_volume - volume_poured_so_far()) / (duration_seconds - elapsed_seconds);

            // fetch the digital signal and send it!
            const auto best_digital_signal = FlowController::the().hit_me_with_your_best_shot(ideal_flow);
            send_digital_signal_to_driver(best_digital_signal);
        } else {
            // BRK is realeased after a little bit to not stress the motor too hard
            if (m_end_pour_timer >= 2s and digitalRead(Pin::BRK) == LOW)
                digitalWrite(Pin::BRK, HIGH); // fly high 🕊️
        }
    }
}

void Spout::pour_with_desired_volume(millis_t duration, float desired_volume) {
    if (duration == 0 or desired_volume == 0)
        return;

    begin_pour(duration);

    const auto flow = desired_volume / (duration / 1000);
    const auto digital_signal = FlowController::the().hit_me_with_your_best_shot(flow);
    send_digital_signal_to_driver(digital_signal);

    m_total_desired_volume = desired_volume;
    m_correction_timer.start();

    FlowController::the().update_flow_hint_for_pulse_calculation(flow);

    if (digital_signal != FlowController::INVALID_DIGITAL_SIGNAL)
        LOG_IF(LogPour, "despejo iniciado - [volume = ", desired_volume, " | sinal = ", m_digital_signal, "]");
}

float Spout::pour_with_desired_volume_and_wait(millis_t duration, float desired_volume) {
    pour_with_desired_volume(duration, desired_volume);
    util::idle_while([] { return Spout::the().pouring(); }, core::current_filters() & ~core::Filter::Spout);
    return FlowController::the().pulses_to_volume(m_pulses_at_end_of_pour - m_pulses_at_start_of_pour);
}

float Spout::pour_with_digital_signal_and_wait(millis_t duration, DigitalSignal digital_signal) {
    pour_with_digital_signal(duration, digital_signal);
    util::idle_while([] { return Spout::the().pouring(); }, core::current_filters() & ~core::Filter::Spout);
    return FlowController::the().pulses_to_volume(m_pulses_at_end_of_pour - m_pulses_at_start_of_pour);
}

void Spout::pour_with_digital_signal(millis_t duration, DigitalSignal digital_signal) {
    if (duration == 0 or digital_signal == 0)
        return;

    begin_pour(duration);
    send_digital_signal_to_driver(digital_signal);
    if (digital_signal != FlowController::INVALID_DIGITAL_SIGNAL)
        LOG_IF(LogPour, "despejo iniciado - [sinal = ", m_digital_signal, "]");
}

void Spout::setup() {
    setup_pins();
    FlowController::the().setup();
}

void Spout::setup_pins() {
    // driver setup
    analogWriteResolution(12);

    pinMode(Pin::SV, OUTPUT);
    pinMode(Pin::BRK, OUTPUT);
    pinMode(Pin::EN, OUTPUT);
    // enable is permanently on, the driver is controlled using only SV and BRK
    analogWrite(Pin::SV, LOW);
    digitalWrite(Pin::BRK, HIGH);
    digitalWrite(Pin::EN, LOW);

    // flow sensor
    pinMode(Pin::FlowSensor, INPUT);
    attachInterrupt(
        digitalPinToInterrupt(Pin::FlowSensor),
        +[] {
            ++s_pulse_counter;
        },
        RISING);
}

void Spout::send_digital_signal_to_driver(DigitalSignal v) {
    if (v == FlowController::INVALID_DIGITAL_SIGNAL) {
        LOG_ERR("sinal digital invalido, finalizando despejo");
        end_pour();
        return;
    }

    m_digital_signal = v;
    digitalWrite(Pin::BRK, m_digital_signal);
    analogWrite(Pin::SV, m_digital_signal);
    RecipeQueue::the().reset_inactivity();
}

void Spout::fill_hose(float desired_volume) {
    constexpr auto FALLBACK_SIGNAL = 1400; // radomly picked this
    constexpr auto TIME_TO_FILL_HOSE = 20s;

    MotionController::the().travel_to_sewer();

    LOG_IF(LogCalibration, "preenchendo a mangueira com agua - [duracao = ", u32(TIME_TO_FILL_HOSE.count()), "s]");
    if (desired_volume) {
        pour_with_desired_volume_and_wait(TIME_TO_FILL_HOSE, desired_volume);
    } else {
        pour_with_digital_signal_and_wait(TIME_TO_FILL_HOSE, FALLBACK_SIGNAL);
    }
}

f32 Spout::volume_poured_so_far() const {
    return FlowController::the().pulses_to_volume(s_pulse_counter - m_pulses_at_start_of_pour);
}

void Spout::begin_pour(millis_t duration) {
    m_pouring = true;
    m_begin_pour_timer.start();
    m_end_pour_timer.stop();
    m_pour_duration = chrono::milliseconds{ duration };
    m_pulses_at_start_of_pour = s_pulse_counter;
}

void Spout::end_pour() {
    send_digital_signal_to_driver(0);

    m_pouring = false;

    const auto duration = m_begin_pour_timer.elapsed();
    m_begin_pour_timer.stop();
    m_end_pour_timer.start();
    m_correction_timer.stop();
    m_pour_duration = {};

    m_total_desired_volume = 0.f;
    m_pulses_at_end_of_pour = s_pulse_counter;

    const auto pulses = m_pulses_at_end_of_pour - m_pulses_at_start_of_pour;
    const auto poured_volume = FlowController::the().pulses_to_volume(pulses);
    LOG_IF(LogPour, "despejo finalizado - [duracao = ", u32(duration.count()), "ms | volume = ", poured_volume, " | pulsos = ", pulses, "]");
}

void Spout::FlowController::setup() {
    m_storage_handle = storage::register_handle_for_entry("flow", sizeof(m_digital_signal_table));
}

void Spout::FlowController::analyse_and_store_flow_data() {
    constexpr auto ITERATION_STEP = 25;
    constexpr auto INITIAL_ITERATION_STEP = 200;

    // accepts normalized values between 0.f and 1.f
    const auto update_progress = [this](float progress) {
        m_analysis_progress = progress;
        inform_progress_to_host();
    };

    const auto update_status = [this](FlowAnalysisStatus status) {
        m_analysis_status = status;
        inform_progress_to_host();
    };

    clean_digital_signal_table(PurgeStorageEntry::No);
    update_status(FlowAnalysisStatus::Preparing);
    const auto beginning = millis();

    if (CFG(GigaMode)) {
        m_analysis_status = FlowAnalysisStatus::Executing;

        for (size_t i = 0; i < 100; i++) {
            update_progress(i / 100.f);
            util::idle_for(500ms);
        }

        util::idle_for(2s);
        update_status(FlowAnalysisStatus::Finalizing);

        util::idle_for(2s);
        update_status(FlowAnalysisStatus::Done);
    } else {
        // place the spout on the sewer and fill the hose so we avoid silly errors
        MotionController::the().travel_to_sewer();
        Spout::the().fill_hose();

        auto minimum_flow_info = obtain_specific_flow(FLOW_MIN, { 0.f, 0 }, INITIAL_ITERATION_STEP);
        save_flow_info_to_table(minimum_flow_info.flow, minimum_flow_info.digital_signal);

        auto info_when_finished = FlowInfo{ 0.f, 0 };
        auto mod_when_finished = 0;
        auto number_of_occupied_cells = 0;

        m_analysis_status = FlowAnalysisStatus::Executing;

        begin_iterative_pour(
            [&,
             last_average_flow = 0.f,
             fixed_bad_delta = false](FlowInfo info, s32& digital_signal_mod) mutable {
                if (m_abort_analysis)
                    return util::Iter::Break;
                if (last_average_flow and info.flow < last_average_flow) {
                    digital_signal_mod = ITERATION_STEP;
                    LOG_IF(LogCalibration, "fluxo diminuiu?! aumentando sinal digital - [ultimo = ", last_average_flow, "]");
                    return util::Iter::Continue;
                }

                // let's make sure the jump from one iteration to the other respects the maximum delta
                if (last_average_flow) {
                    // no need to 'abs' this since we chcked that 'last_average_flow' is smaller
                    const auto delta = info.flow - last_average_flow;
                    // big jump! cut the modification and subtract it from the signal
                    if (delta > 1.f and last_average_flow + delta < FLOW_MAX) {
                        if (digital_signal_mod > 0) {
                            LOG_IF(LogCalibration, "inverteu legal");
                            digital_signal_mod *= -1;
                        }

                        digital_signal_mod = std::min(digital_signal_mod / 2, -1);
                        fixed_bad_delta = true;

                        LOG_IF(LogCalibration, "pulo muito grande de força - [delta = ", delta, " | nova modificacao = ", digital_signal_mod, "]");
                        return util::Iter::Continue;
                    } else if (delta < 1.f and fixed_bad_delta) {
                        fixed_bad_delta = false;
                        digital_signal_mod *= -1;
                        LOG_IF(LogCalibration, "inverteu denovo");
                    }
                }

                if (std::abs(info.flow - FLOW_MAX) <= 0.1) { // lucky!
                    save_flow_info_to_table(info.flow, info.digital_signal);
                    number_of_occupied_cells++;
                    return util::Iter::Break;
                } else if (info.flow > FLOW_MAX) { // not so lucky
                    info_when_finished = info;
                    mod_when_finished = digital_signal_mod > 0 ? -digital_signal_mod : digital_signal_mod;
                    return util::Iter::Break;
                } else {
                    save_flow_info_to_table(info.flow, info.digital_signal);
                    update_progress(util::normalize(info.flow, FLOW_MIN, FLOW_MAX));
                    number_of_occupied_cells++;
                    last_average_flow = info.flow;
                }

                return util::Iter::Continue;
            },
            minimum_flow_info.digital_signal + ITERATION_STEP,
            ITERATION_STEP);

        // if we didn't find the maximum flow in the iteration above, try finding it now
        if (m_digital_signal_table.back().back() == INVALID_DIGITAL_SIGNAL and not m_abort_analysis) {
            update_status(FlowAnalysisStatus::Finalizing);
            auto maximum_flow_info = obtain_specific_flow(FLOW_MAX, info_when_finished, mod_when_finished);
            save_flow_info_to_table(maximum_flow_info.flow, maximum_flow_info.digital_signal);
        }

        Spout::the().end_pour();
        update_status(FlowAnalysisStatus::Done);
        if (m_abort_analysis) {
            clean_digital_signal_table(PurgeStorageEntry::No);
            return;
        }

        LOG_IF(LogCalibration, "tabela preenchida - [duracao = ", (millis() - beginning) / 60000.f, "min | celulas = ", number_of_occupied_cells, "]");
        LOG_IF(LogCalibration, "resultado: ");
        for_each_occupied_cell([](DigitalSignal digital_signal, usize i, usize j) {
            LOG_IF(LogCalibration, "[", i, "][", j, "] = ", digital_signal, " = ", i + FLOW_MIN, ".", j, "g/s");
            return util::Iter::Continue;
        });

        save_digital_signal_table_to_file();
    }
}

Spout::FlowController::FlowInfo Spout::FlowController::obtain_specific_flow(float desired_flow, FlowInfo starting_flow_info, s32 starting_mod) {
    FlowInfo result = { 0.f, 0 };
    begin_iterative_pour(
        [this,
         &result,
         desired_flow,
         last_greater_than_specific = starting_flow_info.flow > desired_flow](FlowInfo info, s32& digital_signal_mod) mutable {
            if (std::abs(info.flow - desired_flow) <= 0.1f) {
                result = info;
                LOG_IF(LogCalibration, "fluxo especifico obtido - [sinal = ", info.digital_signal, "]");
                return util::Iter::Break;
            } else {
                const bool greater_than_specific = info.flow > desired_flow;
                // this little algorithm makes us gravitates towards a specific value
                // making it guaranteed that, even if it takes a little bit, we arrive
                // at the correct digital signal
                if (greater_than_specific != last_greater_than_specific) {
                    if (digital_signal_mod < 0) {
                        digital_signal_mod = std::min(digital_signal_mod / 2, -1);
                    } else {
                        digital_signal_mod = std::max(digital_signal_mod / 2, 1);
                    }
                    digital_signal_mod *= -1;
                }
                last_greater_than_specific = greater_than_specific;
                LOG_IF(LogCalibration, "fluxo especifico nao obtido - ", greater_than_specific ? "diminuindo" : "aumentando", " o sinal - [mod = ", digital_signal_mod, "]");
            }
            return util::Iter::Continue;
        },
        starting_flow_info.digital_signal,
        starting_mod);
    return result;
}

// this takes a desired flow and tries to guess the best digital signal to achieve that
Spout::DigitalSignal Spout::FlowController::hit_me_with_your_best_shot(float flow) const {
    const auto [rounded_flow, decimal] = decompose_flow(flow);
    const auto flow_index = rounded_flow - FLOW_MIN;
    const auto stored_value = m_digital_signal_table[flow_index][decimal];
    // if we already have the right value on the table just return that, no need to get fancy
    if (stored_value != INVALID_DIGITAL_SIGNAL)
        return stored_value;

    // otherwise we get the two closest values above and below the desired flow
    // then interpolate between them
    // it works surprisingly well!
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

void Spout::FlowController::update_flow_hint_for_pulse_calculation(f32 volume_hint) {
    const auto norm = util::normalize(
        // functions like `obtain_specific_flow` may generate pours that fall out of this range
        // and since this funciton gets called on every analysis iteratin we have to account for those cases
        std::clamp(volume_hint, f32(FLOW_MIN), f32(FLOW_MAX)),
        FLOW_MIN,
        FLOW_MAX);

    constexpr auto MIN_ML_PER_PULSE = 0.5475f;
    constexpr auto MAX_ML_PER_PULSE = 0.5f;

    m_pulse_weight = std::lerp(MIN_ML_PER_PULSE, MAX_ML_PER_PULSE, norm);
    LOG_IF(LogCalibration, "atualizando peso do pulso - [volume = ", volume_hint, " - peso = ", m_pulse_weight, "]");
}

f32 Spout::FlowController::pulses_to_volume(u32 pulses) const {
    return pulses * m_pulse_weight;
}

void Spout::FlowController::clean_digital_signal_table(PurgeStorageEntry purge) {
    for (auto& t : m_digital_signal_table)
        std::fill(t.begin(), t.end(), INVALID_DIGITAL_SIGNAL);

    if (purge == PurgeStorageEntry::Yes)
        storage::purge_entry(m_storage_handle);

    m_analysis_progress = 0.f;
    m_analysis_status = FlowAnalysisStatus::None;
}

void Spout::FlowController::save_digital_signal_table_to_file() {
    auto entry = storage::fetch_or_create_entry(m_storage_handle);
    entry.write_binary(m_digital_signal_table);

    m_target_temperature_for_last_analysis = Boiler::the().target_temperature();
}

void Spout::FlowController::fetch_digital_signal_table_from_file() {
    if (auto entry = storage::fetch_entry(m_storage_handle))
        entry->read_binary_into(m_digital_signal_table);
}

void Spout::FlowController::inform_progress_to_host() const {
    if (m_analysis_status == FlowAnalysisStatus::None)
        return;

    if (m_analysis_status == FlowAnalysisStatus::Executing) {
        info::send(
            info::Event::Calibration,
            [this](JsonObject o) {
                o["progress"] = m_analysis_progress;
            });
    } else {
        info::send(
            info::Event::Calibration,
            [this](JsonObject o) {
                o["status"] = usize(m_analysis_status);
            });
    }
}

Spout::FlowController::FlowInfo Spout::FlowController::first_flow_info_below_flow(s32 flow_index, s32 decimal) const {
    for (; flow_index >= 0; --flow_index, decimal = 9) {
        const auto& cells = m_digital_signal_table[flow_index];
        for (; decimal >= 0; --decimal) {
            if (cells[decimal] != INVALID_DIGITAL_SIGNAL) {
                const auto flow = float(flow_index + FLOW_MIN) + float(decimal) / 10.f;
                return { flow, cells[decimal] };
            }
        }
    }

    return { 0.f, INVALID_DIGITAL_SIGNAL };
}

Spout::FlowController::FlowInfo Spout::FlowController::first_flow_info_above_flow(s32 flow_index, s32 decimal) const {
    for (; flow_index < FLOW_RANGE; ++flow_index, decimal = 0) {
        const auto& cells = m_digital_signal_table[flow_index];
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
    return { s32(clamped), s32(clamped * 10) % 10 };
}
}
