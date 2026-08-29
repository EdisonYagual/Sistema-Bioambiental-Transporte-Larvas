#ifndef APWIFIEEPROMMODE_H
#define APWIFIEEPROMMODE_H


#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>


// =====================================================
// SERVIDOR WEB COMPARTIDO
// =====================================================

// Este mismo servidor será usado por:
// - Configuración WiFi (AP)
// - WebMonitor
extern WebServer server;



// =====================================================
// FUNCIONES WIFI
// =====================================================


// Intenta conectar a redes guardadas en EEPROM
// Si no encuentra ninguna crea el AP
void intentoconexion(
    const char* apname,
    const char* appassword
);



// Crea la red WiFi:
// Proyecto Camarones
// password: 123456789
void iniciarAP();

// Intenta las dos redes almacenadas en memoria no volatil.
bool lastRed();



// Mantiene activo el servidor cuando está en modo AP
void loopAP();



#endif
