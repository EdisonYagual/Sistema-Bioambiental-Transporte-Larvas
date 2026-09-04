# Conexiones completas del ESP32

Este documento registra las conexiones utilizadas por el firmware final del
prototipo. Los GPIO se verificaron directamente en `src/main.cpp`.

## Mapa completo de señales

| GPIO | Nombre en el firmware | Componente | Terminal del componente | Tipo de señal | Uso |
|---:|---|---|---|---|---|
| VIN / 5V | No aplica | Regulador LM2596 | Salida de 5 V | Alimentación | Alimentación regulada de la placa ESP32 |
| 3V3 | No aplica | Sensor DS18B20 y lógica compatible | VCC | Alimentación | Salida de 3.3 V de la placa |
| GND | No aplica | Todos los módulos | GND | Referencia común | Retorno y referencia eléctrica común |
| 4 | `PIN_DS18B20` | Sensor de temperatura DS18B20 | DQ / DATA | One-Wire | Lectura de la temperatura del agua |
| 5 | `PIN_TFT_CS` | Pantalla TFT ILI9341 | TFT_CS / CS | Salida digital SPI | Selección de la pantalla |
| 16 | `PIN_TFT_DC` | Pantalla TFT ILI9341 | TFT_DC / DC | Salida digital | Selección de datos o comandos |
| 17 | `PIN_TFT_RST` | Pantalla TFT ILI9341 | TFT_RST / RESET | Salida digital | Reinicio de la pantalla |
| 18 | `PIN_SPI_SCK` | ILI9341 y XPT2046 | SCK y T_CLK | Reloj SPI compartido | Sincronización del bus SPI |
| 19 | `PIN_SPI_MISO` | ILI9341 y XPT2046 | MISO/SDO y T_DO | Entrada SPI | Datos enviados por los periféricos al ESP32 |
| 23 | `PIN_SPI_MOSI` | ILI9341 y XPT2046 | MOSI/SDI y T_DIN | Salida SPI | Datos enviados por el ESP32 a los periféricos |
| 25 | `PIN_TOUCH_CS` | Controlador táctil XPT2046 | T_CS | Salida digital SPI | Selección del controlador táctil |
| 26 | `PIN_AIR_PUMP` | Módulo de relés optoacoplado | IN1, canal de la bomba | Salida digital | Conmutación de la bomba de 12 V |
| 27 | `PIN_PELTIER` | Módulo de relés optoacoplado | IN2, canal Peltier | Salida digital | Conmutación de la celda Peltier y su ventilador |
| 32 | `PIN_TFT_LED` | Pantalla TFT ILI9341 | LED / BL | Salida PWM | Regulación y apagado de la retroiluminación |
| 34 | `PIN_PH_SENSOR` | Módulo de pH PH-4502C | PO / salida analógica | Entrada ADC | Medición referencial de pH |

## Conexiones por componente

### Pantalla TFT ILI9341 y controlador táctil XPT2046

La pantalla y el panel táctil comparten las líneas SPI `SCK`, `MISO` y `MOSI`.
El ESP32 distingue cada dispositivo mediante señales de selección independientes:
GPIO 5 para la pantalla y GPIO 25 para el táctil.

| Terminal del módulo | Conexión al ESP32 |
|---|---:|
| TFT_CS / CS | GPIO 5 |
| TFT_DC / DC | GPIO 16 |
| TFT_RST / RESET | GPIO 17 |
| SCK y T_CLK | GPIO 18 |
| MISO/SDO y T_DO | GPIO 19 |
| MOSI/SDI y T_DIN | GPIO 23 |
| T_CS | GPIO 25 |
| LED / BL | GPIO 32 |
| T_IRQ | No conectado; no se utiliza en el firmware |

### Sensor de temperatura DS18B20

| Terminal | Conexión |
|---|---|
| DQ / DATA | GPIO 4 |
| VCC | 3.3 V |
| GND | GND |

La línea DQ debe disponer de una resistencia *pull-up* de aproximadamente 4.7
kΩ hacia 3.3 V, salvo que el módulo utilizado ya la incorpore.

### Módulo de pH PH-4502C

| Terminal | Conexión |
|---|---|
| PO / salida analógica | GPIO 34 (ADC) |
| VCC | Alimentación correspondiente al módulo |
| GND | GND |

GPIO 34 es únicamente de entrada. La tensión aplicada al ADC del ESP32 no debe
superar 3.3 V; si la salida del módulo puede exceder ese valor, se debe emplear
acondicionamiento o un divisor de tensión apropiado.

### Módulo de relés y cargas de 12 V

| Señal | Conexión |
|---|---:|
| IN1, canal que conmuta la bomba | GPIO 26 |
| IN2, canal que conmuta la celda Peltier y el ventilador | GPIO 27 |
| Alimentación lógica del módulo de relés | 5 V, según el módulo utilizado |
| Bomba | Fuente de 12 V a través de los contactos del relé |
| Celda Peltier y ventilador | Fuente de 12 V a través de los contactos del relé |

Los GPIO solamente gobiernan las entradas del módulo de relés. Las cargas de
12 V no deben conectarse directamente al ESP32.

## Resumen de alimentación

| Elemento | Alimentación documentada |
|---|---|
| ESP32 y lógica | 5 V regulados hacia la entrada de alimentación de la placa |
| DS18B20 | 3.3 V |
| Módulo de relés | 5 V |
| Bomba | 12 V mediante relé |
| Celda Peltier y ventilador | 12 V mediante relé |

La alimentación exacta de la pantalla y del módulo PH-4502C debe respetar las
especificaciones de los módulos físicos instalados. Todas las señales que llegan
al ESP32 deben mantenerse dentro de los niveles lógicos y analógicos permitidos
por la placa.
