#include "telemetryManager.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <time.h>

namespace
{
// Complete estas dos constantes antes de habilitar Ubidots.
// El token nunca debe publicarse en GitHub.
constexpr char UBIDOTS_TOKEN[] = "CAMBIAR_TOKEN_UBIDOTS";
constexpr char DEVICE_LABEL[] = "tanque-01";
constexpr char MQTT_HOST[] = "industrial.api.ubidots.com";
constexpr uint16_t MQTT_PORT = 1883;
constexpr uint32_t PUBLISH_INTERVAL_MS = 60000;
constexpr uint32_t RECONNECT_MIN_MS = 5000;
constexpr uint8_t QUEUE_CAPACITY = 20;

struct Muestra
{
  float temperatura;
  float ph;
  uint32_t epoch;
  bool bomba;
  bool peltier;
  bool manual;
};

WiFiClient networkClient;
PubSubClient mqtt(networkClient);
Preferences preferences;
EstadoViaje viaje{};
Muestra cola[QUEUE_CAPACITY];
uint8_t colaInicio = 0;
uint8_t colaCantidad = 0;
uint32_t ultimoEnvio = 0;
uint32_t ultimoIntento = 0;

// Evento pendiente de viaje. Se conserva en RAM si Internet/MQTT no esta listo
// y se publica en cuanto se recupera la conexion.
struct EventoViajePendiente
{
  bool pendiente;
  bool inicio;
  uint32_t epoch;
  uint32_t duracionSegundos;
};

EventoViajePendiente eventoViaje{};

void copiarTexto(char *destino, size_t capacidad, const String &texto)
{
  if (capacidad == 0) return;
  size_t salida = 0;
  // Se aceptan nombres humanos, pero se eliminan caracteres que romperian el
  // JSON enviado a Ubidots o permitirian inyectar campos adicionales.
  for (size_t i = 0; i < texto.length() && salida < capacidad - 1; ++i)
  {
    const char c = texto[i];
    if (c >= 32 && c != '"' && c != '\\') destino[salida++] = c;
  }
  destino[salida] = '\0';
}

void guardarViaje()
{
  preferences.begin("viaje", false);
  preferences.putBool("activo", viaje.activo);
  preferences.putString("id", viaje.id);
  preferences.putString("origen", viaje.origen);
  preferences.putString("destino", viaje.destino);
  preferences.putString("tanque", viaje.tanque);
  preferences.putULong("inicio", viaje.inicioEpoch);
  preferences.end();
}

void cargarViaje()
{
  preferences.begin("viaje", true);
  viaje.activo = preferences.getBool("activo", false);
  copiarTexto(viaje.id, sizeof(viaje.id), preferences.getString("id", ""));
  copiarTexto(viaje.origen, sizeof(viaje.origen), preferences.getString("origen", ""));
  copiarTexto(viaje.destino, sizeof(viaje.destino), preferences.getString("destino", ""));
  copiarTexto(viaje.tanque, sizeof(viaje.tanque), preferences.getString("tanque", ""));
  viaje.inicioEpoch = preferences.getULong("inicio", 0);
  preferences.end();
}

void encolar(const Muestra &muestra)
{
  // Cola circular de memoria fija: nunca consume heap de forma creciente.
  if (colaCantidad == QUEUE_CAPACITY)
  {
    colaInicio = (colaInicio + 1) % QUEUE_CAPACITY;
    colaCantidad--;
  }
  const uint8_t posicion = (colaInicio + colaCantidad) % QUEUE_CAPACITY;
  cola[posicion] = muestra;
  colaCantidad++;
}

bool publicar(const Muestra &muestra)
{
  if (!mqtt.connected()) return false;

  char topic[96];
  char payload[600];
  snprintf(topic, sizeof(topic), "/v1.6/devices/%s", DEVICE_LABEL);

  // Un solo mensaje contiene todas las variables y el contexto del viaje.
  const int longitud = snprintf(
    payload, sizeof(payload),
    "{\"temperatura\":{\"value\":%.2f,\"timestamp\":%llu,\"context\":{\"viaje_id\":\"%s\",\"origen\":\"%s\",\"destino\":\"%s\",\"tanque\":\"%s\"}},"
    "\"ph\":%.2f,\"bomba\":%d,\"peltier\":%d,\"modo_manual\":%d,\"heap_libre\":%u}",
    muestra.temperatura, static_cast<unsigned long long>(muestra.epoch) * 1000ULL,
    viaje.activo ? viaje.id : "sin-viaje", viaje.origen, viaje.destino, viaje.tanque,
    muestra.ph, muestra.bomba, muestra.peltier, muestra.manual, ESP.getFreeHeap());

  return longitud > 0 && longitud < static_cast<int>(sizeof(payload)) &&
         mqtt.publish(topic, payload, false);
}

bool publicarEventoViaje()
{
  if (!eventoViaje.pendiente || !mqtt.connected()) return false;

  char topic[96];
  char payload[760];
  snprintf(topic, sizeof(topic), "/v1.6/devices/%s", DEVICE_LABEL);

  const unsigned long long timestamp =
    static_cast<unsigned long long>(eventoViaje.epoch) * 1000ULL;
  const float duracionMinutos = eventoViaje.duracionSegundos / 60.0f;
  const int activo = eventoViaje.inicio ? 1 : 0;
  const int codigoEvento = eventoViaje.inicio ? 1 : 2;

  // Los valores numericos aparecen como variables normales en Ubidots. Los
  // textos del recorrido permanecen adjuntos como contexto consultable.
  const int longitud = snprintf(
    payload, sizeof(payload),
    "{\"viaje_activo\":{\"value\":%d,\"timestamp\":%llu,\"context\":{\"viaje_id\":\"%s\",\"origen\":\"%s\",\"destino\":\"%s\",\"tanque\":\"%s\",\"tipo_evento\":\"%s\"}},"
    "\"evento_viaje\":{\"value\":%d,\"timestamp\":%llu,\"context\":{\"viaje_id\":\"%s\",\"origen\":\"%s\",\"destino\":\"%s\",\"tanque\":\"%s\",\"tipo_evento\":\"%s\"}},"
    "\"inicio_viaje\":%lu,\"fin_viaje\":%lu,\"duracion_viaje_minutos\":%.2f}",
    activo, timestamp, viaje.id, viaje.origen, viaje.destino, viaje.tanque,
    eventoViaje.inicio ? "inicio" : "fin",
    codigoEvento, timestamp, viaje.id, viaje.origen, viaje.destino, viaje.tanque,
    eventoViaje.inicio ? "inicio" : "fin",
    viaje.inicioEpoch,
    eventoViaje.inicio ? 0UL : eventoViaje.epoch,
    eventoViaje.inicio ? 0.0f : duracionMinutos);

  if (longitud <= 0 || longitud >= static_cast<int>(sizeof(payload))) return false;
  if (!mqtt.publish(topic, payload, false)) return false;
  eventoViaje.pendiente = false;
  return true;
}

void conectarMQTT()
{
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  if (strcmp(UBIDOTS_TOKEN, "CAMBIAR_TOKEN_UBIDOTS") == 0) return;
  if (millis() - ultimoIntento < RECONNECT_MIN_MS) return;
  ultimoIntento = millis();

  char clientId[40];
  snprintf(clientId, sizeof(clientId), "camaron-%08X", static_cast<unsigned>(ESP.getEfuseMac()));
  mqtt.connect(clientId, UBIDOTS_TOKEN, "");
}
} // namespace

void iniciarTelemetria()
{
  cargarViaje();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(700);
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.google.com");
}

void actualizarTelemetria()
{
  conectarMQTT();
  mqtt.loop();
  if (eventoViaje.pendiente)
  {
    publicarEventoViaje();
    return;
  }
  if (mqtt.connected() && colaCantidad > 0 && publicar(cola[colaInicio]))
  {
    colaInicio = (colaInicio + 1) % QUEUE_CAPACITY;
    colaCantidad--;
  }
}

void registrarMuestraTelemetria(float temperatura, float ph, bool bomba,
                                bool peltier, bool manual)
{
  if (millis() - ultimoEnvio < PUBLISH_INTERVAL_MS) return;
  ultimoEnvio = millis();
  // JSON no admite NaN. Una desconexion del DS18B20 nunca debe producir una
  // muestra corrupta ni ocupar un espacio de la cola offline.
  if (!isfinite(temperatura) || !isfinite(ph)) return;
  Muestra muestra{temperatura, ph, static_cast<uint32_t>(obtenerEpochActual()),
                  bomba, peltier, manual};
  if (!publicar(muestra)) encolar(muestra);
}

bool iniciarViaje(const String &origen, const String &destino, const String &tanque)
{
  if (viaje.activo || origen.isEmpty() || destino.isEmpty()) return false;
  viaje.activo = true;
  viaje.inicioEpoch = obtenerEpochActual();
  const unsigned long referencia = viaje.inicioEpoch ? viaje.inicioEpoch : millis() / 1000;
  snprintf(viaje.id, sizeof(viaje.id), "V-%lu-%08X", referencia,
           static_cast<unsigned>(ESP.getEfuseMac()));
  copiarTexto(viaje.origen, sizeof(viaje.origen), origen);
  copiarTexto(viaje.destino, sizeof(viaje.destino), destino);
  copiarTexto(viaje.tanque, sizeof(viaje.tanque), tanque);
  guardarViaje();
  eventoViaje = {true, true, static_cast<uint32_t>(viaje.inicioEpoch), 0};
  publicarEventoViaje();
  return true;
}

bool finalizarViaje()
{
  if (!viaje.activo) return false;
  const uint32_t fin = static_cast<uint32_t>(obtenerEpochActual());
  const uint32_t duracion = fin >= viaje.inicioEpoch ? fin - viaje.inicioEpoch : 0;
  viaje.activo = false;
  guardarViaje();
  eventoViaje = {true, false, fin, duracion};
  publicarEventoViaje();
  return true;
}

EstadoViaje obtenerEstadoViaje() { return viaje; }
bool telemetriaConectada() { return mqtt.connected(); }

unsigned long obtenerEpochActual()
{
  const time_t ahora = time(nullptr);
  return ahora > 1700000000 ? static_cast<unsigned long>(ahora) : 0;
}
