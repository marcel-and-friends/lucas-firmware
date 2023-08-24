#pragma once

#include <src/MarlinCore.h>
#include <lucas/util/util.h>

namespace lucas {
class Station;
class Spout {
public:
    static Spout& the() {
        static Spout instance;
        return instance;
    }

    void tick();

    enum class CorrectFlow {
        Yes,
        No
    };

    using DigitalSignal = uint32_t;

    void pour_with_desired_volume(millis_t duration, float volume_desejado, CorrectFlow corrigir);

    float pour_with_desired_volume_and_wait(millis_t duration, float volume_desejado, CorrectFlow corrigir);

    void pour_with_digital_signal(millis_t duration, DigitalSignal digital_signal);

    void end_pour();

    void setup();

    void travel_to_station(const Station&, float offset = 0.f) const;

    void travel_to_station(size_t, float offset = 0.f) const;

    void finish_movements() const;

    void travel_to_sewer() const;

    void home() const;

    bool pouring() const { return m_pouring; }

    class FlowController {
    public:
        static constexpr auto INVALID_DIGITAL_SIGNAL = 0xF0F0; // UwU
        static inline auto ML_PER_PULSE = 0.5375f;

        static FlowController& the() {
            static FlowController instance;
            return instance;
        }

        void fill_digital_signal_table();

        enum class SaveToFlash {
            Yes,
            No
        };

        void clean_digital_signal_table(SaveToFlash salvar);

        void try_fetching_digital_signal_table_from_flash();

        DigitalSignal hit_me_with_your_best_shot(float flow) const;

        static constexpr auto FLOW_MIN = 5;
        static constexpr auto FLOW_MAX = 15;
        static constexpr auto FLOW_RANGE = FLOW_MAX - FLOW_MIN;
        static constexpr auto NUMBER_OF_CELLS = FLOW_RANGE * 10;
        static_assert(FLOW_MAX > FLOW_MIN, "FLOW_MAX deve ser maior que FLOW_MIN");

        using Table = std::array<std::array<DigitalSignal, 10>, FLOW_RANGE>;
        static_assert(sizeof(Table) == sizeof(DigitalSignal) * NUMBER_OF_CELLS, "unexpected table size");

    private:
        FlowController() {
            clean_digital_signal_table(SaveToFlash::No);
        }

        void save_digital_signal_table_to_flash();
        void fetch_digital_signal_table_from_flash();

        struct FlowInfo {
            float flow;
            DigitalSignal digital_signal;
        };

        FlowInfo first_flow_info_below_flow(int flow_index, int decimal) const;
        FlowInfo first_flow_info_above_flow(int flow_index, int decimal) const;

        struct DecomposedFlow {
            int rounded_flow;
            int decimal;
        };

        DecomposedFlow decompose_flow(float flow) const;

        void for_each_occupied_cell(util::IterFn<DigitalSignal, size_t, size_t> auto&& callback) const {
            for (size_t i = 0; i < m_digital_signal_table.size(); ++i) {
                auto& cells = m_digital_signal_table[i];
                for (size_t j = 0; j < cells.size(); ++j) {
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
    };

private:
    void send_digital_signal_to_driver(DigitalSignal);

    void begin_pour(millis_t duration);

    void fill_hose(float desired_volume = 0.f);

    static volatile inline uint32_t s_pulse_counter = 0;
    uint32_t m_pulses_at_start_of_pour = 0;
    uint32_t m_pulses_at_end_of_pour = 0;

    millis_t m_pour_duration = 0;

    float m_total_desired_volume = 0.f;

    CorrectFlow m_flow_correction = CorrectFlow::No;

    millis_t m_time_of_last_correction = 0;

    millis_t m_time_elapsed = 0;

    millis_t m_tick_pour_began = 0;
    millis_t m_tick_pour_ended = 0;

    DigitalSignal m_digital_signal = 0;

    bool m_pouring = false;
};
}
