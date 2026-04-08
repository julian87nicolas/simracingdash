# Simracing Dash — ESP8266 F1 2025 Telemetry Dashboard

Dashboard en tiempo real para F1 2025 (PS5) usando un ESP8266 (NodeMCU v2) y una pantalla SPI 480×320 (ILI9488 / ST7796).

El firmware escucha paquetes UDP en el puerto 20777, parsea telemetría oficial F1 (compatible con F1 22 a F1 25) y renderiza 6 pantallas estilo F1 en tiempo real usando `TFT_eSPI`.

**Estructura del proyecto**
- `platformio.ini` — configuración de build (env `nodemcuv2` y `native` para pruebas)
- `include/` — cabeceras públicas (`telemetry.h`, `state.h`, ...)
- `src/` — código fuente (listener UDP, parser, state, dashboard manager)
- `src/dashboards/` — renderers por pantalla (dash_main, dash_setup, dash_pits, dash_tyres, dash_temps, dash_ers)
- `tools/` — simulador interactivo Python para pruebas sin el juego
- `test/` — pruebas PlatformIO (Unity) y `test/host/` pruebas host-side (g++)
- `scripts/` — scripts de conveniencia (`clean_build.sh`)
- `lib/` — configuración TFT_eSPI y stubs para pruebas locales

---

Hardware
--------

**Requisitos**
- ESP8266 (NodeMCU v2)
- Pantalla SPI 480×320 (ILI9488 o ST7796, sin touch)

**Conexiones (NodeMCU v2 → ILI9488)**

| Display    | NodeMCU | GPIO   | Notas              |
|------------|---------|--------|--------------------|
| LED (BL)   | 3V3     | —      | Backlight directo  |
| SCK (CLK)  | D5      | GPIO14 | Hardware SPI HSCLK |
| SDI (MOSI) | D7      | GPIO13 | Hardware SPI HMOSI |
| DC (RS)    | D4      | GPIO2  | Data/Command       |
| RST        | D6      | GPIO12 | Reset              |
| CS         | D8      | GPIO15 | Chip Select        |
| GND        | GND     | —      | Tierra             |
| VCC        | 3V3     | —      | Alimentación 3.3V  |

> D5/D7 son los pines hardware SPI del ESP8266 (máxima velocidad). D8 tiene pull-down interno, ideal para CS.

**Drivers soportados:** ILI9488 o ST7796. Seleccionar en `lib/TFT_eSPI/User_Setup.h`.

---

Instalación
-----------

1. Instalar PlatformIO:

```bash
# usando pipx (recomendado)
brew install pipx && pipx ensurepath
pipx install platformio

# o con pip
pip install platformio
```

2. Compilar y subir:

```bash
pio run -e nodemcuv2 --target upload

# Monitor serial
pio device monitor -b 115200
```

**Configuración WiFi:** el firmware arranca como AP `F1Dash-Setup` (IP 192.168.4.1) si no tiene credenciales guardadas. Conectate al AP, entrá a la IP en el navegador y configurá SSID/contraseña. Las credenciales se guardan en EEPROM.

> En macOS los puertos serie suelen verse como `/dev/tty.SLAB_USBtoUART` o `/dev/tty.usbserial-XXXX`. Si PlatformIO no detecta el puerto: `pio device list`.

---

Pantallas del dashboard
-----------------------

El firmware cambia de pantalla automáticamente según el `mfdPanelIndex` del juego (el MFD del HUD en F1 2025). En modo carrera:

| MFD | Pantalla   | Contenido |
|-----|------------|-----------|
| 255 | **MAIN**   | Velocidad, RPM (barra LED), marcha, acelerador/freno, DRS, posición, tiempo de vuelta, fuel, brake bias, ERS |
| 0   | **SETUP**  | Brake bias (slider), diferencial ON/OFF throttle, modo ERS, barra energía ERS |
| 1   | **PITS**   | Estado pit (track/pitting/in pit), posición, compuesto, edad neumáticos, fuel restante |
| 2   | **TYRES**  | Desgaste por rueda (FL/FR/RL/RR) con código de color, compuesto, edad |
| 3   | **TEMPS**  | Temperaturas superficie/interior neumáticos, frenos y motor por esquina |
| 4   | **ENGINE** | Desgaste de componentes: MGU-H, ES, CE, ICE, MGU-K, TC, caja de cambios |

En sesiones de práctica/clasificación la pantalla PITS se omite y los índices se recorren diferente.

Pruebas
-------

```bash
# Runner unificado (PlatformIO native o fallback g++)
./test/run_local.sh
```

CI: GitHub Actions en `.github/workflows/ci.yml`.

---

Características
---------------

- Parser de telemetría F1 oficial compatible con F1 22, 23, 24 y 25 (auto-detecta versión por header)
- 6 tipos de paquete: Session, CarTelemetry, CarStatus, LapData, CarSetup, CarDamage
- StateManager con `current`/`previous` frames y helpers (`hasMFDChanged()`, `isInPit()`, `hasERSChanged()`)
- 6 pantallas con renderers parciales (solo redibuja lo que cambia)
- Barra RPM estilo LEDs con zonas de color
- Portal WiFi de configuración automática (AP `F1Dash-Setup`)
- Simulador Python interactivo para pruebas sin el juego

---

Depuración
----------

- **Pantalla en negro:** verificar driver y pines en `User_Setup.h`, alimentación y pin RST.
- **Sin datos UDP:** verificar que el ESP esté en la misma red que la consola/PC y que el juego emita telemetría UDP al puerto 20777.
- **Serial debug:** `pio device monitor -b 115200` — imprime estado WiFi, recepción UDP y parsing.
- **Recompilación limpia:** `./scripts/clean_build.sh`

Emulación local (sin placa ni pantalla)
-------------------------------------

### Simulador interactivo (Python)

`tools/test_dashboard.py` es un simulador interactivo que envía tramas F1 2025 reales al ESP8266 para probar todas las pantallas del dashboard sin necesidad del juego.

**Requisitos:** Python 3 (sin dependencias externas).

```bash
# Enviar a la IP del NodeMCU
python3 tools/test_dashboard.py 192.168.1.XXX 20777

# O broadcast en la red local (default)
python3 tools/test_dashboard.py
```

**Dos modos de operación:**

- **MANUAL** — Controlás cada valor con el teclado.
- **PLAY** (SPACE) — Simula vueltas realistas automáticamente: aceleraciones, frenadas, zonas DRS, consumo de combustible, desgaste de neumáticos, temperaturas que varían coherentemente.

**Controles:**

| Tecla | Acción |
|-------|--------|
| `1-6` | Seleccionar pantalla: 1=MAIN 2=SETUP 3=PITS 4=TYRES 5=TEMPS 6=ENGINE |
| `SPACE` | Alternar entre modo PLAY y MANUAL |
| `↑ / ↓` | RPM ±1000 (solo manual) |
| `g / G` | Marcha arriba / abajo (solo manual) |
| `t / T` | Acelerador ±25% (solo manual) |
| `b / B` | Freno ±25% (solo manual) |
| `d` | Toggle DRS (solo manual) |
| `p` | Ciclar pit status (solo manual) |
| `c` | Ciclar compuesto de neumático (solo manual) |
| `r` | Randomizar todos los valores (solo manual) |
| `q` | Salir |

**Terminal con colores ANSI:** barras de RPM con zonas de color, acelerador/freno, temperaturas y desgaste con código de colores según severidad, indicador de modo PLAY/MANUAL, segmento de pista actual en modo play.

Scripts (conveniencia)
----------------------

- `scripts/clean_build.sh`: limpia directorios de build de PlatformIO (`.pio`, `.pioenvs`, `.cache`) para forzar una recompilación limpia.

```bash
./scripts/clean_build.sh
```

