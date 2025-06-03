#include "pico/stdlib.h"            // Biblioteca da Raspberry Pi Pico para funções padrão (GPIO, temporização, etc.)
#include "pico/cyw43_arch.h"        // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "pico/unique_id.h"         // Biblioteca com recursos para trabalhar com os pinos GPIO do Raspberry Pi Pico

#include "hardware/gpio.h"          // Biblioteca de hardware de GPIO
#include "hardware/irq.h"           // Biblioteca de hardware de interrupções
#include "hardware/adc.h"           // Biblioteca de hardware para conversão ADC

#include "lwip/apps/mqtt.h"         // Biblioteca LWIP MQTT -  fornece funções e recursos para conexão MQTT
#include "lwip/apps/mqtt_priv.h"    // Biblioteca que fornece funções e recursos para Geração de Conexões
#include "lwip/dns.h"               // Biblioteca que fornece funções e recursos suporte DNS:
#include "lwip/altcp_tls.h"         // Biblioteca que fornece funções e recursos para conexões seguras usando TLS:

#include "credenciais_mqtt.h" // Altere o arquivo de exemplo dentro do lib para suas credenciais e retire example do nome
#include "perifericos.h"

float temp_max = 37.0; // Temperatura máxima em graus Celsius
float temp_min = 35.0; // Temperatura mínima em graus Celsius
int bpm_max = 100; // Batimento cardíaco máximo
int bpm_min = 60;  // Batimento cardíaco mínimo
bool alarme_medico = false; // Variável para controlar o alarme médico
bool alarme_manual = false; // Variável para controlar o alarme manual


// Definição da escala de temperatura
#ifndef TEMPERATURE_UNITS
#define TEMPERATURE_UNITS 'C' // Set to 'F' for Fahrenheit
#endif

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

//Dados do cliente MQTT
typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

// Cria registro com os dados do cliente
static MQTT_CLIENT_DATA_T state;

#ifndef DEBUG_printf
#ifndef NDEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf(...)
#endif
#endif

#ifndef INFO_printf
#define INFO_printf printf
#endif

#ifndef ERROR_printf
#define ERROR_printf printf
#endif

// Temporização da coleta de saúde - how often to measure our saúde
#define HEALTH_WORKER_TIME_S 5

// Manter o programa ativo - keep alive in seconds
#define MQTT_KEEP_ALIVE_S 60

// QoS - mqtt_subscribe
// At most once (QoS 0)
// At least once (QoS 1)
// Exactly once (QoS 2)
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

// Tópico usado para: last will and testament
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "pico"
#endif

// Definir como 1 para adicionar o nome do cliente aos tópicos, para suportar vários dispositivos que utilizam o mesmo servidor
#ifndef MQTT_UNIQUE_TOPIC
#define MQTT_UNIQUE_TOPIC 0
#endif

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */


// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err);

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);

// Controle do LED
static void control_led(bool on);

// Publicar temperatura
static void publish_health(MQTT_CLIENT_DATA_T *state);

// Verifica se há uma condição de alarme
static bool verifica_condicao_alarme(float temperatura, int batimento);

// Gerencia o alarme médico e manual
static bool gerenciar_alarme(float temperatura, int batimento);

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err);

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err);

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub);

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

// Publicar saúde
static void health_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t health_worker = { .do_work = health_worker_fn };

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state);

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);

// Interrupção do botão A
static void alarme_manual_handler(uint gpio, uint32_t events);


int main(void) {

    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();
    INFO_printf("mqtt client starting\n");

    // Inicializa o conversor ADC
    adc_init();

    // Inicializa a arquitetura do cyw43
    if (cyw43_arch_init()) {
        panic("Failed to inizialize CYW43");
    }

    // Usa identificador único da placa
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for(int i=0; i < sizeof(unique_id_buf) - 1; i++) {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Gera nome único, Ex: pico1234
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    INFO_printf("Device name %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // TLS enabled
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    // This confirms the indentity of the server and the client
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
            client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Warning: tls without verification is insecure\n");
#endif
#else
    state->client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Warning: tls without a certificate is insecure\n");
#endif
#endif

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        panic("Failed to connect");
    }
    INFO_printf("\nConnected to Wifi\n");

    //Faz um pedido de DNS para o endereço IP do servidor MQTT
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    // Se tiver o endereço, inicia o cliente
    if (err == ERR_OK) {
        start_client(&state);
    } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
        panic("dns request failed");
    }

    // Inicializa o botão A como botão de alarme manual, atraves de interrupção
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A); // Configura o botão A com pull-up interno
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &alarme_manual_handler);


    // Configura o led vermelho e o verde
    gpio_init(LED_PIN_RED);
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);
    gpio_init(LED_PIN_GREEN);
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);

    // Loop condicionado a conexão mqtt
    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst)) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));
    }

    INFO_printf("mqtt client exiting\n");
    return 0;
}

// Interrupção do botão A
static void alarme_manual_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_A) {
        static uint32_t last_press_time = 0;
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - last_press_time > 200) { // Debounce em 200ms
            alarme_manual = !alarme_manual; // Alterna o estado do alarme manual
            INFO_printf("Alarme manual %s\n", alarme_manual ? "ativado" : "desativado");
            
            // Publica tópico imediatamente
            const char *alarme_key = full_topic(&state, "/alarme");
            const char *alarme_msg = alarme_manual ? "1" : "0";
            if (alarme_manual) {
                control_led(true); // Liga o LED se o alarme manual estiver ativado
                iniciar_buzzer(BUZZER_A); // Inicia o buzzer
                INFO_printf("Publishing alarm status %s to %s\n", alarme_msg, alarme_key);
                mqtt_publish(state.mqtt_client_inst, alarme_key, alarme_msg, strlen(alarme_msg), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, &state);
            } else if (!alarme_medico) {
                control_led(false); // Desliga o LED se o alarme manual estiver desativado
                parar_buzzer(BUZZER_A); // Para o buzzer
                INFO_printf("Publishing alarm status %s to %s\n", alarme_msg, alarme_key);
                mqtt_publish(state.mqtt_client_inst, alarme_key, alarme_msg, strlen(alarme_msg), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, &state);
            }
        }
        last_press_time = current_time;
    }
}

// Leitura de temperatura do sensor
static float read_temperatura() {
    adc_select_input(1); // SSeleciona o sensor de temperatura
    uint16_t vrx_value = adc_read(); // Lê o valor do eixo x (Temperatura)
    return ((vrx_value - 16) / max_value_joy) * 20 + 26; // Converte o valor do eixo x para a faixa de 26 a 46
}

// Leitura de batimento cardíaco do sensor
static int read_batimento() {
    adc_select_input(0); // Seleciona o sensor de batimento
    uint16_t vry_value = adc_read(); // Lê o valor do eixo y (Batimento)
    return ((vry_value - 16) / max_value_joy) * 80 + 40; // Ajusta o valor do eixo y para a faixa de 40 a 120
}

// Verifica se há uma condição de alarme
static bool verifica_condicao_alarme(float temperatura, int batimento) {
    // Exemplo de condição de alarme: temperatura acima de 37.5 ou batimento fora da faixa de 60 a 100
    return (temperatura > temp_max || temperatura < temp_min ||
            batimento < bpm_min || batimento > bpm_max);
}

// Gerencia o alarme médico e manual
static bool gerenciar_alarme(float temperatura, int batimento){
    
    if (verifica_condicao_alarme(temperatura, batimento)) {
        alarme_medico = true; // Ativa o alarme médico
        INFO_printf("Alarme médico ativado!\n");
        iniciar_buzzer(BUZZER_A); // Inicia o buzzer
        control_led(true); // Liga o LED vermelho
        return true;
    } else{
        alarme_medico = false; // Desativa o alarme médico
        if (alarme_manual){
            INFO_printf("Alarme manual ativado!\n");
            iniciar_buzzer(BUZZER_A); // Inicia o buzzer
            control_led(true); // Liga o LED vermelho
            return true;
        } else{
            INFO_printf("Condições normalizadas.\n");
            parar_buzzer(BUZZER_A); // Para o buzzer
            control_led(false); // Liga o LED verde
            return false; // Desativa o alarme
        }
    }
}

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != 0) {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

//Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

// Controle do LED
static void control_led(bool on) {
    if (on){
        gpio_put(LED_PIN_RED, 1); // Liga o LED vermelho
        gpio_put(LED_PIN_GREEN, 0); // Desliga o LED verde
        INFO_printf("LED vermelho ligado\n");
    } else {
        gpio_put(LED_PIN_RED, 0); // Desliga o LED vermelho
        gpio_put(LED_PIN_GREEN, 1); // Liga o LED verde
        INFO_printf("LED verde ligado\n");
    }

    //mqtt_publish(state->mqtt_client_inst, full_topic(state, "/alarm/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// Publicar saúde
static void publish_health(MQTT_CLIENT_DATA_T *state) {

    const char *temperatura_key = full_topic(state, "/temperatura");
    float temperatura = read_temperatura();

    // Publish temperatura on /temperatura topic
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temperatura);
    INFO_printf("Publishing %s to %s\n", temp_str, temperatura_key);
    mqtt_publish(state->mqtt_client_inst, temperatura_key, temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);


    static int old_batimento;
    const char *batimento_key = full_topic(state, "/batimento");
    int batimento = read_batimento();
    if (batimento != old_batimento) {
        old_batimento = batimento;
        // Publish batimento on /batimento topic
        char bat_str[16];
        snprintf(bat_str, sizeof(bat_str), "%.2d", batimento);
        INFO_printf("Publishing %s to %s\n", bat_str, batimento_key);
        mqtt_publish(state->mqtt_client_inst, batimento_key, bat_str, strlen(bat_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }


    bool alarme_atual = gerenciar_alarme(temperatura, batimento);
    const char *alarme_key = full_topic(state, "/alarme");
    const char *alarme_msg = alarme_atual ? "1" : "0";
    
    INFO_printf("Publishing alarm status %s to %s\n", alarme_msg, alarme_key);
    mqtt_publish(state->mqtt_client_inst, alarme_key, alarme_msg, strlen(alarme_msg), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client) {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub) {
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/comando/temperatura"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/comando/batimento"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/led"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);
    if (strcmp(basic_topic, "/comando/batimento") == 0) {
        char *data_str = (char *)state->data;
        char *separator = strchr(data_str, ',');
        if (separator){
            *separator = '\0'; // Separa a string em dois, pela vírgula
            int novo_batimento_min = atoi(data_str);
            int novo_batimento_max = atoi(separator + 1);
            if (novo_batimento_min < 0 || novo_batimento_max < 0 || novo_batimento_min >= novo_batimento_max) {
                ERROR_printf("Faixa de batimento inválida: %.2d, %.2d\n", novo_batimento_min, novo_batimento_max);
            } else {
                bpm_min = (int)novo_batimento_min;
                bpm_max = (int)novo_batimento_max;
                INFO_printf("Faixa de batimento atualizada: %d - %d\n", bpm_min, bpm_max);
            }
        } else {
            ERROR_printf("Formato inválido para batimento: %s\n", state->data);
        }
    } else if (strcmp(basic_topic, "/comando/temperatura") == 0) {
        char *data_str = (char *)state->data;
        char *separator = strchr(data_str, ',');
        if (separator) {
            *separator = '\0'; // Separa a string em dois, pela vírgula
            float novo_temp_min = atof(data_str);
            float novo_temp_max = atof(separator + 1);
            if (novo_temp_min < 0 || novo_temp_max < 0 || novo_temp_min >= novo_temp_max) {
                ERROR_printf("Faixa de temperatura inválida: %.2f, %.2f\n", novo_temp_min, novo_temp_max);
            } else {
                temp_min = novo_temp_min;
                temp_max = novo_temp_max;
                INFO_printf("Faixa de temperatura atualizada: %.2f - %.2f\n", temp_min, temp_max);
            }
        } else {
            ERROR_printf("Formato inválido para temperatura: %s\n", state->data);
        }

    } else if (strcmp(basic_topic, "/print") == 0) {
        INFO_printf("%.*s\n", len, data);
    } else if (strcmp(basic_topic, "/ping") == 0) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    } else if (strcmp(basic_topic, "/exit") == 0) {
        state->stop_client = true; // stop the client when ALL subscriptions are stopped
        sub_unsub_topics(state, false); // unsubscribe
    }
}

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

// Publicar saúde
static void health_worker_fn(async_context_t *context, async_at_time_worker_t *worker) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    publish_health(state);
    async_context_add_at_time_worker_in_ms(context, worker, HEALTH_WORKER_TIME_S * 1000);
}

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        // Publish health data every 10 sec if it's changed
        health_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &health_worker, 0);
    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
            panic("Failed to connect to mqtt server");
        }
    }
    else {
        panic("Unexpected status");
    }
}

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    } else {
        panic("dns request failed");
    }
}