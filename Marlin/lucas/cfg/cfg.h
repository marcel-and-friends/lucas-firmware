#pragma once

#include <array>
#include <lucas/types.h>

namespace lucas::cfg {
struct [[gnu::packed]] Option {
    static constexpr char ID_DEFAULT = 0x2B;

    char id = ID_DEFAULT;
    bool active = false;
};

enum Options {
    LogPour = 0,
    LogTravel,
    LogQueue,
    LogCalibration,
    LogStations,

    LogSerial,
    LogWifi,
    LogGcode,
    LogLn,

    LogFlowSensorDataForTesting,
    LogTemperatureForTesting,

    GigaMode,
    MaintenanceMode,

    SetTargetTemperatureOnCalibration,
    ForceFlowAnalysis,

    Count
};

using OptionList = std::array<Option, usize(Options::Count)>;

constexpr auto FIRMWARE_VERSION = "1.0.40";

void setup();

void save_options();

Option& get(Options option);

OptionList& options();

#define CFG(option) ::lucas::cfg::get(::lucas::cfg::Options::option).active
#define LOG_IF(option, ...)                                                            \
    do {                                                                               \
        if (CFG(option)) {                                                             \
            const auto& option_cfg = ::lucas::cfg::get(::lucas::cfg::Options::option); \
            if (option_cfg.id != ::lucas::cfg::Option::ID_DEFAULT)                     \
                SERIAL_CHAR(option_cfg.id);                                            \
            else                                                                       \
                SERIAL_CHAR('?');                                                      \
            SERIAL_ECHOLNPGM("", ": ", __VA_ARGS__);                                   \
        }                                                                              \
    } while (false)
}
