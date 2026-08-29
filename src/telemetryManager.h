#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <Arduino.h>

// Datos descriptivos de un recorrido. No intervienen en el control local.
struct EstadoViaje
{
  bool activo;
  char id[40];
  char origen[48];
  char destino[48];
  char tanque[32];
  unsigned long inicioEpoch;
};

// Inicializa NTP, preferencias y el cliente MQTT de Ubidots.
void iniciarTelemetria();

// Mantiene la conexion MQTT y vacia gradualmente la cola sin bloquear el control.
void actualizarTelemetria();

// Recibe una muestra local. Publica cada 60 s o la conserva en una cola fija.
void registrarMuestraTelemetria(float temperatura, float ph, bool bomba,
                                bool peltier, bool manual);

// Gestion del recorrido desde la pagina local del telefono.
bool iniciarViaje(const String &origen, const String &destino, const String &tanque);
bool finalizarViaje();
EstadoViaje obtenerEstadoViaje();

bool telemetriaConectada();
unsigned long obtenerEpochActual();

#endif
