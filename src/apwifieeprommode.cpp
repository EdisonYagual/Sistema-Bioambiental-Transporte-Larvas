#include "apwifieeprommode.h"

#include <ESPmDNS.h>
#include <Preferences.h>

WebServer server(80);

namespace
{
Preferences preferencias;

bool conectar(const String &ssid, const String &clave, uint8_t segundos)
{
  if (ssid.isEmpty()) return false;
  WiFi.begin(ssid.c_str(), clave.c_str());
  const unsigned long limite = millis() + segundos * 1000UL;
  while (WiFi.status() != WL_CONNECTED && static_cast<long>(limite - millis()) > 0)
  {
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

void guardarRed(const String &ssid, const String &clave)
{
  preferencias.begin("redes", false);
  const uint8_t siguiente = preferencias.getUChar("siguiente", 0) % 2;
  const char *ssidKey = siguiente == 0 ? "ssid0" : "ssid1";
  const char *passKey = siguiente == 0 ? "pass0" : "pass1";
  preferencias.putString(ssidKey, ssid.substring(0, 32));
  preferencias.putString(passKey, clave.substring(0, 63));
  preferencias.putUChar("siguiente", (siguiente + 1) % 2);
  preferencias.end();
}

void paginaConfiguracion()
{
  static const char pagina[] PROGMEM =
    "<!doctype html><html lang='es'><meta name='viewport' content='width=device-width'>"
    "<style>body{font-family:system-ui;background:#07121d;color:#fff;max-width:420px;margin:40px auto;padding:20px}"
    "input,button{width:100%;box-sizing:border-box;padding:14px;margin:7px 0;border-radius:10px;border:1px solid #345}"
    "button{background:#20a99b;color:white;font-weight:bold}</style>"
    "<h2>Configurar Wi-Fi</h2><p>Se pueden recordar hasta dos redes.</p>"
    "<form method='POST' action='/wifi'><input name='ssid' maxlength='32' placeholder='Nombre de red' required>"
    "<input type='password' name='password' maxlength='63' placeholder='Contraseña'>"
    "<button>Conectar y guardar</button></form></html>";
  server.send(200, "text/html", pagina);
}

void recibirConfiguracion()
{
  const String ssid = server.arg("ssid");
  const String clave = server.arg("password");
  if (ssid.isEmpty())
  {
    server.send(400, "text/plain", "Falta el nombre de la red");
    return;
  }
  WiFi.mode(WIFI_AP_STA);
  if (conectar(ssid, clave, 10))
  {
    guardarRed(ssid, clave);
    server.send(200, "text/html", "<h2>Red guardada</h2><p>Reinicie el equipo.</p>");
  }
  else
  {
    server.send(400, "text/html", "<h2>No se pudo conectar</h2><p>Revise nombre y contraseña.</p>");
  }
}
} // namespace

bool lastRed()
{
  WiFi.mode(WIFI_STA);
  preferencias.begin("redes", true);
  const String ssid0 = preferencias.getString("ssid0", "");
  const String pass0 = preferencias.getString("pass0", "");
  const String ssid1 = preferencias.getString("ssid1", "");
  const String pass1 = preferencias.getString("pass1", "");
  preferencias.end();

  const bool conectado = conectar(ssid0, pass0, 5) || conectar(ssid1, pass1, 5);
  if (conectado)
  {
    MDNS.begin("proyectocamarones");
    Serial.printf("Wi-Fi conectado. Monitor: http://%s/monitor\n",
                  WiFi.localIP().toString().c_str());
  }
  return conectado;
}

void iniciarAP()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Proyecto Camarones", "123456789");
  server.on("/", HTTP_GET, paginaConfiguracion);
  server.on("/wifi", HTTP_POST, recibirConfiguracion);
  Serial.printf("AP listo en http://%s/\n", WiFi.softAPIP().toString().c_str());
}

void loopAP() { server.handleClient(); }

void intentoconexion(const char *apname, const char *appassword)
{
  // Se conservan los parametros por compatibilidad; el nombre y clave actuales
  // del AP siguen siendo exactamente los utilizados por el proyecto original.
  (void)apname;
  (void)appassword;
  if (!lastRed()) iniciarAP();
}
