#pragma once

#include <lucas/info/Report.h>
#include <lucas/util/util.h>
#include <span>

namespace lucas::info {
constexpr size_t BUFFER_SIZE = 1024;
using JsonDocument = StaticJsonDocument<BUFFER_SIZE>;

void setup();

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
    Firmware
};

void send(Event type, util::Fn<void, JsonObject> auto&& callback) {
    constexpr static auto EVENT_NAMES = std::to_array({
        [size_t(Event::Boiler)] = "infoBoiler",
        [size_t(Event::Recipe)] = "infoRecipe",
        [size_t(Event::Station)] = "infoStation",
        [size_t(Event::AllStations)] = "infoAllStations",
        [size_t(Event::Security)] = "infoSecurity",
        [size_t(Event::Calibration)] = "infoCalibration",
        [size_t(Event::Firmware)] = "infoFirmware",
    });

    JsonDocument doc;
    std::invoke(FWD(callback), doc.createNestedObject(EVENT_NAMES[size_t(type)]));
    print_json(doc);
}
}
