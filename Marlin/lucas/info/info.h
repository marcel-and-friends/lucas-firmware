#pragma once

#include <lucas/info/Report.h>
#include <lucas/util/util.h>
#include <span>

namespace lucas::info {
constexpr size_t BUFFER_SIZE = 1024;
using JsonDocument = StaticJsonDocument<BUFFER_SIZE>;

void setup();

void tick();

void interpret_json(std::span<char> buffer);

void print_json(JsonDocument const& doc);

void event(char const* name, util::Fn<void, JsonObject> auto&& callback) {
    JsonDocument doc;
    std::invoke(FWD(callback), doc.createNestedObject(name));
    print_json(doc);
}
}
