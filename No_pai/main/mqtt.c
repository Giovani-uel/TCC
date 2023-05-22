#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "mqtt_client.h"
#include "mqtt.h"
#include "conectado.h"

static const char *TAG = "MQTT";

esp_mqtt_client_handle_t client;
char mqtt_buf_payload[50];
int MQTT_BUF_PAYLOAD_LEN = 50;

/**
 * @brief Publica uma mensagem em um topico mqtt
 * 
 * @param topic Topico mqtt em que a mensagem vai ser publicada.
 * @param qos Qualidade de servico da mensagem (0, 1 ou 2).
 * @param retain Retencao (0 ou 1)
 * @param fmt mensagem
 * @param ... 
 * @return int 
 */
int publicar(char *topic, int qos, int retain, char *fmt,...)
{   
    /*Converte a leitura em uma string*/
    
    /*Ponteiro para lista de argumentos*/
    va_list  argptr; 
    /*Inicializa função va_*/                      
    va_start( argptr, fmt );              
    /**
     * @brief vsnprintf() escreve uma sequência de caracteres em uma string de tamanho limitado, seguindo 
     * um formato especificado por uma string de formato
     * 
     * @param mqtt_buf_payload Buffer de destino, onde a sequencia de carecteres sera escrita.
     * @param MQTT_BUF_PAYLOAD_LEN Tamanho máximo permitido para a string de destino.
     * @param fmt String de formato que contém especificadores de formato.
     * @param argptr Ponteiro para a lista de argumentos.
     */
    vsnprintf(mqtt_buf_payload,MQTT_BUF_PAYLOAD_LEN, fmt, argptr);
    /*Encerra va_*/
    va_end( argptr );                     
    
    /*Publica no broker mqtt*/
    return esp_mqtt_client_publish(client,topic,mqtt_buf_payload,0,qos,retain);
}




/**
 * @brief Manipulador de eventos do mqtt
 * 
 * @param handler_args Argumentos do manipulador de eventos.
 * @param base Base do evento. 
 * @param event_id ID do evento MQTT.
 * @param event_data Dados do evento MQTT.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Evento despachado do loop de eventos base=%s, ID do evento=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    client = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    /* Caso mqtt conectado, seta o bit MQTT_DISPONIVEL_BIT indicando que o mqtt esta disponivel*/    
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Conectado");
        xEventGroupSetBits(IM_event_group, MQTT_DISPONIVEL_BIT);
        break;

    /* Caso mqtt desconectado limpa o bit MQTT_DISPONIVEL_BIT indicando que o mqtt nao esta conectado*/
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Desconectado");
        xEventGroupClearBits(IM_event_group, MQTT_DISPONIVEL_BIT);
        break;

    /* Evento de inscricao, indica que ocorreu*/
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
        break;

    /* Evento de UNSUBSCRIBED, indica que ocorreu*/
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
        break;

    /* Evento de publicacao indica que ocorreu*/
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT Publicado");
        break;
    /*Indica que o cliente MQTT recebeu dados de um tópico ao qual ele se inscreveu*/
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        break;

    /*Indica se ocorreu um erro*/
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    /*Indica se ocorreu algum outro evento*/    
    default:
        ESP_LOGI(TAG, "Outro evento id");
        break;
    }
}

/**
 * @brief Configura e inicializa o cliente mqtt
 * 
 */
void mqtt_start(void)
{
   /*Configura o cliente mqtt*/ 
   esp_mqtt_client_config_t mqtt_cfg = 
   {
        /*URL do broker mqtt*/
       .uri = "mqtt://broker.emqx.io",                                                 
   };
   /*Inicializa o cliente com a config acima*/
   client = esp_mqtt_client_init(&mqtt_cfg);
  
   /**
    * @brief Registra o cliente configurado como um identificador de evento
    * 
    * @param client Identificador do cliente mqtt
    * @param ESP_EVENT_ANY_ID ID do evento a ser registrado(qualquer um nesse caso)
    * @param mqtt_event_handler Identificador desse evento
    * @param client Argumento passado
    */
   esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client); 
   
   /*Espera o wifi ser conectado*/ 
   xEventGroupWaitBits(
       IM_event_group,               //EventGroup q vai esperar
       INTERNET_DISPONIVEL_BIT ,     //Bits q está esperando a mudanca
       pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
       pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
       portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
   );
   /*Inicializa o cliente mqtt configurado*/
   esp_mqtt_client_start(client);                                                       
    
}



