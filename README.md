# Simracing Dash — ESP8266 F1 2025 Telemetry Dashboard

Proyecto de ejemplo: un dashboard en tiempo real para F1 2025 (PS5) usando un ESP8266 (NodeMCU v2) y una pantalla SPI 480×320 (ILI9488 / ST7796).

**Resumen:** el firmware escucha paquetes UDP en el puerto 20777 (por defecto usado por los juegos F1), parsea paquetes de telemetría simplificados y renderiza varias pantallas estilo F1 en una pantalla SPI usando `TFT_eSPI`.

**Estado:** implementación básica y modular; pruebas unitarias integradas (PlatformIO / g++ fallback) y CI en GitHub Actions.

**Estructura del proyecto**
- `platformio.ini` — configuración de build (env `nodemcuv2` y `native` para pruebas)
- `include/` — cabeceras públicas (`telemetry.h`, `state.h`, ...)
- `src/` — código fuente (listener UDP, parser, state, dashboards)
- `src/dashboards/` — renderers por pantalla
- `test/` — pruebas PlatformIO (Unity) y `test/host/` pruebas host-side (g++)
- `lib/` — stubs y configuraciones para pruebas locales

**Requisitos de hardware**
- ESP8266 (NodeMCU v2 recomendado)
- Pantalla SPI 480×320 (ILI9488 o ST7796, sin touch)
- Cables y alimentación 5V/3.3V según tu pantalla

**Conexiones recomendadas (NodeMCU v2)**
> Ajusta según tu placa y la configuración en `lib/TFT_eSPI/User_Setup.h`.

- `TFT_MOSI` → D7 (GPIO13)
- `TFT_SCLK` → D5 (GPIO14)
- `TFT_CS`   → D8 (GPIO15)
- `TFT_DC`   → D4 (GPIO2)
- `TFT_RST`  → D6 (GPIO12)
- `TFT_BL`   → (opcional) pin de backlight si tu pantalla lo requiere

Verifica la alimentación: muchas pantallas 480×320 requieren 3.3V señal pero 5V para la retroiluminación o nivel shifters — sigue la documentación de tu módulo.

**Drivers soportados**
- ILI9488 (descomentar `ILI9488_DRIVER` en `lib/TFT_eSPI/User_Setup.h`)
- ST7796 (descomentar `ST7796_DRIVER` en `lib/TFT_eSPI/User_Setup.h`)

Configuración de pines y driver: edita `lib/TFT_eSPI/User_Setup.h` para que coincida con tu hardware.

**Instalación y compilación (local)**

1. Instalar PlatformIO (recomendado):

```bash
# usando pipx (aislado)
brew install pipx
pipx ensurepath
pipx install platformio

# o con pip en virtualenv si prefieres
python3 -m venv ~/.venvs/pio
source ~/.venvs/pio/bin/activate
python -m pip install -U pip
python -m pip install platformio
```

2. Configura WiFi en `src/main.cpp` reemplazando `YOUR_SSID` y `YOUR_PASS`.
3. Edita `lib/TFT_eSPI/User_Setup.h` para seleccionar el driver correcto y los pines.
4. Compilar y subir al NodeMCU (asegúrate de seleccionar el puerto serie correcto):

```bash
cd /ruta/al/proyecto
platformio run -e nodemcuv2 -t upload
# Monitor serial
platformio device monitor -e nodemcuv2 -b 115200
```

**Flasheo al ESP8266 (NodeMCU v2)**

- Usando PlatformIO (recomendado): compila y sube directamente al puerto serie detectado.

```bash
# compilar y subir (reemplaza el entorno si lo cambiaste)
platformio run -e nodemcuv2 -t upload

# abrir monitor serie (115200 baudios)
platformio device monitor -e nodemcuv2 -b 115200
```

- Si prefieres usar `esptool.py` manualmente (necesitas compilar primero):

```bash
# compilar sin subir
platformio run -e nodemcuv2

# firmware resultante (ejemplo): .pio/build/nodemcuv2/firmware.bin
# identifica el puerto serie en macOS con: ls /dev/tty.*  o pio device list
esptool.py --port /dev/tty.SOME_USB_PORT write_flash 0x00000 .pio/build/nodemcuv2/firmware.bin

# luego abrir monitor serie
platformio device monitor -e nodemcuv2 -b 115200
```

Notas rápidas:
- En macOS los puertos suelen verse como `/dev/tty.SLAB_USBtoUART` o `/dev/tty.usbserial-XXXX`.
- Si PlatformIO no detecta el puerto, ejecuta `platformio device list`.
- El firmware ahora incluye un portal de configuración WiFi: si el ESP no tiene credenciales guardadas arrancará como AP `F1Dash-Setup` en la IP 192.168.4.1.

Si no tienes PlatformIO en el PATH, usa `pio` (alias) o la extensión PlatformIO en VS Code.

**Pruebas (unificadas)**

El proyecto usa un runner unificado `test/run_local.sh` que intentará ejecutar `platformio test -e native` si PlatformIO está disponible (instala la plataforma `native` si hace falta) y, si no, realizará un fallback compilando pruebas con `g++` desde `test/host/`.

Ejecutar local:

```bash
chmod +x test/run_local.sh
./test/run_local.sh
```

CI: el repo incluye un workflow de GitHub Actions en `.github/workflows/ci.yml` que ejecuta el mismo runner.

**Características implementadas**
- Listener UDP en `F1_UDP_PORT` (por defecto 20777)
- Parser mínimo para paquetes: CarTelemetry, CarStatus, LapData (solo campos necesarios)
- StateManager mantiene `current` y `previous` frames y helpers (`hasMFDChanged()`, `isInPit()`, `hasERSChanged()`)
- DashboardManager con pantallas: `MAIN`, `TYRES`, `ERS`, `DAMAGE`, `PITS`
- Renderers con actualizaciones parciales para minimizar redibujos
- Visualización de RPM en barra tipo LEDs
- Smoothing simple para valores críticos
- Salida de debug por Serial

**Limitaciones y notas de rendimiento**
- ESP8266 tiene RAM limitada: el código evita buffers grandes y minimiza asignaciones dinámicas.
- Frecuencia de refresco objetivo ~20–30 FPS; ajusta `lastRender` en `src/main.cpp` si necesitas otro valor.
- Parser usa offsets conservadores y no implementa el protocolo UDP completo; puede que necesites ajustar los offsets según la versión exacta del juego.

**Configuración adicional**
- `platformio.ini` contiene `build_flags` y librerías necesarias (`TFT_eSPI`, `ArduinoJson` opcional). Modifica según tus necesidades.

**Depuración y problemas comunes**
- Pantalla en negro: verifica `User_Setup.h` driver y pines, alimentación y reset pin.
- Errores de compilación PlatformIO/`native`: ejecuta `platformio platform install native` o usa `test/run_local.sh` para fallback g++.
- Si los paquetes UDP no se muestran, verifica IP del ESP está en la misma red que la PS5 y que el juego emite telemetría por UDP al puerto 20777.

**Siguientes mejoras posibles**
- Parse completo del protocolo F1 2025
- Visualizaciones adicionales (mapa, fuel strategy, lap delta)
- Soporte para múltiples displays y temas

Si querés, puedo generar instrucciones para flashear con esptool directamente, o crear templates para más pantallas.

---

Uso del dashboard
-----------------

El firmware muestra diferentes pantallas automáticamente según la telemetría recibida. A continuación se explica qué muestra cada pantalla y cómo se cambia.

- `MAIN` (pantalla principal):
	- Gear: número grande centrado (N = neutral, R = reverse).
	- Speed: velocidad en km/h mostrada grande.
	- RPM bar: barra horizontal estilo "LEDs" que indica régimen del motor; zonas verde/naranja/roja.
	- Throttle / Brake: barras de llenado que muestran posición del acelerador y freno.

- `TYRES` (neumáticos):
	- Muestra las 4 ruedas (FL/FR/RL/RR) con indicadores de temperatura y desgaste.
	- Actualmente usa valores de status mínimos como placeholders; puede ampliarse para leer datos de temperatura por rueda en el parser.

- `ERS`:
	- Barra de energía ERS (0–1000 escala del juego).
	- Indicador de modo de despliegue (`ersDeployMode`).

- `DAMAGE`:
	- Porcentajes de daño de componentes (alas, motor, etc.).
	- Actualmente usa algunos campos de estado como valores aproximados; puede mapearse a datos reales si el parser se amplía.

- `PITS`:
	- Pantalla prioritaria cuando `pitStatus` indica que el coche está entrando/en box.

Cómo se cambia la pantalla
- Automático: el firmware selecciona la pantalla según `mfdPanelIndex`, `pitStatus` y condiciones críticas (p. ej. pit). El índice `mfdPanelIndex` lo controla el juego (MFD en HUD) y hará que cambie la vista cuando el piloto active el MFD.
- Manual: actualmente no hay botones físicos configurados; podés añadir botones en el código (leer GPIOs) y llamar a `dashboard_update()` con la pantalla forzada.

Uso y depuración
- Serial: el firmware imprime mensajes de debug por Serial a `115200` — útil para depurar conexión WiFi, recepción UDP y parsing.
- Ajustes de rendimiento: modifica el intervalo de render en `src/main.cpp` (variable `lastRender`) para cambiar la tasa de refresco (valor actual ≈ 25 FPS).
- Ajustes de pantalla: edita `lib/TFT_eSPI/User_Setup.h` para ajustar pines, driver y frecuencia SPI.

Tests
-----

Run unified tests (prefer PlatformIO native, fallback to g++ host builds):

```bash
./test/run_local.sh
```

PlatformIO tests live under `test/` (Unity). Host-side helper tests live under `test/host/`.

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

