#ifndef WEBMONITOR_H
#define WEBMONITOR_H


#include <Arduino.h>


// Inicia servidor web
void iniciarWebMonitor();


// Actualiza datos enviados al navegador
void actualizarDatosWeb(
    float temperatura,
    float ph,
    bool bomba,
    bool peltier,
    bool manual
);


// Atiende comunicación websocket
void actualizarWebSocket();


// Recibir comandos desde página web
void recibirComandoWeb(
    String dispositivo,
    bool estado
);


#endif