#pragma once

#include <array>
#include <lucas/types.h>

namespace lucas::cfg {
struct Option {
    static constexpr char ID_DEFAULT = 0x47;

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

    GigaMode,

    SetTargetTemperatureOnCalibration,
    FillDigitalSignalTableOnCalibration,

    Count
};

using OptionList = std::array<Option, usize(Options::Count)>;

constexpr auto FIRMWARE_VERSION = "0.0.1";

void setup();

void save_options();

void reset_options();

Option get(Options option);
OptionList& opcoes();

#define CFG(option) cfg::get(cfg::Options::option).active
#define LOG_IF(option, ...)                                          \
    do {                                                             \
        if (CFG(option)) {                                           \
            const auto& option_cfg = cfg::get(cfg::Options::option); \
            if (option_cfg.id != cfg::Option::ID_DEFAULT)            \
                SERIAL_ECHO(AS_CHAR(option_cfg.id));                 \
            else                                                     \
                SERIAL_ECHO(AS_CHAR('?'));                           \
            LOG("", ": ", __VA_ARGS__);                              \
        }                                                            \
    } while (false)
}
