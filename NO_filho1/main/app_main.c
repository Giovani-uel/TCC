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
#include "wifi.h"
#include "esp_sleep.h"
#include "conectado.h"
#include "dht.h"
#include "mdf_common.h"
#include "mwifi.h"

#include "mwcomwifi.h"

#define SENSOR_TYPE DHT_TYPE_AM2301
const int DHT_DATA_GPIO = 17;
float temperatura, umidade;

static const char *TAG = "MAIN";

TaskHandle_t task_ler_handle;

/**
 * @brief Solicita dados do tempo atualizado para o no raiz caso necessario. Apaga a tarefa de ler apos verificar se ainda e requisitada. 
 * Depois de 5 min do dispositivo operando, faz o dispositivo entrar em hibernacao
 * 
 * @param arg Nenhum argumento e passado
 */
static void hiberna_task(void *arg)
{
  /*Inicializa as variaveis utilizadas*/
  time_t now;
  struct tm timeinfo;
  
  //Espera o mwifi ser conectado ao no pai
  xEventGroupWaitBits(
    IM_event_group,               //EventGroup q vai esperar
    MWIFI_DISPONIVEL_BIT ,     //Bits q está esperando a mudanca
    pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
    pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
    portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
  );
    
  
  while(1) {
    /*Armazena a hora local atual do dispositivo na estrutura timeinfo*/
    time(&now);
    localtime_r(&now, &timeinfo);
    /*Verifica se a hora local esta atualizada*/
    if(timeinfo.tm_year < (2023 - 1900)){
      /*Se nao estiver*/
      /*Inicializa variaveis necessarias para enviar dados*/
      char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
      mwifi_data_type_t data_type      = {0x0};
      /*O tipo de dado para solicatao de tempo e o com custom = 2*/
      data_type.custom = 2;
      size_t size   = MWIFI_PAYLOAD_LEN;
      mdf_err_t ret = MDF_OK;
      /*Formata a string "Me atualiza" e armazena no vetor data. size armazena o numero de caracteres*/
      size = sprintf(data, "Me atualiza");
      /*Verica se o mwifi wsta disponivel*/
      if(mwifi_is_connected()){
        /*Se estiver, manda data para root node*/
        ret = mwifi_write(NULL, &data_type, data, size, true);
      }
      else{
        /*Se nao estiver, indica erro*/
        MDF_LOGI("MWIFI ainda nao esta conectado"); 
      }
      /*Aguarda 2000ms*/
      vTaskDelay(2000 / portTICK_RATE_MS);
    }
    else{
      /*Se o relogio local, ja estiver atualizado*/
      /*Deleta tarefa de ler (node_read_task)*/
      vTaskDelete(task_ler_handle); 
      /*Espera 5 minutos (300000ms)*/
      vTaskDelay(300000 / portTICK_RATE_MS);
      /*Armazena a hora local do momento no dispositivo na estrutura timeinfo*/
      time(&now);                                                                                         
      localtime_r(&now, &timeinfo); 
      /*Calcula a proxima hora cheia. Exemplo: 13:00, 12:00*/
      int total_p_prox_hora = ((60 - timeinfo.tm_min) * 60)+ (60 - timeinfo.tm_sec );
      /*Mostra quanto tempo vai dormir*/
      ESP_LOGI(TAG, "Vai acordar em: %d s", total_p_prox_hora);
      /*Desliga wifi*/
      esp_wifi_stop();
      /*Hiberna o dispositivo ate a proxima hora cheia calculada*/
      esp_deep_sleep(1000000LL * total_p_prox_hora);
    }   
  }
}

/*
      int acorda_tempo = (4 - (timeinfo.tm_min%5))*60 + (60 - timeinfo.tm_sec);
      ESP_LOGI(TAG, "Vai acordar em: %d s", acorda_tempo);
      esp_deep_sleep(1000000LL * acorda_tempo);*/

/**
 * @brief Recebe dados de horario pelo root node. Atualiza relogio interno do microcontrolador.
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
    struct tm timeinfo;
    struct timeval tv;
    char strftime_buf[64];
    time_t now;

    /*Armazena a hora local atual do dispositivo na estrutura timeinfo*/
    time(&now);
    localtime_r(&now, &timeinfo);

    MDF_LOGI("No filho esta pronto para ler");

    //Espera o mwifi ser conectado ao no pai
    xEventGroupWaitBits(
      IM_event_group,               //EventGroup q vai esperar
      MWIFI_DISPONIVEL_BIT ,     //Bits q está esperando a mudanca
      pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
      pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
      portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
    );
    

  while(1) {

    /*Tamanho maximo da mensagem*/
    size = MWIFI_PAYLOAD_LEN;                                                                           
    /*Preenche o vetor dado com 0 para garantir que nao tenha nada*/
    memset(data, 0, MWIFI_PAYLOAD_LEN);                                                                 
    /*Realiza a leitura do dado recebido pelo root node. "ret" armazena se o dado foi recebido ou nao*/
    ret = mwifi_read(src_addr, &data_type, data, &size, 1000 / portTICK_RATE_MS);                       
    /*Se recebeu o dado com sucesso*/
    if(ret == MDF_OK) {                                                                                 
      /*Verifica se o dispositivo atual ja esta com relogio sincronizado*/
      if (timeinfo.tm_year < (2023 - 1900)) {
        /*Se nao estiver, atualiza o relogio interno*/
        /*Extrai os dados de tempo recebido para a estrutura tv*/
        sscanf(data, "s: %ld s", &tv.tv_sec);
        /*Define o relógio do sistema a partir de um valor de tempo especificado*/
        settimeofday(&tv, NULL);
        /*Define o fuso horario local, utilizando a string "BRT3BRST,M10.3.0/0,M2.3.0/0". Colocando assim o fuso Horario de Brasilia*/
        setenv("TZ", "BRT3BRST,M10.3.0/0,M2.3.0/0", 1);
        /*Le o valor da variavel de ambiente TZ (Time Zone) e define a zona de tempo local do sistema com base nesse valor*/
        tzset();
        /*Converte o valor de tempo em segundos de tv para uma estrura em um objeto de estrutura tm*/
        //localtime_r(&tv.tv_sec, &timeinfo);
        /*Armazena o novo tempo atual do dispositivo */
        time(&now);
        /*Converte o tempo atual em uma estrutura que separa o tempo em ano, mes e dia*/  
        localtime_r(&now, &timeinfo);
        /*Formata o tempo armazenado na struct timeinfo em uma string legivel e a armazena no buffer strftime_buf*/  
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        /*Mostra o relogio local do dispositivo*/  
        ESP_LOGI(TAG, "O relogio do esp esta assim: %s", strftime_buf); 
      }  
    }
    /*Se o dado nao foi recebido com sucesso*/    
    else if(ret != MDF_ERR_TIMEOUT) {
      /*Indica o erro*/
      MDF_LOGE("mwifi_read, ret: %x", ret);                                                          
    }
    

  /*A tarefa repete-se a cada 1000ms*/  
  vTaskDelay(1000 / portTICK_RATE_MS); 
  }
}

/**
 * @brief Lê o sensor e envia os dados para o no pai. Essa Task que repete-se a cada 10000 ms. 
 * 
 * @param pvParameters Nao e passado nenhum paramentro
 */

void dht_test(void *pvParameters)
{
  //Espera o mwifi ser conectado ao no pai
  xEventGroupWaitBits(
    IM_event_group,               //EventGroup q vai esperar
    MWIFI_DISPONIVEL_BIT ,     //Bits q está esperando a mudanca
    pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
    pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
    portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
  );

  while(1)
  {
    /*se a leitura for bem sucedida*/
    if (dht_read_float_data(SENSOR_TYPE, DHT_DATA_GPIO, &umidade, &temperatura) == ESP_OK)
    { 
      /*Envia os dados para o no raiz*/
      mandar_no(temperatura, umidade);
    }
    /*Caso não seja*/
    else
    {
      printf("Nao e possivel ler os dados do sensor\n");
    }
    /*A tarefa repete-se a cada 10000ms*/
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

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

  /* Limpa os bits que são utilizados, assim garantem que são 0
  antes de serem utlizados */
  xEventGroupClearBits(IM_event_group, INTERNET_DISPONIVEL_BIT);
   xEventGroupClearBits(IM_event_group, MWIFI_DISPONIVEL_BIT);
  
  /*Inicia o wifi no modo sta*/
  wifi_start();
  /*Inicia a rede mesh e configura o dispositivo como no filho*/
  mw_start();
  
  /* Cria a task dht_test */
  xTaskCreate(dht_test, "dht_test", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
  xTaskCreate(node_read_task, "node_read_task", 4 * 1024,NULL, 5, &task_ler_handle);
  xTaskCreate(hiberna_task, "hiberna_task", 3 * 1024,NULL, CONFIG_MDF_TASK_DEFAULT_PRIOTY, NULL);      
  /* Para evitar watch dog*/
  while(1)
  {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  
}

