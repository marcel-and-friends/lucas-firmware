#include "wifi.h"
#include "kofy.h"
#include <cstdint>
#include <vector>
#include <src/lcd/extui/mks_ui/draw_ui.h>

namespace kofy::marlin::wifi {

enum class Mensagem : byte {
	/*
	* +--------------------------+--------------------------+----------------------------------------------------+
	* | Segmento                 | Tamanho (bytes)          | Significado                                        |
	* +--------------------------+--------------------------+----------------------------------------------------+
	* | Modo                     | 1 (uint8_t)              | Modo da conexão - veja o enum 'ModoConexao'        |
	* | Tamanho do nome da rede  | 1 (uint8_t)              | Tamanho do nome da rede - máximo de 32 caracteres  |
	* | Nome da rede             | Tamanho do nome da rede  | Nome da rede                                       |
	* | Tamanho da senha da rede | 1 (uint8_t)              | Tamanho da senha da rede - máximo de 64 caracteres |
	* | Senha da rede            | Tamanho da senha da rede | Senha da rede                                      |
	* +--------------------------+--------------------------+----------------------------------------------------+
	*/
	Config = 0x0,
	/*
	* +----------+-----------------+-----------------------------------+
	* | Segmento | Tamanho (bytes) | Significado                       |
	* +----------+-----------------+-----------------------------------+
	* | Operação | 1 (uint8_t)     | 0x1: Conectar à rede configurada  |
    * |          |                 | 0x2: Desconectar da rede atual    |
    * |          |                 | 0x3: Esquecer senha da rede atual |
	* +----------+-----------------+-----------------------------------+
	*/
	Conectar = 0x09
};

enum class ModoConexao : byte {
	AP = 0x1,
	Cliente = 0x2
};

enum class Operacao : byte {
	Conectar = 0x1,
	Desconectar = 0x2,
	Esquecer = 0x3
};


template<typename It>
concept Iterator = requires(It it) {
	{ it.begin() };
	{ it.end() };
};

template<Iterator It>
static void enviar_protocolo(Mensagem tipo, It&& it) {
	/*
	* - Comunicação com o módulo WiFi da MakerBase -
	*
	* Todos os protocolos enviados/recebidos do módulo WiFi tem a seguinte estrutura:
	*
	* +----------+-----------------+--------------------------------------------------------------------+
	* | Segmento | Tamanho (bytes) | Descrição                                                          |
	* +----------+-----------------+--------------------------------------------------------------------+
	* | Início   | 1 (uint8_t)     | Início do protocolo - sempre 0xA5                                  |
	* | Tipo     | 1 (uint8_t)     | Tipo da mensagem - veja o enum 'Mensagem'                          |
	* | Tamanho  | 2 (uint16_t)    | Tamanho da mensagem - máximo de 1024 - 5 (bytes reservados) = 1019 |
	* | Mensagem | Tamanho         | Mensagem em sí                                                     |
	* | Fim      | 1 (uint8_t)     | Fim do protocolo - sempre 0xFC                                     |
	* +----------+-----------------+--------------------------------------------------------------------+
	*
	* Cada mensagem possui uma estrutura interna, documentadas no enum 'Mensagem'
	*/
	constexpr byte INICIO = 0xA5;
	constexpr byte FIM = 0xFC;
	constexpr auto BYTES_RESERVADOS = 5; // u8 + u8 + u16 + u8

	uint16_t tamanho_msg = std::distance(it.begin(), it.end());

	std::vector<byte> buffer;
	buffer.reserve(BYTES_RESERVADOS + tamanho_msg);
	// Início
	buffer.push_back(INICIO);
	// Tipo
	buffer.push_back(static_cast<byte>(tipo));
	// Tamanho (16 bytes divididos em 2)
	buffer.push_back(tamanho_msg & 0xFF);
	buffer.push_back((tamanho_msg >> 8) & 0xFF);
	// Mensagem
	buffer.insert(buffer.end(), it.begin(), it.end());
	// Fim
	buffer.push_back(FIM);

	raw_send_to_wifi(buffer.data(), buffer.size());
}

template<typename... Bytes>
static void enviar_protocolo(Mensagem tipo, Bytes... bytes) {
	std::array msg = { static_cast<byte>(bytes)... };
	enviar_protocolo(tipo, msg);
}

void conectar(std::string_view nome_rede, std::string_view senha_rede) {
	// u8 + u8 + nome_rede + u8 + senha_rede
	auto tamanho_msg = 1 + 1 + nome_rede.size() + 1 + senha_rede.size();

	std::vector<byte> config_msg;
	config_msg.reserve(tamanho_msg);
	// Modo
	config_msg.push_back(static_cast<byte>(ModoConexao::Cliente));
	// Tamanho do nome da rede
	config_msg.push_back(nome_rede.size());
	// Nome da rede
	config_msg.insert(config_msg.end(), nome_rede.begin(), nome_rede.end());
	// Tamanho da senha da rede
	config_msg.push_back(senha_rede.size());
	// Senha da rede
	config_msg.insert(config_msg.end(), senha_rede.begin(), senha_rede.end());

	enviar_protocolo(Mensagem::Config, config_msg);
	enviar_protocolo(Mensagem::Conectar, Operacao::Conectar);

	DBG("conectando wifi...");
}

bool conectado() {
    return wifi_link_state == WIFI_CONNECTED;
}

std::string_view ip() {
	return ipPara.ip_addr;
}

std::string_view nome_rede() {
	return wifiPara.ap_name;
}

std::string_view senha_rede() {
	return wifiPara.keyCode;
}

}
