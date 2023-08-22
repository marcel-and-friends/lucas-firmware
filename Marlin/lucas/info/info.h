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

void print_json(const JsonDocument& doc);

void event(const char* name, util::Fn<void, JsonObject> auto&& callback) {
    JsonDocument doc;
    std::invoke(FWD(callback), doc.createNestedObject(name));
    print_json(doc);
}
}
