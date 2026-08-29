#include "webmonitor.h"

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include "telemetryManager.h"

extern WebServer server;
extern void recibirComandoWeb(String dispositivo, bool estado);

namespace
{
WebSocketsServer webSocket(81);
char ultimoEstadoWeb[256] = "{}";

void servirArchivo(const char *ruta, const char *tipo)
{
  File archivo = SPIFFS.open(ruta, "r");
  if (!archivo)
  {
    server.send(404, "text/plain", "Archivo web no encontrado");
    return;
  }
  server.streamFile(archivo, tipo);
  archivo.close();
}

void enviarEstadoViaje()
{
  const EstadoViaje viaje = obtenerEstadoViaje();
  char respuesta[400];
  snprintf(respuesta, sizeof(respuesta),
    "{\"activo\":%s,\"id\":\"%s\",\"origen\":\"%s\",\"destino\":\"%s\","
    "\"tanque\":\"%s\",\"inicio\":%lu,\"nube\":%s,\"heap\":%u}",
    viaje.activo ? "true" : "false", viaje.id, viaje.origen, viaje.destino,
    viaje.tanque, viaje.inicioEpoch, telemetriaConectada() ? "true" : "false",
    ESP.getFreeHeap());
  server.send(200, "application/json", respuesta);
}

void iniciarViajeWeb()
{
  const bool ok = iniciarViaje(server.arg("origen"), server.arg("destino"),
                               server.arg("tanque"));
  server.send(ok ? 200 : 409, "application/json",
              ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"Datos incompletos o viaje activo\"}");
}

void finalizarViajeWeb()
{
  const bool ok = finalizarViaje();
  server.send(ok ? 200 : 409, "application/json",
              ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"No existe un viaje activo\"}");
}

void eventoWebSocket(uint8_t cliente, WStype_t tipo, uint8_t *payload, size_t longitud)
{
  if (tipo == WStype_CONNECTED)
  {
    webSocket.sendTXT(cliente, ultimoEstadoWeb);
    return;
  }
  if (tipo != WStype_TEXT || longitud > 160) return;

  const String mensaje(reinterpret_cast<char *>(payload), longitud);
  if (mensaje.indexOf("\"accion\":\"bomba\"") >= 0)
  {
    recibirComandoWeb("bomba", mensaje.indexOf("\"estado\":true") >= 0);
  }
  else if (mensaje.indexOf("\"accion\":\"peltier\"") >= 0)
  {
    recibirComandoWeb("peltier", mensaje.indexOf("\"estado\":true") >= 0);
  }
}
} // namespace

void iniciarWebMonitor()
{
  if (!SPIFFS.begin(true)) Serial.println("ERROR: no se pudo montar SPIFFS");
  server.on("/monitor", []() { servirArchivo("/index.html", "text/html"); });
  server.on("/style.css", []() { servirArchivo("/style.css", "text/css"); });
  server.on("/script.js", []() { servirArchivo("/script.js", "application/javascript"); });
  server.on("/api/viaje", HTTP_GET, enviarEstadoViaje);
  server.on("/api/viaje/iniciar", HTTP_POST, iniciarViajeWeb);
  server.on("/api/viaje/finalizar", HTTP_POST, finalizarViajeWeb);
  server.on("/favicon.ico", []() { server.send(204); });
  server.onNotFound([]() { server.send(404, "text/plain", "Ruta no encontrada"); });
  webSocket.begin();
  webSocket.onEvent(eventoWebSocket);
}

void actualizarDatosWeb(float temperatura, float ph, bool bomba,
                        bool peltier, bool manual)
{
  snprintf(ultimoEstadoWeb, sizeof(ultimoEstadoWeb),
    "{\"temperatura\":%.1f,\"ph\":%.2f,\"bomba\":%s,\"peltier\":%s,"
    "\"manual\":%s,\"wifi\":%s,\"nube\":%s}",
    temperatura, ph, bomba ? "true" : "false", peltier ? "true" : "false",
    manual ? "true" : "false", WiFi.status() == WL_CONNECTED ? "true" : "false",
    telemetriaConectada() ? "true" : "false");
  webSocket.broadcastTXT(ultimoEstadoWeb);
}

void actualizarWebSocket() { webSocket.loop(); }
