#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi.h"
#include "conectado.h"

//#define EXAMPLE_ESP_WIFI_SSID      "WIFI GIOVANI"
//#define EXAMPLE_ESP_WIFI_PASS      "GiovaniSenha"
#define EXAMPLE_ESP_WIFI_SSID      "Multilaser_6D8920"
#define EXAMPLE_ESP_WIFI_PASS      "noseafter605"
static const char *TAG = "wifi station";


/**
 * @brief Informa a mudança de estados a partir de eventos
 * 
 * @param arg --
 * @param event_base O ID base do evento para registrar o handler
 * @param event_id O ID do evento para registrar o handler
 * @param event_data O dado, especifico da ocorrencia do evento, que é passado para o handler
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    /* Evento de WIFI */
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            /* Caso wifi ligado, conecta o WIFI */
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;

            /* Caso o wifi desconectado, limpa o bit INTERNET_DISPONIVEL_BIT e tenta reconectar*/
            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupClearBits(IM_event_group, INTERNET_DISPONIVEL_BIT);
                esp_wifi_connect();  
        }
    }
    /* Evento IP - seta o bit INTERNET_DISPONIVEL_BIT */ 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(IM_event_group, INTERNET_DISPONIVEL_BIT);
    }    
}


void wifi_start(void)
{

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    /* Configura o wifi */
    wifi_config_t wifi_config =
    {
        .sta = 
        {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = 
            {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG, "wifi_init_sta finished.");


    /* Para colocar o wifi em modo de economia */
    #if CONFIG_PM_ENABLE
        /*
        * Coloca o WiFi em modo de economia de energia. (Essa opção é obrigatória quando o gerenciamento de energia é
        * habilitado. Caso contrário, a CPU sempre estará no clock máximo.)
        * As opções são WIFI_PS_NONE (sem economia de energia), WIFI_PS_MIN_MODEM (economia mínima), WIFI_PS_MAX_MODEM
        * (economia máxima). O modo WIFI_PS_MAX_MODEM não traz uma economia muito maior e depende do parâmetro
        * "listen_interval" da struct "wifi_sta_config_t".
        */
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    #endif // CONFIG_PM_ENABLE
}
