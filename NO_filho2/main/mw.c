#include "mw.h"
#include "mdf_common.h"
#include "mwifi.h"
#include "conectado.h"

static const char *TAG = "MWCOMWIFI";


/**
 * @brief Lida com os eventos da rede mesh
 * 
 * @param event Indica o evento que ocorreu na rede mesh
 * @param ctx --
 * @return mdf_err_t 
 */
static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{
    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event) {
        /*Rede mesh e iniciada*/
        case MDF_EVENT_MWIFI_STARTED:
            MDF_LOGI("MESH comecou");
            break;
        /*Conectado ao no pai*/
        case MDF_EVENT_MWIFI_PARENT_CONNECTED:
            MDF_LOGI("No pai esta conectado");
            xEventGroupSetBits(IM_event_group, MW_DISPONIVEL_BIT);
            
            break;

        /*Desconectado do no pai*/
        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
            MDF_LOGI("No pai nao esta conectado");
            xEventGroupClearBits(IM_event_group, MW_DISPONIVEL_BIT);
            break;

        default:
            break;
    }

    return MDF_OK;
}

/**
 * @brief Realiza a rotina de inicializacao do MWIFI
 * 
 */
void mw_start(void){
    
    /*Salva configuracoes padroes ao cfg*/
    mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
    /*Salva configuracoes especificas ao config*/
    mwifi_config_t config   = {
        .channel   = 13,
        .mesh_id   = "123456",
        .mesh_type = MESH_NODE,
    };

    // registra o callback de eventos da rede mesh
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));

    // inicializa o controlador WiFi com as configuracoes padroes
    MDF_ERROR_ASSERT(mwifi_init(&cfg));

    // Altera as configuracoes especificas alteradas em config  
    MDF_ERROR_ASSERT(mwifi_set_config(&config));

    /*Aguarda o wifi ser iniciado*/
    xEventGroupWaitBits(
    IM_event_group,                       //EventGroup q vai esperar
    INTERNET_DISPONIVEL_BIT ,             //Bits q está esperando a mudanca
    pdTRUE,                               //Os bits vao ser limpos dps de lidos      
    pdFALSE,                              //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
    portMAX_DELAY                         //tempo para esperar os dois bits dps q o primeiro é ativado   
    );

    // inicia a rede mesh
    MDF_ERROR_ASSERT(mwifi_start());

}
