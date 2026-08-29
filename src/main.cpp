#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <SPI.h>
#include <math.h>
#include "apwifieeprommode.h"
#include <EEPROM.h>
#include "webmonitor.h"
#include <XPT2046_Touchscreen.h>
#include "telemetryManager.h"
//#define TOUCH_IRQ 26

// ===================== PINES =====================

// TFT
constexpr uint8_t PIN_TFT_CS = 5;
constexpr uint8_t PIN_TFT_DC = 16;
constexpr uint8_t PIN_TFT_RST = 17;
constexpr uint8_t PIN_TFT_LED = 32;

//SPI
constexpr uint8_t PIN_SPI_SCK  = 18;//T_CLK
constexpr uint8_t PIN_SPI_MISO = 19;//T_DO
constexpr uint8_t PIN_SPI_MOSI = 23;//T_DIN

//Touch
constexpr uint8_t PIN_TOUCH_CS = 25;//T_CS

//Sensores
constexpr uint8_t PIN_DS18B20 = 4;
constexpr uint8_t PIN_PH_SENSOR = 34;

//Actuadores
constexpr uint8_t PIN_PELTIER = 27;
constexpr uint8_t PIN_AIR_PUMP = 26;

constexpr unsigned long READ_INTERVAL_MS = 1000;

// Retroiluminacion: se atenúa y apaga sin dormir el ESP32 ni detener el control.
constexpr unsigned long BACKLIGHT_DIM_MS = 60000;
constexpr unsigned long BACKLIGHT_OFF_MS = 90000;
constexpr uint8_t BACKLIGHT_FULL = 255;
constexpr uint8_t BACKLIGHT_DIM = 35;
constexpr uint8_t BACKLIGHT_CHANNEL = 0;

// ===================== OBJETOS =====================

Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);
XPT2046_Touchscreen ts(PIN_TOUCH_CS);
OneWire oneWire(PIN_DS18B20);
DallasTemperature tempSensor(&oneWire);

// ===================== PALETA DE COLORES =====================

#define COLOR_FONDO     0x18C3   // azul oscuro elegante
#define COLOR_BOTON     ILI9341_BLUE   // azul claro moderno
#define COLOR_BOTON_ALT ILI9341_RED   // amarillo suave
#define COLOR_TEXTO     ILI9341_WHITE

// ===================== MODO DEL SISTEMA =====================
enum ModoSistema
{
  AUTOMATICO,
  MANUAL
};

ModoSistema modoActual = AUTOMATICO;

// ===================== PANTALLAS =====================
enum PantallaSistema
{
  MENU,
  PANTALLA_AUTOMATICO,
  PANTALLA_MANUAL,
  PANTALLA_DATOS
};

PantallaSistema pantallaActual = MENU;

// ===================== VARIABLES COMPARTIDAS =====================
float temperatura = 0;
float phValue = 7;

// Estados manuales
bool bombaEstado = false;
bool peltierEstado = false;

// Estados automaticos
bool peltierState = false;
bool pumpState = false;

// Indica si trabaja manual
bool modoManualActivo = false;

unsigned long lastRead = 0;

volatile bool pantallaNeedsUpdate = true;

volatile unsigned long ultimaInteraccionPantalla = 0;
volatile bool pantallaIluminada = true;

bool mostrarDatosManual = false;


// ===================== CONTROL DESDE WEB =====================

void recibirComandoWeb(
  String dispositivo,
  bool estado
)
{

  modoManualActivo = true;
  modoActual = MANUAL;


  if(dispositivo == "bomba")
  {
    bombaEstado = estado;
    pumpState = estado;

    digitalWrite(
      PIN_AIR_PUMP,
      estado
    );
  }



  if(dispositivo == "peltier")
  {
    peltierEstado = estado;
    peltierState = estado;

    digitalWrite(
      PIN_PELTIER,
      estado
    );
  }


  actualizarDatosWeb(
    temperatura,
    phValue,
    pumpState,
    peltierState,
    modoManualActivo
  );


  pantallaNeedsUpdate = true;

}

// ===================== FREERTOS =====================

TaskHandle_t taskSensores;
TaskHandle_t taskControl;
TaskHandle_t taskDisplay;
TaskHandle_t taskWiFi;
TaskHandle_t taskTouch;

// Protección del bus SPI entre TFT y Touch
SemaphoreHandle_t spiMutex;

// ===================== BOTONES =====================
struct Boton
{
  int x;
  int y;
  int w;
  int h;
  String texto;
};

// -------- MENU PRINCIPAL --------
Boton botonAutomatico =
{
  35,
  60,
  250,
  45,
  "MODO AUTOMATICO"
};

Boton botonManual =
{
  35,
  120,
  250,
  45,
  "MODO MANUAL"
};

Boton botonDatos =
{
  35,
  180,
  250,
  45,
  "MOSTRAR DATOS"
};

// -------- MODO MANUAL --------
Boton botonBomba =
{
  35,
  60,
  250,
  45,
  "BOMBA OFF"
};

Boton botonPeltier =
{
  35,
  120,
  250,
  45,
  "PELTIER OFF"
};

// -------- REGRESAR --------
Boton botonRegresar =
{
  20,
  190,
  120,
  35,
  "REGRESAR"
};

// -------- OK --------
Boton botonOK =
{
  180,
  190,
  120,
  35,
  "OK"
};

// ===================== RECONFIGURAR SPI TFT =====================

void reiniciarSPI_TFT()
{
  SPI.begin(
    PIN_SPI_SCK,
    PIN_SPI_MISO,
    PIN_SPI_MOSI
  );
}

// ===================== DIBUJAR BOTONES =====================
void dibujarBoton(
  Boton boton,
  uint16_t color
)
{
  tft.fillRoundRect(
    boton.x,
    boton.y,
    boton.w,
    boton.h,
    8,
    color
  );

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setTextSize(2);
  int16_t x1;
  int16_t y1;
  uint16_t ancho;
  uint16_t alto;
  tft.getTextBounds(
    boton.texto,
    0,
    0,
    &x1,
    &y1,
    &ancho,
    &alto
  );
  tft.setCursor(
    boton.x + (boton.w-ancho)/2,
    boton.y + (boton.h-alto)/2
  );
  tft.print(
    boton.texto
  );
}

void convertirTouch(
  TS_Point punto,
  int &x,
  int &y
)
{
  x = map(
    punto.x,
    3800,
    200,
    0,
    320
  );
  y = map(
    punto.y,
    200,
    3800,
    0,
    240
  );
}

// ===================== DETECTAR BOTON =====================
bool tocarBoton(
  TS_Point punto,
  Boton boton
)
{
  int x;
  int y;
  convertirTouch(
    punto,
    x,
    y
  );
  Serial.print("Touch convertido X:");
  Serial.print(x);
  Serial.print(" Y:");
  Serial.println(y);
  if(
    x > boton.x &&
    x < boton.x + boton.w &&
    y > boton.y &&
    y < boton.y + boton.h
  )
  {
    return true;
  }
  return false;
}


// ===================== RETROILUMINACION =====================
void establecerBrillo(uint8_t brillo)
{
  // GPIO 32 se conserva. PWM modifica solo la luz, no el contenido de la TFT.
  ledcWrite(BACKLIGHT_CHANNEL, brillo);
  pantallaIluminada = brillo > 0;
}

void despertarPantalla()
{
  ultimaInteraccionPantalla = millis();
  establecerBrillo(BACKLIGHT_FULL);
}


// ===================== PANTALLAS =====================
// ===================== MENU PRINCIPAL =====================
void drawMenuScreen()
{
  if(xSemaphoreTake(spiMutex, portMAX_DELAY))
  {
    reiniciarSPI_TFT();
    tft.fillScreen(
      COLOR_FONDO
    );
    tft.setTextSize(2);
    tft.setCursor(
      35,
      20
    );
    tft.print(
      "ESTADO DEL SISTEMA"
    );
    dibujarBoton(
      botonAutomatico,
      COLOR_BOTON
    );
    dibujarBoton(
      botonManual,
      COLOR_BOTON
    );
    dibujarBoton(
      botonDatos,
      COLOR_BOTON
    );
    xSemaphoreGive(spiMutex);
  }
}

// ===================== MODO AUTOMATICO =====================
void drawAutomaticScreen()
{
  if(xSemaphoreTake(spiMutex, portMAX_DELAY))
  {
    reiniciarSPI_TFT();
    tft.fillScreen(COLOR_FONDO);
    tft.setTextColor(
      ILI9341_GREEN
    );
    tft.setTextSize(2);
    tft.setCursor(
      55,
      20
    );
    tft.print(
      "MODO AUTOMATICO"
    );
    tft.setTextColor(
      COLOR_TEXTO
    );
    tft.setCursor(30,60);
    tft.print("Control sensores");
    tft.setCursor(30,95);
    tft.print("Temperatura >24 C");
    tft.setCursor(30,125);
    tft.print("pH <6.5");
    tft.setCursor(30,155);
    tft.print("Bomba: ");
    tft.print(pumpState ? "ON":"OFF");
    tft.setCursor(150,155);
    tft.print("Peltier: ");
    tft.print(peltierState ? "ON":"OFF");
    dibujarBoton(
      botonRegresar,
      COLOR_BOTON_ALT
    );
    dibujarBoton(
      botonOK,
      COLOR_BOTON_ALT
    );
    xSemaphoreGive(spiMutex);
  }
}

// ===================== MODO MANUAL =====================
void drawManualScreen()
{
  if(xSemaphoreTake(spiMutex, portMAX_DELAY))
  {
    reiniciarSPI_TFT();

    // ================= FONDO =================
    tft.fillScreen(
      COLOR_FONDO
    );

    // ================= TITULO =================
    tft.setTextColor(
      COLOR_BOTON_ALT
    );
    tft.setTextSize(2);
    tft.setCursor(
      75,
      15
    );
    tft.print(
      "MODO MANUAL"
    );

    // ================= BOTON BOMBA =================
    botonBomba.texto =
      bombaEstado ?
      "BOMBA ON":
      "BOMBA OFF";
    dibujarBoton(
      botonBomba,
      bombaEstado ?
      ILI9341_GREEN :
      ILI9341_RED
    );

    // ================= BOTON PELTIER =================
    botonPeltier.texto =
      peltierEstado ?
      "PELTIER ON":
      "PELTIER OFF";
    dibujarBoton(
      botonPeltier,
      peltierEstado ?
      ILI9341_GREEN :
      ILI9341_RED
    );

    // ================= INFORMACION =================
    tft.setTextColor(
      COLOR_TEXTO
    );
    tft.setTextSize(2);
    tft.setCursor(
      35,
      175
    );
    tft.print(
      "Control manual"
    );

    // ================= BOTONES INFERIORES =================
    dibujarBoton(
      botonRegresar,
      COLOR_BOTON_ALT
    );
    dibujarBoton(
      botonOK,
      COLOR_BOTON
    );
    xSemaphoreGive(
      spiMutex
    );
  }
}

// ===================== MOSTRAR DATOS =====================
void drawDataScreen()
{

  if(xSemaphoreTake(spiMutex, portMAX_DELAY))
  {

    reiniciarSPI_TFT();


    // ================= FONDO =================

    tft.fillScreen(
      COLOR_FONDO
    );


    // ================= ESTADOS ACTUALES =================

    bool bombaMostrar = pumpState;
    bool peltierMostrar = peltierState;



    // ================= TITULO =================

    tft.setTextColor(
      COLOR_BOTON_ALT
    );

    tft.setTextSize(2);

    tft.setCursor(
      65,
      12
    );

    tft.print(
      "DATOS SISTEMA"
    );



    // ================= MODO =================

    tft.setTextSize(1);

    tft.setCursor(
      105,
      32
    );


    tft.setTextColor(
      COLOR_TEXTO
    );


    if(modoManualActivo)
    {

      tft.print(
        "MODO MANUAL"
      );

    }
    else
    {

      tft.print(
        "MODO AUTOMATICO"
      );

    }



    // ================= TEMPERATURA =================


    tft.drawRoundRect(
      15,
      50,
      290,
      40,
      8,
      ILI9341_RED
    );


    tft.setTextSize(2);

    tft.setTextColor(
      COLOR_TEXTO
    );


    tft.setCursor(
      30,
      60
    );

    tft.print(
      "TEMP:"
    );


    tft.setCursor(
      150,
      60
    );

    tft.print(
      temperatura,
      1
    );

    tft.print(
      " C"
    );



    // ================= PH =================


    tft.drawRoundRect(
      15,
      100,
      290,
      40,
      8,
      ILI9341_CYAN
    );


    tft.setCursor(
      30,
      110
    );


    tft.print(
      "pH:"
    );


    tft.setCursor(
      150,
      110
    );


    tft.print(
      phValue,
      2
    );



    // ================= BOMBA =================


    tft.setCursor(
      30,
      155
    );


    tft.setTextColor(
      COLOR_TEXTO
    );


    tft.print(
      "BOMBA:"
    );


    if(bombaMostrar)
    {

      tft.setTextColor(
        ILI9341_GREEN
      );

      tft.print(
        " ON"
      );

    }
    else
    {

      tft.setTextColor(
        ILI9341_RED
      );

      tft.print(
        " OFF"
      );

    }



    // ================= PELTIER =================


    tft.setCursor(
      170,
      155
    );


    tft.setTextColor(
      COLOR_TEXTO
    );


    tft.print(
      "PELTIER:"
    );


    if(peltierMostrar)
    {

      tft.setTextColor(
        ILI9341_GREEN
      );

      tft.print(
        " ON"
      );

    }
    else
    {

      tft.setTextColor(
        ILI9341_RED
      );

      tft.print(
        " OFF"
      );

    }



    // ================= BOTON REGRESAR =================


    dibujarBoton(
      botonRegresar,
      COLOR_BOTON_ALT
    );


    xSemaphoreGive(
      spiMutex
    );

  }

}

// ===================== TOUCH =====================
void taskTouchFunc(void *pvParameters)
{

  while(true)
  {

    bool tocado = false;

    TS_Point punto;



    // ================= LEER TOUCH =================

    if(xSemaphoreTake(spiMutex, portMAX_DELAY))
    {

      tocado = ts.touched();


      if(tocado)
      {

        punto = ts.getPoint();


        // devolver SPI al TFT
        reiniciarSPI_TFT();

      }


      xSemaphoreGive(spiMutex);

    }




    if(tocado)
    {

      // El primer toque con la luz apagada solo despierta la pantalla.
      // Esto evita activar accidentalmente una bomba o un boton del menu.
      if(!pantallaIluminada)
      {
        despertarPantalla();
        while(ts.touched())
        {
          vTaskDelay(pdMS_TO_TICKS(20));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }

      despertarPantalla();


      Serial.print("X:");
      Serial.print(punto.x);

      Serial.print(" Y:");
      Serial.println(punto.y);



      // =====================================================
      // MENU PRINCIPAL
      // =====================================================

      if(pantallaActual == MENU)
      {


        // ---------- AUTOMATICO ----------

        if(tocarBoton(punto, botonAutomatico))
        {

          Serial.println("ENTRANDO AUTOMATICO");


          modoActual = AUTOMATICO;

          modoManualActivo = false;

          // El sensor de pH esta averiado: al salir del modo manual la bomba
          // queda apagada y nunca se gobierna con una lectura no confiable.
          bombaEstado = false;
          pumpState = false;
          digitalWrite(PIN_AIR_PUMP, LOW);


          pantallaActual = PANTALLA_AUTOMATICO;

          pantallaNeedsUpdate = true;


          actualizarDatosWeb(
            temperatura,
            phValue,
            pumpState,
            peltierState,
            modoManualActivo
          );

        }



        // ---------- MANUAL ----------

        else if(tocarBoton(punto, botonManual))
        {

          Serial.println("ENTRANDO MANUAL");


          bombaEstado = pumpState;

          peltierEstado = peltierState;


          modoActual = MANUAL;

          modoManualActivo = true;


          pantallaActual = PANTALLA_MANUAL;


          pantallaNeedsUpdate = true;



          actualizarDatosWeb(
            temperatura,
            phValue,
            pumpState,
            peltierState,
            modoManualActivo
          );

        }




        // ---------- DATOS ----------

        else if(tocarBoton(punto, botonDatos))
        {

          Serial.println("ENTRANDO DATOS");


          pantallaActual = PANTALLA_DATOS;


          pantallaNeedsUpdate = true;

        }

      }





      // =====================================================
      // PANTALLA AUTOMATICO
      // =====================================================

      else if(pantallaActual == PANTALLA_AUTOMATICO)
      {


        if(tocarBoton(punto, botonOK))
        {

          mostrarDatosManual = false;


          pantallaActual = PANTALLA_DATOS;


          pantallaNeedsUpdate = true;

        }



        else if(tocarBoton(punto, botonRegresar))
        {

          pantallaActual = MENU;


          pantallaNeedsUpdate = true;

        }

      }






      // =====================================================
      // PANTALLA MANUAL
      // =====================================================

      else if(pantallaActual == PANTALLA_MANUAL)
      {


        // =================================================
        // BOMBA
        // =================================================

        if(tocarBoton(punto, botonBomba))
        {


          bombaEstado = !bombaEstado;



          pumpState = bombaEstado;



          digitalWrite(
            PIN_AIR_PUMP,
            bombaEstado
          );



          Serial.print(
            "BOMBA TFT: "
          );


          Serial.println(
            bombaEstado ? "ON":"OFF"
          );



          actualizarDatosWeb(
            temperatura,
            phValue,
            pumpState,
            peltierState,
            modoManualActivo
          );



          pantallaNeedsUpdate = true;


        }



        // =================================================
        // PELTIER
        // =================================================

        else if(tocarBoton(punto, botonPeltier))
        {


          peltierEstado = !peltierEstado;



          peltierState = peltierEstado;



          digitalWrite(
            PIN_PELTIER,
            peltierEstado
          );



          Serial.print(
            "PELTIER TFT: "
          );


          Serial.println(
            peltierEstado ? "ON":"OFF"
          );



          actualizarDatosWeb(
            temperatura,
            phValue,
            pumpState,
            peltierState,
            modoManualActivo
          );



          pantallaNeedsUpdate = true;


        }




        // =================================================
        // OK
        // =================================================

        else if(tocarBoton(punto, botonOK))
        {


          mostrarDatosManual = true;


          pantallaActual = PANTALLA_DATOS;


          pantallaNeedsUpdate = true;


        }




        // =================================================
        // REGRESAR
        // =================================================

        else if(tocarBoton(punto, botonRegresar))
        {


          pantallaActual = MENU;


          pantallaNeedsUpdate = true;


        }


      }






      // =====================================================
      // PANTALLA DATOS
      // =====================================================

      else if(pantallaActual == PANTALLA_DATOS)
      {


        if(tocarBoton(punto, botonRegresar))
        {


          pantallaActual = MENU;


          pantallaNeedsUpdate = true;


        }

      }






      // ESPERAR SOLTAR TOUCH

      while(ts.touched())
      {

        vTaskDelay(
          pdMS_TO_TICKS(20)
        );

      }


    }



    vTaskDelay(
      pdMS_TO_TICKS(50)
    );


  }

}

// ===================== SENSORES =====================
float readTemperatureC()
{
  tempSensor.requestTemperatures();
  float value = tempSensor.getTempCByIndex(0);
  if (value == DEVICE_DISCONNECTED_C)
    return NAN;
  return value;
}

float readPH()
{
  float sum = 0;
  for (int i = 0; i < 10; i++)
  {
    sum += analogRead(PIN_PH_SENSOR);
  }

  // Formula aproximada sin calibracion promedio de 10 lecturas para estabilidad
  float raw = sum / 10.0;
  float voltage = (raw * 3.3f) / 4095.0f;
  float ph = 5.0 + ((2.5 - voltage) / 0.18);
  return ph;
}

// ===================== TAREA SENSORES =====================
void taskSensoresFunc(void *pvParameters)
{
  while (true)
  {

    // ===================== LEER SENSORES =====================

    temperatura = readTemperatureC();

    phValue = readPH();

    // La lectura local sigue siendo cada segundo. La telemetria decide cuándo
    // publicar para no saturar memoria, red ni la plataforma en la nube.
    registrarMuestraTelemetria(
      temperatura,
      phValue,
      pumpState,
      peltierState,
      modoManualActivo
    );

    // ===================== ACTUALIZAR WEB =====================
    // Enviar siempre el estado real del sistema
    // Si está en manual usa las variables manuales
    // Si está en automático usa las variables automáticas

    if(modoManualActivo)
    {

      actualizarDatosWeb(
        temperatura,
        phValue,
        bombaEstado,
        peltierEstado,
        true
      );

    }
    else
    {

      actualizarDatosWeb(
        temperatura,
        phValue,
        pumpState,
        peltierState,
        false
      );

    }



    // ===================== ESPERA =====================

    vTaskDelay(
      pdMS_TO_TICKS(1000)
    );

  }
}

// ===================== TAREA CONTROL =====================
void taskControlFunc(void *pvParameters)
{
  while (true)
  {

    // ================= MODO AUTOMATICO ===================

    if (modoManualActivo == false)
    {

      // -------- CONTROL TEMPERATURA --------

      if (!isnan(temperatura))
      {

        if (temperatura > 24.0f)
        {

          digitalWrite(
            PIN_PELTIER,
            HIGH
          );

          peltierState = true;

        }


        else if (temperatura < 22.0f)
        {

          digitalWrite(
            PIN_PELTIER,
            LOW
          );

          peltierState = false;

        }

      }



      // -------- CONTROL pH --------
      // La lectura se conserva para demostracion e historial, pero no acciona
      // la bomba porque el sensor reporta valores no confiables. La bomba solo
      // responde a los botones manuales de la TFT o del telefono.
      digitalWrite(PIN_AIR_PUMP, LOW);
      pumpState = false;


    }



    // ==================== MODO MANUAL ====================

    else
    {

      digitalWrite(
        PIN_PELTIER,
        peltierEstado
      );


      digitalWrite(
        PIN_AIR_PUMP,
        bombaEstado
      );



      // Sincronizar estados generales

      peltierState = peltierEstado;

      pumpState = bombaEstado;


    }



    vTaskDelay(
      pdMS_TO_TICKS(500)
    );

  }

}

// ===================== TAREA DISPLAY =====================
void taskDisplayFunc(void *pvParameters)
{
  unsigned long ultimaActualizacionDatos = 0;
  while(true)
  {
    // ACTUALIZAR CUANDO CAMBIA DE PANTALLA
    if(pantallaNeedsUpdate)
    {
      Serial.print("DIBUJANDO PANTALLA NUMERO: ");
      Serial.println(pantallaActual);
      switch(pantallaActual)
      {
        case MENU:
          Serial.println("MENU");
          drawMenuScreen();
          break;
        case PANTALLA_AUTOMATICO:
          Serial.println("AUTOMATICO");
          drawAutomaticScreen();
          break;
        case PANTALLA_MANUAL:
          Serial.println("MANUAL");
          drawManualScreen();
          break;
        case PANTALLA_DATOS:
          Serial.println("DATOS");
          drawDataScreen();
          break;
      }
      pantallaNeedsUpdate = false;

      // reiniciar contador cuando cambia pantalla
      ultimaActualizacionDatos = millis();
    }

    // ACTUALIZACION PERIODICA DE DATOS
    if(
      pantallaActual == PANTALLA_DATOS &&
      millis() - ultimaActualizacionDatos >= 1000
    )
    {
      Serial.println("ACTUALIZANDO DATOS TFT");
      drawDataScreen();
      ultimaActualizacionDatos = millis();
    }

    // La pantalla se oscurece por inactividad; sensores y actuadores continúan.
    const unsigned long inactividad = millis() - ultimaInteraccionPantalla;
    if(inactividad >= BACKLIGHT_OFF_MS && pantallaIluminada)
    {
      establecerBrillo(0);
    }
    else if(inactividad >= BACKLIGHT_DIM_MS && pantallaIluminada)
    {
      establecerBrillo(BACKLIGHT_DIM);
    }

    vTaskDelay(
      pdMS_TO_TICKS(100)
    );
  }
}

// ===================== TAREA WIFI =====================
/*
void taskWiFiFunc(void *pvParameters)
{
  intentoconexion(
    "Proyecto Camarones",
    "123456789"
  );

  iniciarFirebase();
  iniciarWebMonitor();

  server.begin();

  Serial.println(
    "🌐 Servidor web listo"
  );


  while(true)
  {
    loopAP();

    actualizarWebSocket();


    vTaskDelay(
      pdMS_TO_TICKS(10)
    );
  }
}
*/

void taskWiFiFunc(void *pvParameters)
{
  intentoconexion(
    "Proyecto Camarones",
    "123456789"
  );


  // El servidor local tambien inicia en modo Access Point. Nunca esperamos
  // indefinidamente Internet porque el control local es prioritario.
  iniciarTelemetria();
  

  iniciarWebMonitor();

  server.begin();

  Serial.println(
    "🌐 Servidor web listo"
  );


  while(true)
  {
    loopAP();

    server.handleClient();

    actualizarWebSocket();

    actualizarTelemetria();


    vTaskDelay(
      pdMS_TO_TICKS(10)
    );
  }
}

// ===================== DEBUG TFT =====================
void mostrarDebug(String texto, int y)
{
  if (xSemaphoreTake(spiMutex, portMAX_DELAY))
  {
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);
    tft.setCursor(10, y);
    tft.print(texto);
    xSemaphoreGive(spiMutex);
  }
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(500);
  EEPROM.begin(512);

  // ===================== ACTUADORES =====================
  pinMode(PIN_PELTIER, OUTPUT);
  pinMode(PIN_AIR_PUMP, OUTPUT);
  digitalWrite(PIN_PELTIER, LOW);
  digitalWrite(PIN_AIR_PUMP, LOW);

  // ===================== TFT =====================
  // PWM sobre el mismo GPIO 32 ya soldado en la PCB.
  ledcSetup(BACKLIGHT_CHANNEL, 5000, 8);
  ledcAttachPin(PIN_TFT_LED, BACKLIGHT_CHANNEL);
  ultimaInteraccionPantalla = millis();
  establecerBrillo(BACKLIGHT_FULL);

  // ===================== SPI =====================
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

  // ===================== MUTEX SPI =====================
  spiMutex = xSemaphoreCreateMutex();
  if (spiMutex == NULL)
  {
    Serial.println("ERROR creando mutex");
  }
  else
  {
    Serial.println("Mutex OK");
  }

  // ===================== TFT =====================
  tft.begin();
  tft.setRotation(1);

  // ===================== TOUCH =====================
  ts.begin();
  ts.setRotation(1);

  // ===================== SENSORES =====================
  tempSensor.begin();

  // Primera pantalla
  pantallaActual = MENU;
  drawMenuScreen();

  // ===================== CREAR TAREAS =====================
  xTaskCreatePinnedToCore(
    taskSensoresFunc,
    "Sensores",
    8192,
    NULL,
    1,
    &taskSensores,
    1
  );

  xTaskCreatePinnedToCore(
    taskControlFunc,
    "Control",
    4096,
    NULL,
    1,
    &taskControl,
    1
  );

  xTaskCreatePinnedToCore(
    taskDisplayFunc,
    "Display",
    4096,
    NULL,
    1,
    &taskDisplay,
    1
  );

  xTaskCreatePinnedToCore(
    taskWiFiFunc,
    "WiFi",
    8192,
    NULL,
    1,
    &taskWiFi,
    0
  );

  xTaskCreatePinnedToCore(
    taskTouchFunc,
    "Touch",
    4096,
    NULL,
    1,
    &taskTouch,
    1
  );
  Serial.println("Sistema con FreeRTOS iniciado");
}

// ===================== LOOP =====================
void loop()
{
}


/*
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS    5
#define TFT_DC    16
#define TFT_RST   17
#define TFT_LED   32

#define TOUCH_CS  25

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);

uint8_t estado = 0;
bool ultimoEstado = false;

void cambiarPantalla() {

  switch (estado) {

    case 0:
      tft.fillScreen(ILI9341_RED);
      tft.setCursor(80,110);
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(3);
      tft.print("ROJO");
      break;

    case 1:
      tft.fillScreen(ILI9341_GREEN);
      tft.setCursor(70,110);
      tft.setTextColor(ILI9341_BLACK);
      tft.setTextSize(3);
      tft.print("VERDE");
      break;

    case 2:
      tft.fillScreen(ILI9341_BLUE);
      tft.setCursor(85,110);
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(3);
      tft.print("AZUL");
      break;

    case 3:
      tft.fillScreen(ILI9341_BLACK);
      tft.setCursor(65,110);
      tft.setTextColor(ILI9341_WHITE);
      tft.setTextSize(3);
      tft.print("NEGRO");
      break;
  }
}

void setup() {

  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, HIGH);

  SPI.begin(18,19,23);

  tft.begin();
  tft.setRotation(1);

  ts.begin();
  ts.setRotation(1);

  cambiarPantalla();
}

void loop() {

  bool tocando = ts.touched();

  // Detecta un nuevo toque
  if (tocando && !ultimoEstado) {

    estado++;
    if (estado > 3) estado = 0;

    cambiarPantalla();
  }

  ultimoEstado = tocando;

  delay(20);
}*/
/*
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

//==================== TFT ====================//
constexpr uint8_t PIN_TFT_CS  = 5;
constexpr uint8_t PIN_TFT_DC  = 16;
constexpr uint8_t PIN_TFT_RST = 17;

//==================== SPI ====================//
constexpr uint8_t PIN_SPI_SCK  = 18;
constexpr uint8_t PIN_SPI_MISO = 19;
constexpr uint8_t PIN_SPI_MOSI = 23;

//==================== Sensor pH ====================//
constexpr uint8_t PIN_PH_SENSOR = 34;

//==================== TFT ====================//
Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

void setup()
{
    Serial.begin(115200);

    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(ILI9341_BLACK);

    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(2);

    tft.setCursor(40,15);
    tft.println("PRUEBA SENSOR pH");

    analogReadResolution(12);                 // 0-4095
    analogSetPinAttenuation(PIN_PH_SENSOR, ADC_11db);

    delay(1000);
}

void loop()
{
    // Promedio de 20 lecturas
    long suma = 0;

    for(int i=0;i<20;i++)
    {
        suma += analogRead(PIN_PH_SENSOR);
        delay(10);
    }

    float adc = suma / 20.0;

    // Voltaje del ESP32
    float voltaje = adc * 3.3 / 4095.0;

    // Conversión aproximada para PH-4502C
    // Luego se calibra correctamente
    float ph = 7 + ((2.50 - voltaje) / 0.18);

    tft.fillRect(0,50,320,190,ILI9341_BLACK);

    tft.setCursor(20,60);
    tft.print("ADC:");

    tft.setCursor(150,60);
    tft.print(adc,0);

    tft.setCursor(20,100);
    tft.print("Voltaje:");

    tft.setCursor(150,100);
    tft.print(voltaje,3);
    tft.print(" V");

    tft.setCursor(20,140);
    tft.print("pH:");

    tft.setCursor(150,140);
    tft.print(ph,2);

    Serial.print("ADC: ");
    Serial.print(adc);

    Serial.print("  Voltaje: ");
    Serial.print(voltaje,3);

    Serial.print("  pH: ");
    Serial.println(ph,2);

    delay(500);
}*/
