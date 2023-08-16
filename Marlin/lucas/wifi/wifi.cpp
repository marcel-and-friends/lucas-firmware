#include "wifi.h"
#include <lucas/lucas.h>
#include <cstdint>
#include <vector>
#ifdef MKS_WIFI_MODULE
    #include <src/lcd/extui/mks_ui/draw_ui.h>
#endif

namespace lucas::wifi {

enum class Protocol : byte {
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
    Connect = 0x09
};

enum class ModoConexao : byte {
    AP = 0x1,
    Client = 0x2
};

enum class Operacao : byte {
    Connect = 0x1,
    Disconnect = 0x2,
    Forget = 0x3
};

template<typename It>
concept Iterator = requires(It it) {
    { it.begin() };
    { it.end() };
};

template<Iterator It>
static void send_protocol(Protocol tipo, It&& it) {
    /*
     * - Comunicação com o módulo WiFi da MakerBase -
     *
     * Todos os protocolos enviados/recebidos do módulo WiFi tem a seguinte estrutura:
     *
     * +----------+-----------------+--------------------------------------------------------------------+
     * | Segmento | Tamanho (bytes) | Descrição                                                          |
     * +----------+-----------------+--------------------------------------------------------------------+
     * | Início   | 1 (uint8_t)     | Início do protocolo - sempre 0xA5                                  |
     * | Tipo     | 1 (uint8_t)     | Tipo da mensagem - veja o enum 'Protocol'                          |
     * | Tamanho  | 2 (uint16_t)    | Tamanho da mensagem - máximo de 1024 - 5 (bytes reservados) = 1019 |
     * | Protocol | Tamanho         | Protocol em sí                                                     |
     * | Fim      | 1 (uint8_t)     | Fim do protocolo - sempre 0xFC                                     |
     * +----------+-----------------+--------------------------------------------------------------------+
     *
     * Cada mensagem possui uma estrutura interna, documentadas no enum 'Protocol'
     */
    constexpr byte BEGINNING = 0xA5;
    constexpr byte ENDING = 0xFC;
    constexpr auto RESERVED_BYTES = 5; // u8 + u8 + u16 + u8

    uint16_t message_size = std::distance(it.begin(), it.end());

    std::vector<byte> buffer;
    buffer.reserve(RESERVED_BYTES + message_size);
    // Início
    buffer.push_back(BEGINNING);
    // Tipo
    buffer.push_back(static_cast<byte>(tipo));
    // Tamanho (16 bytes divididos em 2)
    buffer.push_back(message_size & 0xFF);
    buffer.push_back((message_size >> 8) & 0xFF);
    // Protocol
    buffer.insert(buffer.end(), it.begin(), it.end());
    // Fim
    buffer.push_back(ENDING);

#ifdef MKS_WIFI_MODULE
    raw_send_to_wifi(buffer.data(), buffer.size());
#endif
}

template<typename... Bytes>
static void send_protocol(Protocol tipo, Bytes... bytes) {
    std::array msg = { static_cast<byte>(bytes)... };
    send_protocol(tipo, msg);
}

static bool g_conectando = false;

void connect(std::string_view network_name, std::string_view network_password) {
    // u8 + u8 + network_name + u8 + network_password
    auto message_size = 1 + 1 + network_name.size() + 1 + network_password.size();

    std::vector<byte> config_msg;
    config_msg.reserve(message_size);
    // Modo
    config_msg.push_back(static_cast<byte>(ModoConexao::Client));
    // Tamanho do nome da rede
    config_msg.push_back(network_name.size());
    // Nome da rede
    config_msg.insert(config_msg.end(), network_name.begin(), network_name.end());
    // Tamanho da senha da rede
    config_msg.push_back(network_password.size());
    // Senha da rede
    config_msg.insert(config_msg.end(), network_password.begin(), network_password.end());

    send_protocol(Protocol::Config, config_msg);
    send_protocol(Protocol::Connect, Operacao::Connect);

    LOG_IF(LogWifi, "connecting wifi na rede '", network_name.data(), " - ", network_password.data(), "'");
}

void informar_sobre_rede() {
    g_conectando = false;
    LOG_IF(LogWifi, "conectado ");
    LOG_IF(LogWifi, "-- informacoes da rede --");
    LOG_IF(LogWifi, "ip = ", wifi::ip().data(), " \nnome = ", wifi::network_name().data(), " \nsenha = ", wifi::network_password().data());
    LOG_IF(LogWifi, "-------------------------");
}

std::string_view ip() {
#ifdef MKS_WIFI_MODULE
    return ipPara.ip_addr;
#else
    return "";
#endif
}

std::string_view network_name() {
#ifdef MKS_WIFI_MODULE
    return wifiPara.ap_name;
#else
    return "";
#endif
}

std::string_view network_password() {
#ifdef MKS_WIFI_MODULE
    return wifiPara.keyCode;
#else
    return "";
#endif
}
}
