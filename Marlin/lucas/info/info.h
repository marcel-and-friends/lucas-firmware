#pragma once

#include <lucas/info/Report.h>
#include <lucas/serial/FirmwareUpdateHook.h>
#include <lucas/util/util.h>
#include <span>

namespace lucas::info {
constexpr usize BUFFER_SIZE = 2048;
using JsonDocument = StaticJsonDocument<BUFFER_SIZE>;

void tick();

void interpret_command_from_host(std::span<char> buffer);

void print_json(const JsonDocument& doc);

// https://www.notion.so/Eventos-informa-es-enviadas-da-m-quina-para-o-app-93dca4c7c1984aa38ffd5bddbe2c22a2?pvs=4
enum class Event {
    Boiler = 0,
    Recipe,
    Station,
    AllStations,
    Security,
    Calibration,
    Firmware,
    Other,
};

void send(Event type, util::Fn<void, JsonObject> auto&& callback) {
    constexpr static auto EVENT_NAMES = std::to_array({
        [usize(Event::Boiler)] = "infoBoiler",
        [usize(Event::Recipe)] = "infoRecipe",
        [usize(Event::Station)] = "infoStation",
        [usize(Event::AllStations)] = "infoAllStations",
        [usize(Event::Security)] = "infoSecurity",
        [usize(Event::Calibration)] = "infoCalibration",
        [usize(Event::Firmware)] = "infoFirmware",
        [usize(Event::Other)] = "infoOther",
    });

    JsonDocument doc;
    std::invoke(FWD(callback), doc.createNestedObject(EVENT_NAMES[usize(type)]));
    print_json(doc);
}
}
