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
            /* Caso wifi ligado */
            case WIFI_EVENT_STA_START:
                break;

            /* Caso o wifi desconectado*/
            case WIFI_EVENT_STA_DISCONNECTED: 
                break;   
        }
    }
}

/**
 * @brief Inicialia e configura o wifi no modo estacao (sta)
 * 
 */
void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());                              //Inicializa a interface de rede 
    ESP_ERROR_CHECK(esp_event_loop_create_default());               //Cria a o loop de eventos
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();            //Cria uma estrutura com a config padrao do wifi
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));                           //Inicializa o driver wifi com a config feita acima
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));      //Configura o armazenamento da info de conexao do wifi para a memoria flash
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );             //Configura o wifi p o modo sta
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));                 //Configura o WiFi para o modo de economia de energia desativado
    ESP_ERROR_CHECK(esp_wifi_start() );                             //Inicia o wifi
    ESP_LOGI(TAG, "wifi_init_sta finished.");
    xEventGroupSetBits(IM_event_group, INTERNET_DISPONIVEL_BIT);

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
