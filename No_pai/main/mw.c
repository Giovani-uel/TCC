#include "mw.h"
#include "mdf_common.h"
#include "mwifi.h"
#include "conectado.h"

static const char *TAG = "MW";

static mdf_err_t event_loop_cb(mdf_event_loop_t event, void *ctx)
{

    MDF_LOGI("event_loop_cb, event: %d", event);

    switch (event) {
        case MDF_EVENT_MWIFI_STARTED:
            MDF_LOGI("MESH is started");
            xEventGroupSetBits(IM_event_group, MWIFI_DISPONIVEL_BIT);
            break;

        case MDF_EVENT_MWIFI_PARENT_CONNECTED:
            MDF_LOGI("Parent is connected on station interface");
            break;

        case MDF_EVENT_MWIFI_PARENT_DISCONNECTED:
            MDF_LOGI("Parent is disconnected on station interface");
            break;

        default:
            break;
    }

    return MDF_OK;
}


void mw_start(void)
{
    /* Configura o mwifi*/
    mwifi_init_config_t cfg = MWIFI_INIT_CONFIG_DEFAULT();
    mwifi_config_t config   = {
        .channel   = 13,
        .mesh_id   = "123456",
        .mesh_type = MESH_ROOT,
    };

    /**
     * @brief Set the log level for serial port printing.
     */
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    /**
    * @brief Inicializa a rede mesh.
    */
    MDF_ERROR_ASSERT(mdf_event_loop_init(event_loop_cb));
    MDF_ERROR_ASSERT(mwifi_init(&cfg));
    MDF_ERROR_ASSERT(mwifi_set_config(&config));

    /*Para aguardar o wifi estar conectado*/
    xEventGroupWaitBits(
    IM_event_group,               //EventGroup q vai esperar
    MQTT_DISPONIVEL_BIT ,         //Bits q está esperando a mudanca
    pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
    pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
    portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
    );

    /*Espera o wifi ser conectado*/ 
    xEventGroupWaitBits(
       IM_event_group,               //EventGroup q vai esperar
       INTERNET_DISPONIVEL_BIT ,     //Bits q está esperando a mudanca
       pdFALSE,                      //Os bits n vao ser limpos dps de lidos      
       pdFALSE,                      //pdFALSE é igual a "or",ou seja, espera qualquer um dos dois para continuar  
       portMAX_DELAY                 //tempo para esperar os dois bits dps q o primeiro é ativado   
    );

    MDF_ERROR_ASSERT(mwifi_start());


}






