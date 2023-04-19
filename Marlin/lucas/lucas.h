#pragma once

#include <lucas/wifi/wifi.h>
#include <lucas/gcode/gcode.h>
#include <lucas/Bico.h>
#include <lucas/opcoes.h>
#include <lucas/tick.h>

namespace lucas {
// Gcode executado ao apertar os botões
// (lembrando que as coordenadas são relativas!)
static constexpr auto RECEITA_PADRAO =
    R"(G3 F5000 I20 J20
G3 F5000 I15 J15
G3 F5000 I10 J10
G3 F5000 I5 J5
L0
G3 F5000 I5 J5
G3 F5000 I10 J10
G3 F5000 I15 J15
G3 F5000 I20 J20)";

// Gcodes necessário para o funcionamento ideal da máquina, executando quando a máquina liga, logo após conectar ao WiFi
static constexpr auto ROTINA_INICIAL =
    R"(G28 X Y
M190 R93)";

#define DBG SERIAL_ECHOLNPGM
#define UPDATE(tipo, valor) DBG("$", tipo, ":", valor);

inline bool g_trocando_estacao_ativa = false;
inline bool g_conectando_wifi = false;
inline bool g_executando_rotina_inicial = false;

void setup();

void pos_execucao_gcode();

}
