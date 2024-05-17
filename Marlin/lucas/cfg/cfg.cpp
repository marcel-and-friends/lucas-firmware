#include "cfg.h"
#include <lucas/storage/storage.h>
#include <lucas/lucas.h>

namespace lucas::cfg {
namespace detail {
// clang-format off
constexpr auto DEFAULT_OPTIONS = std::to_array<Option>({
    [LogPour] = { .id = 'D', .active = true },
    [LogTravel] = { .id = 'V', .active = true },
    [LogQueue] = { .id = 'F', .active = true },
    [LogCalibration] = { .id = 'N', .active = true },
    [LogStations] = { .id = 'E', .active = true },

    [LogSerial] = { .id = 'S', .active = true},
    [LogWifi] = { .id = 'W', .active = false},
    [LogGcode] = { .id = 'G', .active = false},
    [LogLn] = { .id = 'L', .active = false},

    [LogFlowSensorDataForTesting] = { .id = Option::ID_DEFAULT, .active = false},
    [LogTemperatureForTesting] = { .id = Option::ID_DEFAULT, .active = false},

    [GigaMode] = { .id = 'M', .active = false},
    [MaintenanceMode] = { .id = 'K', .active = true},

    [SetTargetTemperatureOnCalibration] = { .id = 'T', .active = true },
    [ForceFlowAnalysis] = { .id = 'X', .active = false },
});
// clang-format on

consteval bool doesnt_have_duplicated_ids(const OptionList& opcoes) {
    for (usize i = 1; i < opcoes.size(); ++i)
        for (usize j = 0; j < i; ++j)
            if (opcoes[i].id != Option::ID_DEFAULT and opcoes[j].id != Option::ID_DEFAULT)
                if (opcoes[i].id == opcoes[j].id)
                    return false;

    return true;
}

static_assert(DEFAULT_OPTIONS.size() == Options::Count, "weird size");
static_assert(doesnt_have_duplicated_ids(DEFAULT_OPTIONS), "no duplicated ids");
}

// gets properly initialized on setup()
static OptionList s_options = {};
static storage::Handle s_storage_handle;

void setup() {
    s_storage_handle = storage::register_handle_for_entry("cfg", sizeof(OptionList));

    auto entry = storage::fetch_entry(s_storage_handle);
    if (not entry) {
        LOG("setting up default options");
        s_options = detail::DEFAULT_OPTIONS;

        entry = storage::create_entry(s_storage_handle);
        entry->write_binary(s_options);
    } else {
        entry->read_binary_into(s_options);
    }
}

void save_options() {
    auto entry = storage::fetch_or_create_entry(s_storage_handle);
    entry.write_binary(s_options);
}

Option& get(Options option) {
    return s_options[usize(option)];
}

OptionList& options() {
    return s_options;
}
}
