#include <string.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "conectado.h"

#include "esp_sleep.h"
#include "mdf_common.h"
#include "mwifi.h"
#include "wifi.h"
#include "mw.h"

static const char *TAG = "MAIN";

/**
 * @brief Realiza a leitura daas mensagens recebidas pelo no filho
 * Toma uma acao dependendo de qual mensagem recebeu
 * 
 * Tipo 0: Mensagem da leitura do sensor. Extrai e informa o que 
 * recebeu
 * 
 * Tipo 1: Mensagem do tempo atualizado. Extrai, atualiza o relogio interno 
 * e mostra o relogio interno atualizado.
 * 
 * @param arg Nenhum argumento e passado
 */
static void node_read_task(void *arg)
{
  /*Inicializa as variaveis utilizadas*/
  mdf_err_t ret = MDF_OK;
  char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
  size_t size   = MWIFI_PAYLOAD_LEN;
  mwifi_data_type_t data_type      = {0x0};
  uint8_t src_addr[MWIFI_ADDR_LEN] = {0x0};
  float temp, umi = 0;
  struct tm timeinfo;
  struct timeval tv;
  char strftime_buf[64];
  time_t now;

  MDF_LOGI("No filho esta pronto para ler");

  //Espera o HIBERNAR_BIT. HINBERNA_BIT
  xEventGroupWaitBits(
    IM_event_group,               
    HIBERNAR_BIT,                 
    pdFALSE,                       
    pdFALSE,                     
    portMAX_DELAY                    
  );

  while(1) {
    /*Tamanho maximo do dado recebido*/
    size = MWIFI_PAYLOAD_LEN;
    /*Limpa o vetor de dado*/
    memset(data, 0, MWIFI_PAYLOAD_LEN);        
    /*Leitura dos dados recebidos pelo no filho*/                                                         
    ret = mwifi_read(src_addr, &data_type, data, &size, 1000 / portTICK_RATE_MS);                       
    /*Verifica se o dado foi recebido com sucesso*/    
    if(ret == MDF_OK) {
      /*Se ocorreu bem, toma uma acao dependendo de qual tipo de mensagem*/                                                                                 
      switch (data_type.custom)
        {
        /*Tipo 0 e para os dados temperatura e umidade*/
        case 0:
          /*Extrai dados de temperatura e umidadde recebidas*/
          sscanf(data, " Temperatura: %f C e Umidade: %f%%", &temp, &umi);
          /*Mostra os dados que recebeu*/
          MDF_LOGI("Temperatura: %0.2f e Umidade %0.2f%%",temp, umi);
          break;
        /*Tipo 1 e para os dados de horario*/
        case 1:
          /*Extrai as informacoes do tempo atualizado*/
          sscanf(data, "s: %ld s", &tv.tv_sec);
          /*Atualiza o relogio local*/
          settimeofday(&tv, NULL);
          /*Arruma o fuso horario para o de brasilia*/
          setenv("TZ", "BRT3BRST,M10.3.0/0,M2.3.0/0", 1);
          tzset();
          /*Para mostrar o relogio interno*/
          time(&now);
          localtime_r(&now, &timeinfo);
          strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
          ESP_LOGI(TAG, "O relogio do esp esta assim: %s", strftime_buf);
          break;
        }        
    }
    /*Se a leitura ocorreu errado, indica o erro*/    
    else if(ret != MDF_ERR_TIMEOUT) {                                                                 
      MDF_LOGE("mwifi_read, ret: %x", ret);                                                           
    }
    /*Aguarda 1000ms para realizar a proxima leitura*/
    vTaskDelay(10000 / portTICK_RATE_MS); 
  }
}


/**
 * @brief Solicita o tempo atualizado para o no raiz caso necessario. Espera 5 min e manda o 
 * dispositivo hibernar ate a proxima hora cheia.
 * 
 * @param arg Nenhum argumento e passado
 */
static void hiberna_task(void *arg)
{
  /*Inicializa as variaveis utilizadas*/
  
  time_t now;
  struct tm timeinfo;

  /*Aguarda o MWIFI estar configurado*/
  xEventGroupWaitBits(
    IM_event_group,                       
    MW_DISPONIVEL_BIT,                    
    pdFALSE,                                  
    pdFALSE,                                
    portMAX_DELAY                         
  );
  
  while(1) {

    /*Armazena a hora local do momento no dispositivo na estrutura timeinfo*/
    time(&now);                                                                                         
    localtime_r(&now, &timeinfo);                                                                       
    
    /*Verifica se o relogio interno do dispositivo esta atualizado*/
    if(timeinfo.tm_year < (2023 - 1900)){                                                               
      /*Se nao estiver, solicita para atualizar:*/
      /*Variaveis para envio de mensagem*/
      char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
      mwifi_data_type_t data_type      = {0x0};
      /*altera o tipo de mensagem para 1*/
      data_type.custom = 1;
      size_t size   = MWIFI_PAYLOAD_LEN;
      /*Armazena a mensagem no vetor data*/
      size = sprintf(data, "Me atualiza");
      /*Permite a operacao da tarefa node_read_task*/
      xEventGroupSetBits(IM_event_group, HIBERNAR_BIT); 
      /*Envio da mensagem solicitacao tempo atualizado*/                                                                   
      mwifi_write(NULL, &data_type, data, size, true);
      /*Aguarda 2000ms (Necessario caso nao atualize seu relogio na primeira solicitacao)*/
      vTaskDelay(2000 / portTICK_RATE_MS);
    }
    /*Se ja tiver atualizado:, seta o bit, espera 5 min e manda o disp acordar na prox hora cheia*/
    else{
      /*Permite a operacao da proxima tarefa*/        
      xEventGroupSetBits(IM_event_group, HIBERNAR_BIT);  
      /*Aguarda 5 minutos*/
      vTaskDelay(300000 / portTICK_RATE_MS);
      /*Armazena a hora local do momento no dispositivo na estrutura timeinfo*/
      time(&now);                                                                                         
      localtime_r(&now, &timeinfo); 
      /*Calcula a proxima hora cheia*/
      int total_p_prox_hora = ((60 - timeinfo.tm_min) * 60)+ (60 - timeinfo.tm_sec );
      /*Mostra quanto tempo vai dormir*/
      ESP_LOGI(TAG, "Vai acordar em: %d s", total_p_prox_hora);
      //Desliga wifi
      esp_wifi_stop();
      /*Hiberna o dispositivo ate a proxima hora cheia calculada*/
      esp_deep_sleep(1000000LL * total_p_prox_hora);
    } 
  }
}

/**
 * @brief Inicializa WiFi, MWiFi e cria as tarefas node_read_task e hiberna_task
 * 
 */
void app_main(void)
{      
  /* Inicializa NVS */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  /* Para o ESP32 entrar em modo de economia de energia */
  #if CONFIG_PM_ENABLE
    /*
    * Configure a escala de frequência dinâmica:
    * Normalmente, "max_freq_mhz" é igual a freq. padrão da CPU. Enquanto que
    * "min_freq_mhz" é um múltiplo inteiro da freq. XTAL. Contudo, 10 MHz é a
    * frequência mais baixa em que o clock padrão REF_TICK de 1 MHz pode ser gerado.
    */
    esp_pm_config_esp32_t pm_config = 
    {
      .max_freq_mhz = 80,
      .min_freq_mhz = 10,
      #if CONFIG_FREERTOS_USE_TICKLESS_IDLE
      /* O (light sleep) automático é ativado se a opção "tickless idle" estiver habilitada. */
      .light_sleep_enable = true
      #endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
  #endif // CONFIG_PM_ENABLE
  
  /* Cria o evento IM_event_group para utilizar wifi e mqtt */
  criarEvento();

  /* Limpa os bits que sao utilizados que indicam se internet e o mqtt estao disponiveis, assim garantem que sao 0
  antes de serem utlizados */
  xEventGroupClearBits(IM_event_group, INTERNET_DISPONIVEL_BIT);
  xEventGroupClearBits(IM_event_group, MW_DISPONIVEL_BIT);
  xEventGroupClearBits(IM_event_group, HIBERNAR_BIT);

  /*Inicia o wifi no modo sta*/
  wifi_start();
  /*Inicia a rede mesh e configura o dispositivo como no filho*/
  mw_start();

  /*Cria as tarefas*/
  xTaskCreate(node_read_task, "node_read_task", 3 * 1024,NULL, 5, NULL);
  xTaskCreate(hiberna_task, "hiberna_task", 3 * 1024,NULL, 4, NULL);

  /* Para evitar watch dog*/
  while(1)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
}
