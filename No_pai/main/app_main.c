#include <string.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "esp_system.h"
//#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
//#include "esp_pm.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "wifi.h"
#include "mqtt.h"
#include "conectado.h"
//#include <sys/time.h>
#include "esp_sleep.h"
#include "mw.h"
#include "mdf_common.h"
#include "mwifi.h"
#include "sntp.h"

float temp, umi= 0;
static const char *TAG = "MAIN";

int sincronizado = 2;


/**
 * @brief Espera 5 min e coloca o root para hibernar ate a proxima hora cheia.
 * 
 * @param arg Nenhum argumento e passado
 */
static void hiberna_task(void *arg)
{
  /*Inicializa as variaveis utilizadas*/
  time_t now;
  struct tm timeinfo;

  /*Aguarda 5 minutos (300000ms)*/
  vTaskDelay(300000 / portTICK_RATE_MS);
  /*Extrai o tempo atual do dispositivo*/
  time(&now);
  localtime_r(&now, &timeinfo);
  /*Calcula a proxima hora cheia*/
  int total_p_prox_hora = ((60 - timeinfo.tm_min) * 60)+ (60 - timeinfo.tm_sec );
  /*Mostra quanto tempo vai dormir*/
  ESP_LOGI(TAG, "Vai acordar em: %d s", total_p_prox_hora);
  /*Desliga wifi*/
  esp_wifi_stop();
  /*Hiberna o dispositivo ate a proxima hora cheia calculada*/
  esp_deep_sleep(1000000LL * total_p_prox_hora);
}

/**
 * @brief Envia dados (temperatura e umidade) para um nó filho de forma periódica. Seu período depende do bit ENVIARMW_DISPONIVEL_BIT
 * que é coloca em alto na task root_task() apos a leitura de dados. Envia tempo do relogio interno para filho 1 ou 2 caso necessario.
 * 
 * @param arg Nenhum argumento é passado
 */
void root_enviar_task(void *arg)
{
  /*Inicializacao das variaveis utilizadas*/
  mdf_err_t ret                    = MDF_OK;
  char *data                       = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
  size_t size                      = MWIFI_PAYLOAD_LEN;
  uint8_t dest_addr[] = {0xc0, 0x49, 0xef, 0xe4, 0x97, 0xd8};
  uint8_t dest_addr_sensor[] = {0x78, 0x21, 0x84, 0x9d, 0x1f, 0xcc};
  mwifi_data_type_t data_type      = {0};
  struct timeval tv;

  MDF_LOGI("Root enviar está esperando");

  while(1)
  {
    /*Aguarda receber os dados para enviar para enviar para o no filho*/
    xEventGroupWaitBits(
    IM_event_group,                       //EventGroup q vai esperar
    ENVIARMW_DISPONIVEL_BIT ,             //Bits q está esperando a mudanca
    pdTRUE,                               //Os bits vao ser limpos dps de lidos      
    pdFALSE,                              //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
    portMAX_DELAY                         //tempo para esperar os dois bits dps q o primeiro é ativado   
    );

    /*Preenche o vetor data com 0*/
    memset(data, 0, MWIFI_PAYLOAD_LEN);

    /*Toma uma acao dependendo do valor de sincronizado*/   
    switch (sincronizado)
    {
    case 0:
      /* sincroniza 1 */
      /*Armazena o tempo do relogio interno atualizado na estrutura tv*/
      gettimeofday(&tv, NULL);
      /*Modifica o tipo de mensagem que e enviado*/
      data_type.custom = 1;
      /*Converte o tempo em segundos em uma string*/
      size = sprintf(data, "s: %ld s", tv.tv_sec);
      /*Envia a informacao do tempo para o no filho 2*/
      ret = mwifi_root_write(dest_addr, 1, &data_type, data, size, true);
      /*Para continuar com a operacao padrao depois*/
      sincronizado = 2;
      data_type.custom = 0;
      break;
    case 1:
      /* sincroniza 2 */
      /*Armazena o tempo do relogio interno atualizado na estrutura tv*/
      gettimeofday(&tv, NULL);
      /*Modifica o tipo de mensagem que e enviado*/
      data_type.custom = 1;
      /*Converte o tempo em segundos em uma string*/
      size = sprintf(data, "s: %ld s", tv.tv_sec);
      /*Envia a informacao do tempo para o no filho 2*/
      ret = mwifi_root_write(dest_addr_sensor, 1, &data_type, data, size, true);
      /*Para continuar com a operacao padrao depois*/
      sincronizado = 2;
      data_type.custom = 0;
      break;
    case 2:
      /* Envia temp e umidade*/
      size = sprintf(data, " Temperatura: %0.2f C e Umidade: %0.2f%%", temp, umi);
      /**
      * @brief mwifi_root_write() envia dados para um ou mais nós filhos
      * 
      * @param dest_addr Endereço MAC do dispositivo que receberá os dados.  
      * @param dest_addrs_num Numero de destinos, nesse caso é 1
      * @param data_type Tipo de dado
      * @param data Vetor para armazenar os dados recebidos
      * @param size Tamanho do dado a ser enviado
      * @param block Se "true", a função espera até a confirmacao de recebimento seja recebida antes de retornar. 
      * Se "false", a função retorna imediatamente sem esperar pela confirmação.
      */
      ret = mwifi_root_write(dest_addr, 1, &data_type, data, size, false);
      break;
    }
    /*Se recebeu a mensagem com sucesso, imprime o que recebeu*/
    if(ret == MDF_OK){
      MDF_LOGI("data: %s", data);
    }
    /*Se nao recebeu com sucesso, indica erro*/
    else if(ret != MDF_ERR_TIMEOUT){
      MDF_LOGE("mwifi_read, ret: %x", ret);                                                           
    } 
  }  
}

/**
 * @brief Tarefa executada para ler o dados recebidos pelos nos filhos. 
 * 
 * Pode receber tres tipos de mensagens: 
 * Tipo 0: Dados de leitura de temperatura e umidade
 * Tipo 1: Solicitacao de envio de dados do tempo atualizado para o disp 1
 * Tipo 2: Solicitacao de envio de dados do tempo atualizado para o disp 2
 * 
 * @param arg 
 */
void root_task(void *arg)
{
  /*Inicializacao das variaveis utilizadas*/
  mdf_err_t ret                      = MDF_OK;
  char *data                         = MDF_MALLOC(MWIFI_PAYLOAD_LEN);
  size_t size                        = MWIFI_PAYLOAD_LEN;
  uint8_t src_addr[MWIFI_ADDR_LEN]   = {0x0};
  mwifi_data_type_t data_tipo      = {0};


  /*Para aguardar o TEMPO_ATT_BIT*/
  xEventGroupWaitBits(
    IM_event_group,                         //EventGroup q vai esperar
    TEMPO_ATT_BIT ,                         //Bit q esta esperando a mudanca
    pdFALSE,                                //Os bits n vao ser limpos dps de lidos      
    pdFALSE,                                //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
    portMAX_DELAY                           //tempo para esperar os dois bits dps q o primeiro é ativado   
  );

  while(1)
  {
    /*Tamanho da mensagem*/
    size = MWIFI_PAYLOAD_LEN;
    /*Preenche o vetor de dados com 0*/             
    memset(data, 0, MWIFI_PAYLOAD_LEN);   
    /**
    * @brief mwifi_root_read() recebe dados enviados pelos nós filhos 
    * 
    * @param src_addr Endereço do dispositivo que enviou os dados 
    * @param data_type Tipo de dado
    * @param data Vetor para armazenar o dado recebido
    * @param size Tamanho do dado recebido
    * @param portMAX_DELAY Tempo limite de espera
    */
    ret = mwifi_root_read(src_addr, &data_tipo, data, &size, portMAX_DELAY);
    /*Verifica se ocorreu erro na operacao de mwifi_root_read. MDF_OK indica que foi bem sucedida, se nao for somente registra o erro*/
    MDF_ERROR_CONTINUE(ret != MDF_OK, "<%s> mwifi_root_read", mdf_err_to_name(ret));
    /*Executa uma acao dependendo do tipo da mensagem*/ 
    switch (data_tipo.custom)
    {
    case 0:
      /*Extrai a temperatura e umidade dos dados recebidos, os armazena em temp e umi respectivamente*/
      sscanf(data, " Temperatura: %f C e Umidade: %f%%", &temp, &umi);
      /**
      * @brief publicar() e uma funcao de mqtt.h. Publica os dados no mqtt
      * 
      * @param topic Topico
      * @param qos O nivel de qualidade de servico da mensagem. Pode ser 0, 1 ou 2.
      * @param retain Se a mensagem deve ser retida ou n. Pode ser 0 (n retida ) ou 1 (retida).
      * @param dado O dado que e enviado.
      */
      publicar("giovani/teste/0", 0, 0, "Temperatura : %0.2fC", temp);
      publicar("giovani/teste/0", 0, 0, "Umidade : %0.2f%%", umi);
      break;
    /*O disp 1 ta pedindo p sincronizar*/
    case 1: 
      sincronizado = 0;
      break;
    /*O disp 2 ta pedindo p sincronizar  */
    case 2: 
      sincronizado = 1;
    }
    /*Coloca o bit ENVIARMW_DISPONIVEL_BIT em alto, permitindo a execucao da tarefa root_enviar_task*/
    xEventGroupSetBits(IM_event_group, ENVIARMW_DISPONIVEL_BIT);
    /*Aguarda 2s*/
    vTaskDelay(2000 / portTICK_RATE_MS);
  }
}


/**
 * @brief Inicializacao do WiFi, MQTT, SNTP e criacao das tarefas
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
  
  /* Cria o evento IM_event_group */
  criarEvento();

  /* Limpa os bits que sao utilizados garantindo o funcionamento  */
  xEventGroupClearBits(IM_event_group, INTERNET_DISPONIVEL_BIT);
  xEventGroupClearBits(IM_event_group, MQTT_DISPONIVEL_BIT);
  xEventGroupClearBits(IM_event_group, MWIFI_DISPONIVEL_BIT);
  xEventGroupClearBits(IM_event_group, TEMPO_ATT_BIT);
  xEventGroupClearBits(IM_event_group, ENVIARMW_DISPONIVEL_BIT);
  
  /* Inicilização do wifi e mqtt */
  wifi_start();
  mqtt_start();

  /*Inicialização MWIFI*/
  mw_start();

  /*Inicialização do sntp*/
  sntp_configurar();

  /* Cria as tarefas do root*/
  xTaskCreate(root_task, "root_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
  xTaskCreate(root_enviar_task, "root_enviar_task", configMINIMAL_STACK_SIZE * 3, NULL, 4, NULL);
  xTaskCreate(hiberna_task, "hibernar_task", configMINIMAL_STACK_SIZE * 3, NULL, 3, NULL);

  /* Para evitar watch dog*/
  while(1)
  {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  
}


