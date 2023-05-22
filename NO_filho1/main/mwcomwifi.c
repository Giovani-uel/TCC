#include "mwcomwifi.h"
#include "mdf_common.h" 
#include "mwifi.h" 
#include "conectado.h" 

/*Define a tag para uso em logs*/
static const char *TAG = "MW"; 


/**
 * @brief Responsavel por enviar uma mensagem para o no pai com a temperatura e umidade
 * 
 * @param temp Temperatura lida pelo sensor
 * @param umi Umidade lida pelo sensor
 */
void mandar_no(float temp, float umi){
    /*Variaveis necessarias para o envio de dados*/
    mdf_err_t ret = MDF_OK; 
    size_t size   = 0; 
    char *data    = MDF_MALLOC(MWIFI_PAYLOAD_LEN); 
    mwifi_data_type_t data_type = {0x0}; 

    /*Verifica se o dispositivo está conectado a rede mesh*/
    /*se conectado:*/
    if(mwifi_is_connected()){ 
        /*Formata a mensagem com a temperatura e umidade*/
        size = sprintf(data, " Temperatura: %0.2f C e Umidade:  %0.2f%%", temp, umi); 
        /*Envia a mensagem para o nó pai*/
        ret = mwifi_write(NULL, &data_type, data, size, true); 
        /*Libera a memória alocada para a mensagem*/
        MDF_FREE(data); 
    }
    /*Se nao conectado*/
    else{ 
        /*Mostra mensagem informando que o dispositivo nao esta conectado*/
        MDF_LOGI("MWIFI ainda nao esta conectado");  
    }

}

// Callback responsável por tratar os eventos do MWIFI
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event); // Loga uma mensagem com o evento recebido

    switch (event) {
        case MDF_EVENT_MWIFI_STARTED: // Quando a rede mesh é iniciada
            MDF_LOGI("Rede mesh comecou"); // Loga uma mensagem informando que a rede mesh foi iniciada
            break;

        case MDF_EVENT_MWIFI_PARENT_CONNECTED: // Quando o nó filho se conecta ao nó pai
            MDF_LOGI("No pai esta conectado"); // Loga uma mensagem informando que o nó pai está conectado
            xEventGroupSetBits(IM_event_group, MWIFI_DISPONIVEL_BIT);
            break;

        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED: // Quando o nó filho se desconecta do nó pai
            MDF_LOGI("No pai foi desconectado"); // Loga uma mensagem informando que o nó pai foi desconectado
            break;

        default:
            break;
    }

    return MDF_OK;
}

// Função responsável por iniciar a rede mesh
void mw_start(void){
    
    mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT(); // Inicializa a estrutura de configuração do MWIFI
    mwifi_config_t config   = { // Define a estrutura de configuração do MWIFI
        .channel   = 13, // Canal da rede Wi-Fi mesh
        .mesh_id   = "123456", // ID da rede Wi-Fi mesh
        .mesh_type = MESH_NODE,//Define o dispositivo como no filho
    };
    /*Inicializa o loop de eventos e registra o evento*/
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    /* inicializa o módulo WiFi mesh com as configuracoes padrao*/
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    /*Altera as configuracoes padroes para as feitas em config*/
    MDF_ERROR_ASSERT(mwifi_set_config(&config));

    //Espera o wifi ser conectado
    xEventGroupWaitBits(
        IM_event_group,               //EventGroup q vai esperar
        INTERNET_DISPONIVEL_BIT ,     //Bits q está esperando a mudanca
        pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
        pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
        portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
    );

    /*Inicia o módulo WiFi mesh*/
    MDF_ERROR_ASSERT(mwifi_start());

}