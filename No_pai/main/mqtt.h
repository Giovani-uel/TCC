#ifndef MQTT_H
#define MQTT_H

void mqtt_start(void);
int publicar(char *topic, int qos, int retain, char *data_temp,...);


#endif
