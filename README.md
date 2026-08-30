# SISTEMA AUTOMATIZADO DE CONTROL BIOAMBIENTAL Y CONECTIVIDAD INTELIGENTE PARA EL TRANSPORTE DE LARVAS EN TANQUES

Prototipo embebido basado en **ESP32 y FreeRTOS** para monitorear las condiciones
del agua durante el transporte de larvas de camarón. El sistema integra medición
local, control de actuadores, pantalla táctil, una interfaz web para teléfono o
computadora y telemetría remota mediante Ubidots.

![Prototipo final](docs/imagenes/prototipo-final.jpeg)

## Objetivo

Reducir el riesgo de mortalidad de las larvas durante el transporte mediante la
supervisión continua de temperatura y pH referencial, la actuación local sobre
una celda Peltier y una bomba, y el registro remoto de variables y eventos del
viaje.

## Funciones principales

- Medición de temperatura mediante un sensor DS18B20.
- Lectura referencial de pH a través de una entrada analógica.
- Control automático de la celda Peltier según la temperatura.
- Control manual de la bomba y la celda Peltier desde la pantalla o la interfaz web.
- Pantalla táctil TFT ILI9341 para operación local.
- Portal de configuración Wi-Fi con almacenamiento mediante `Preferences`.
- Interfaz web local accesible desde teléfono o computadora.
- Registro de origen, destino, tanque, inicio, finalización y duración del viaje.
- Telemetría MQTT hacia Ubidots con una cola local de 20 muestras.
- Atenuación y apagado automático de la iluminación de la pantalla.

## Arquitectura general

```text
Sensores de temperatura y pH
              |
              v
        ESP32 + FreeRTOS
        /      |       \
       v       v        v
 Pantalla   Actuadores  Wi-Fi/MQTT
   TFT      Peltier y   |       |
            bomba      Web    Ubidots
```

## Evidencias del prototipo

### Interfaz local en pantalla TFT

| Menú principal | Datos del sistema | Control automático |
|---|---|---|
| ![Menú principal](docs/imagenes/pantalla-monitoreo-3.jpeg) | ![Datos del sistema](docs/imagenes/pantalla-monitoreo-2.jpeg) | ![Modo automático](docs/imagenes/pantalla-monitoreo-1.jpeg) |

### Interfaz web y trazabilidad

| Control desde el celular | Viaje en curso |
|---|---|
| ![Control web](docs/imagenes/control-web-celular.jpeg) | ![Trazabilidad](docs/imagenes/trazabilidad-web.jpeg) |

### Plataforma Ubidots

| Estado general | Gráficas del viaje |
|---|---|
| ![Dashboard de Ubidots](docs/imagenes/dashboard-ubidots.jpeg) | ![Gráficas de Ubidots](docs/imagenes/dashboard-ubidots-graficas.jpeg) |

![Variables registradas en Ubidots](docs/imagenes/variables-ubidots.jpeg)

## Organización del proyecto

```text
.
|-- data/                 # Interfaz web almacenada en el ESP32
|-- docs/imagenes/        # Fotografías y capturas del prototipo
|-- src/                  # Firmware del ESP32
|-- platformio.ini        # Configuración y dependencias
`-- README.md             # Documentación principal
```

## Conexiones principales

| Función | GPIO |
|---|---:|
| TFT CS | 5 |
| TFT DC | 16 |
| TFT RESET | 17 |
| Iluminación TFT | 32 |
| SPI SCK | 18 |
| SPI MISO | 19 |
| SPI MOSI | 23 |
| Touch CS | 25 |
| Sensor DS18B20 | 4 |
| Sensor de pH | 34 |
| Celda Peltier | 27 |
| Bomba | 26 |

## Requisitos

- Visual Studio Code con PlatformIO, o PlatformIO Core.
- Tarjeta ESP32 Dev Module.
- Librerías declaradas en `platformio.ini`.

## Configuración segura de Ubidots

Antes de compilar, editar en `src/telemetryManager.cpp`:

```cpp
constexpr char UBIDOTS_TOKEN[] = "CAMBIAR_TOKEN_UBIDOTS";
constexpr char DEVICE_LABEL[] = "tanque-01";
```

El token real no debe guardarse en un repositorio público. El prototipo utiliza
MQTT por el puerto 1883 para reducir el consumo de RAM y memoria Flash. Para una
implementación comercial se recomienda MQTT/TLS o un gateway seguro.

Variables publicadas: `temperatura`, `ph`, `bomba`, `peltier`, `modo_manual`,
`heap_libre`, `viaje_activo`, `evento_viaje`, `inicio_viaje`, `fin_viaje` y
`duracion_viaje_minutos`.

## Compilación y carga

Desde una terminal ubicada en esta carpeta:

```powershell
platformio run
platformio run --target uploadfs
platformio run --target upload
```

`uploadfs` carga la interfaz web y `upload` carga el firmware. Se debe verificar
el puerto configurado en `platformio.ini` antes de programar la tarjeta.

## Consideraciones de seguridad

- Regenerar el token de Ubidots antes de utilizar este código.
- Cambiar la contraseña predeterminada del punto de acceso para una instalación real.
- No publicar credenciales de redes Wi-Fi.
- Utilizar una red de confianza mientras MQTT funcione sin TLS.

## Autores

- **Edison Paulino Yagual Calapiña** - desarrollo, integración y documentación.
- **Anthony Santacruz** - colaboración en el desarrollo práctico del prototipo.

Este repositorio corresponde a la entrega individual de Edison Yagual. La fase
práctica del prototipo se realizó de manera colaborativa.
