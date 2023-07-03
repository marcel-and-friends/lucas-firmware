#pragma once

#include <lucas/info/Report.h>
#include <lucas/info/evento.h>
#include <span>

namespace lucas::info {
constexpr size_t BUFFER_SIZE = 1024;
using DocumentoJson = StaticJsonDocument<BUFFER_SIZE>;

void tick();

void interpretar_json(std::span<char> buffer);

void print_json(const DocumentoJson& doc);

template<typename T, util::FixedString Str>
void evento(const Evento<T, Str>& evento) {
    DocumentoJson evento_doc;
    evento.gerar_json(evento_doc.createNestedObject(evento.nome()));
    print_json(evento_doc);
}
}
