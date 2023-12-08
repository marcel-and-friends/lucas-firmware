#pragma once

#include <lucas/util/util.h>
#include <lucas/core/core.h>
#include <lucas/storage/storage.h>
#include <lucas/lucas.h>
#include <lucas/util/Timer.h>
#include <lucas/util/Singleton.h>

namespace lucas {
class Station;
class Spout : public util::Singleton<Spout> {
public:
    enum Pin {
        SV = PA5,
        EN = PA6,
        BRK = PE8,
        FlowSensor = Z_MIN_PIN
    };

    void tick();

    using DigitalSignal = u32;

    void pour_with_desired_volume(millis_t duration, float desired_volume);
    void pour_with_desired_volume(const auto duration, float desired_volume) {
        const millis_t ms = chrono::duration_cast<chrono::milliseconds>(duration).count();
        pour_with_desired_volume(ms, desired_volume);
    }

    float pour_with_desired_volume_and_wait(millis_t duration, float desired_volume);
    float pour_with_desired_volume_and_wait(const auto duration, float desired_volume) {
        const millis_t ms = chrono::duration_cast<chrono::milliseconds>(duration).count();
        return pour_with_desired_volume_and_wait(ms, desired_volume);
    }

    void pour_with_digital_signal(millis_t duration, DigitalSignal digital_signal);
    void pour_with_digital_signal(const auto duration, DigitalSignal digital_signal) {
        const millis_t ms = chrono::duration_cast<chrono::milliseconds>(duration).count();
        pour_with_digital_signal(ms, digital_signal);
    }

    float pour_with_digital_signal_and_wait(millis_t duration, DigitalSignal digital_signal);
    float pour_with_digital_signal_and_wait(const auto duration, DigitalSignal digital_signal) {
        const millis_t ms = chrono::duration_cast<chrono::milliseconds>(duration).count();
        return pour_with_digital_signal_and_wait(ms, digital_signal);
    }

    void end_pour();

    void setup();

    void setup_pins();

    bool pouring() const { return m_pouring; }

    static volatile inline u32 s_pulse_counter = 0;

    class FlowController : public util::Singleton<FlowController> {
    public:
        static constexpr auto INVALID_DIGITAL_SIGNAL = 0xF0F0; // UwU

        void setup();

        void analyse_and_store_flow_data();

        void clean_digital_signal_table();
        void save_digital_signal_table_to_file();
        void fetch_digital_signal_table_from_file();

        static void inform_flow_analysis_status() {
            core::inform_calibration_status();
            the().inform_progress_to_host();
        }

        void inform_progress_to_host() const;

        void set_abort_analysis(bool b) { m_abort_analysis = b; }
        bool abort_analysis() const { return m_abort_analysis; }

        std::optional<s32> last_analysis_target_temperature() const;

        DigitalSignal hit_me_with_your_best_shot(float flow) const;

        void update_flow_hint_for_pulse_calculation(f32 volume_hint);

        f32 pulses_to_volume(u32 pulses) const;

        static inline auto MIN_ML_PER_PULSE = 0.54794520547f;
        static inline auto MAX_ML_PER_PULSE = 0.49497293116f;

        static constexpr auto FLOW_MIN = 4;
        static constexpr auto FLOW_MAX = 16;
        static constexpr auto FLOW_RANGE = FLOW_MAX - FLOW_MIN;
        static constexpr auto NUMBER_OF_CELLS = FLOW_RANGE * 10;
        // TODO: turn this into a single-dimensional array
        using Table = std::array<std::array<DigitalSignal, 10>, FLOW_RANGE>;

        static_assert(sizeof(Table) == sizeof(DigitalSignal) * NUMBER_OF_CELLS, "unexpected table size");

    private:
        friend class util::Singleton<FlowController>;

        FlowController() {
            clean_digital_signal_table();
        }

        struct FlowInfo {
            float flow;
            DigitalSignal digital_signal;
        };

        void begin_iterative_pour(util::IterFn<FlowInfo, s32&> auto&& callback, DigitalSignal starting_signal, s32 starting_mod) {
            constexpr auto TIME_TO_ANALISE_POUR = 10s;

            // the signal sent to the driver on every iteration
            DigitalSignal digital_signal = starting_signal;
            // the amount by which that signal gets modified at the end of the current iteration
            // can be negative
            s32 digital_signal_mod = starting_mod;
            while (true) {
                if (m_abort_analysis)
                    return;

                // let's not fly too close to the sun
                if (digital_signal >= 4095)
                    break;

                const auto starting_pulses = s_pulse_counter;

                LOG_IF(LogCalibration, "comecando nova iteracao - [sinal = ", digital_signal, "]");
                Spout::the().send_digital_signal_to_driver(digital_signal);

                util::idle_for(TIME_TO_ANALISE_POUR);

                const auto pulses = s_pulse_counter - starting_pulses;
                const auto average_flow = pulses_to_volume(pulses) / TIME_TO_ANALISE_POUR.count();
                update_flow_hint_for_pulse_calculation(average_flow);

                if (digital_signal >= 2000 and pulses == 0) {
                    m_abort_analysis = true;
                    return;
                }

                LOG_IF(LogCalibration, "fluxo estabilizou - [pulsos = ", pulses, " | fluxo medio = ", average_flow, "]");

                if (std::invoke(FWD(callback), FlowInfo{ average_flow, digital_signal }, digital_signal_mod) == util::Iter::Break)
                    return;

                digital_signal += digital_signal_mod;
            }
        }

        FlowInfo obtain_specific_flow(float flow, FlowInfo starting_flow_info, s32 starting_mod);

        FlowInfo first_flow_info_below_flow(s32 flow_index, s32 decimal) const;
        FlowInfo first_flow_info_above_flow(s32 flow_index, s32 decimal) const;

        struct DecomposedFlow {
            s32 rounded_flow;
            s32 decimal;
        };

        DecomposedFlow decompose_flow(float flow) const;

        void save_flow_info_to_table(float flow, DigitalSignal signal) {
            auto [rounded_flow, decimal] = decompose_flow(flow);
            m_digital_signal_table[rounded_flow - FLOW_MIN][decimal] = signal;
            LOG_IF(LogCalibration, "fluxo info salva - [sinal = ", signal, ", index1 = ", rounded_flow - FLOW_MIN, ", index2 = ", decimal, "]");
        }

        void for_each_occupied_cell(util::IterFn<DigitalSignal, usize, usize> auto&& callback) const {
            for (usize i = 0; i < m_digital_signal_table.size(); ++i) {
                auto& cells = m_digital_signal_table[i];
                for (usize j = 0; j < cells.size(); ++j) {
                    auto digital_signal = cells[j];
                    if (digital_signal != INVALID_DIGITAL_SIGNAL)
                        if (std::invoke(FWD(callback), digital_signal, i, j) == util::Iter::Break)
                            return;
                }
            }
        }

        // this table is like a map-like matrix of the digital values that produce a specific flow between FLOW_MIN and FLOW_MAX, with 1 digit of precision
        // the values are accessed using 'whole number - FLOW_MIN' as the index to the first array and the decimal as the index to the second
        // @ref decompose_flow
        // for example, assuming FLOW_MIN == 5:
        // 		- the cell for the flow '5.5 g/s' is located at 'table[0][5]'
        // 		- the cell for the flow '10.3 g/s' is located at 'table[5][3]'
        Table m_digital_signal_table;

        enum class FlowAnalysisStatus {
            Preparing = 0,
            Finalizing,
            Done,

            Executing,
            None
        };

        FlowAnalysisStatus m_analysis_status = FlowAnalysisStatus::None;

        float m_analysis_progress = 0.f;

        bool m_abort_analysis = false;

        storage::Handle m_flow_analysis_storage_handle;

        storage::Handle m_target_temperature_on_last_analysis_handle;

        f32 m_pulse_weight = 0.f;
    };

private:
    void send_digital_signal_to_driver(DigitalSignal);

    void begin_pour(millis_t duration);

    void fill_hose(float desired_volume = 0.f);

    f32 volume_poured_so_far() const;

    u32 m_pulses_at_start_of_pour = 0;
    u32 m_pulses_at_end_of_pour = 0;

    chrono::milliseconds m_pour_duration;

    float m_total_desired_volume = 0.f;

    util::Timer m_begin_pour_timer;
    util::Timer m_end_pour_timer;
    util::Timer m_correction_timer;

    DigitalSignal m_digital_signal = 0;

    bool m_pouring = false;
};
}
