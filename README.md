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
- `TFT_DC`   → D3 (GPIO0)
- `TFT_RST`  → D4 (GPIO2)
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

Contacto y soporte
--
Si encontrás errores o querés nuevas funcionalidades, abrí un issue o un PR en el repositorio.


Tests
-----

Run unified tests (prefer PlatformIO native, fallback to g++ host builds):

```bash
./test/run_local.sh
```

PlatformIO tests live under `test/` (Unity). Host-side helper tests live under `test/host/`.

