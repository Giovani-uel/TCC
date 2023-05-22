#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "conectado.h"

/*Cria o handle do event group*/
EventGroupHandle_t IM_event_group;

void criarEvento(void)
{
    /*Cria o evento*/
    IM_event_group = xEventGroupCreate();
}