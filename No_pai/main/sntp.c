#include "sntp.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_event.h"
#include "esp_system.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "conectado.h"
#include "esp_sntp.h"

static const char *TAG = "SNTP";

static void obtain_time(void);
static void initialize_sntp(void);

/**
 * @brief Quando o tempo é sincronizado com sucesso atraves do SNTP, essa funcao e chamada e imprime uma mensagem informativa
 * 
 * @param tv Ponteiro para uma estrutura timeval, que contem informacoes sobre a data e hora sincronizadas
 */
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notificacao do evento de sincronizacao de tempo");
}

void sntp_configurar(void)
{
    /*Inicializa as variaveis utilizadas*/
    struct timeval tv;
    time_t now;
    struct tm timeinfo;

    /*Extrai a o tempo do relogio interno em segundos*/
    time(&now);
    /*Filtra o tempo em segundos e o divide em ano, mes, dia... na estrutura timeinfo*/
    localtime_r(&now, &timeinfo);

    /*Espera o mwifi ser cconfigurado*/ 
    xEventGroupWaitBits(
       /*EventGroup q vai esperar*/
       IM_event_group,
       /*Bit q esta esperando a mudanca*/               
       MWIFI_DISPONIVEL_BIT,
       /*O bit n vai ser limpo dps de lido*/        
       pdFALSE,
       /*logica "or" para os bits que esta esperando, entretanto so esta esperando um bit*/                            
       pdFALSE,
       /*tempo para esperar o bit ser ativado (ate ocorrer nesse caso)*/                      
       portMAX_DELAY                    
    );

    /*Verifica se o relogio esta atualizado, se n: */ 
    if (timeinfo.tm_year < (2023 - 1900)) {
        /*Atualiza relogio interno*/
        ESP_LOGI(TAG, "O relogio ainda nao esta sincronizado. Atualizando relogio.");
        obtain_time();
        // update 'now' variable with current time
        //time(&now);
    }
  
    /*Configurando o relogio interno para o fuso horario de Brasília */ 
    setenv("TZ", "BRT3BRST,M10.3.0/0,M2.3.0/0", 1);
    tzset();
    
    //gettimeofday(&tv, NULL);
    /*Coloca o bit TEMPO_ATT_BIT em alto, permitindo o que esta esperando esse bit*/
    xEventGroupSetBits(IM_event_group, TEMPO_ATT_BIT);

}

/**
 * @brief Configura obter a hora atual a partir do servidor NTP (Network Time Protocol).
 * 
 */
static void obtain_time(void)
{
    /*Configura o SNTP*/
    initialize_sntp();

    /*Para esperar o relogio atualizar*/
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    /*verifica se o status de sincronização do SNTP e SNTP_SYNC_STATUS_RESET ate um limite de tentativas*/
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Esperando a sincronizacao do tempo... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    //time(&now);
    //localtime_r(&now, &timeinfo);
    
}

/**
 * @brief Configurar o SNTP
 * 
 */
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP");
    /*Define o modo de operação do SNTP. Esta operando como polling nesse caso */
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    /*Configura o servidor NTP que sera usado para sincronizar o tempo do dispositivo com o tempo mundial*/
    sntp_setservername(0, "pool.ntp.org");
    /*Quando atualizado com sucesso chama a funcao time_sync_notification_cb()*/
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    /*Inicializa o SNTP com as configracoes feitas*/
    sntp_init();
}
