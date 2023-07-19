#pragma once

#include <lucas/info/Report.h>
#include <lucas/util/util.h>
#include <span>

namespace lucas::info {
constexpr size_t BUFFER_SIZE = 1024;
using DocumentoJson = StaticJsonDocument<BUFFER_SIZE>;

void setup();

void tick();

void interpretar_json(std::span<char> buffer);

void print_json(const DocumentoJson& doc);

void evento(const char* nome, util::Fn<void, JsonObject> auto&& callback) {
    DocumentoJson evento_doc;
    std::invoke(FWD(callback), evento_doc.createNestedObject(nome));
    print_json(evento_doc);
}
}
