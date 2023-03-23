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
	* +--------------------------+--------------------------+----------------------------------------------------+ */  
	Config = 0x1,
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
	Client = 0x2
};

enum class OperacaoConectar : byte {
	Conectar = 0x1,
	Desconcetar = 0x2,
	Esquecer = 0x3
};


template<typename It>
concept Iterator = requires(It it) {
	{ it.begin() };
	{ it.end() };
};

template<Iterator It>
static void enviar_protocolo(Mensagem tipo, It it) {
	/*
	* - Comunicação com o módulo WiFi da MakerBase -
	* 
	* Todos os protocolos enviados/recebidos do módulo WiFi tem a seguinte estrutura:
	* 
	* +----------+-----------------+--------------------------------------------------------------------+
	* | Segmento | Tamanho (bytes) | Descrição                                                          |
	* +----------+-----------------+--------------------------------------------------------------------+
	* | Início   | 1 (uint8_t)     | Início do protocolo - sempre 0xA5                                  |
	* | Tipo     | 1 (uint8_t)     | Tipo da mensagem - veja o enum 'Tipo'                              |
	* | Tamanho  | 2 (uint16_t)    | Tamanho da mensagem - máximo de 1024 - 5 (bytes reservados) = 1019 |
	* | Mensagem | Tamanho         | Mensagem em sí                                                     |
	* | Fim      | 1 (uint8_t)     | Fim do protocolo - sempre 0xFC                                     |
	* +----------+-----------------+--------------------------------------------------------------------+
	* 
	* Cada mensagem possui uma estrutura interna, documentadas no enum 'Tipo'
	*/
	constexpr byte INICIO = 0xA5;
	constexpr byte FIM = 0xFC;
	constexpr size_t BYTES_RESERVADOS = 5;

	uint16_t tamanho_msg = std::distance(it.begin(), it.end());

	std::vector<byte> buffer;
	buffer.reserve(BYTES_RESERVADOS + tamanho_msg);
	// Início
	buffer.push_back(INICIO);
	// Tipo
	buffer.push_back(static_cast<byte>(tipo));
	// Tamanho (16 bytes dividídos em 2)
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
	std::array<byte, sizeof...(Bytes)> msg = { static_cast<byte>(bytes)... };
	enviar(tipo, msg);
}

void conectar(std::string_view nome_rede, std::string_view senha_rede) {
	// u8 + u8 + nome_rede + u8 + senha_rede
	auto tamanho_msg = 1 + 1 + nome_rede.size() + 1 + senha_rede.size();

	std::vector<byte> msg;
	msg.reserve(tamanho_msg);
	// Modo
	msg.push_back(static_cast<byte>(ModoConexao::AP));
	// Tamanho do nome da rede
	msg.push_back(nome_rede.size());
	// Nome da rede
	msg.insert(msg.end(), nome_rede.begin(), nome_rede.end());
	// Tamanho da senha da rede
	msg.push_back(senha_rede.size());
	// Senha da rede
	msg.insert(msg.end(), senha_rede.begin(), senha_rede.end());

	enviar_protocolo(Mensagem::Config, std::move(msg));
	enviar_protocolo(Mensagem::Conectar, OperacaoConectar::Conectar);

	DBG("conectando wifi...");
}

bool conectado() {
    return strstr(ipPara.ip_addr, "0.0.0.0") == nullptr;
}

}