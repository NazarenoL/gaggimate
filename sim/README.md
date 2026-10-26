# Desktop simulator (`display-sim`)

Runs the **real** GaggiMate display firmware (`src/display/`) natively on your
computer, with the BLE link to the controller ESP32 (`lib/NanoPbComm`) **mocked**.
The LVGL UI renders in an SDL window, a fake controller simulates a full brew, and
the embedded WebUI is served on `localhost`. Use it for fast UI iteration,
screenshots, and validation without any hardware. (Linear: GM-107.)

## 1. Prepare your computer

macOS:

```shell
xcode-select --install     # C/C++ toolchain (clang + assembler); skip if already installed
brew install sdl2          # the windowing/graphics backend
# PlatformIO is already what you build the firmware with; the native platform
# auto-installs on the first `pio run -e display-sim`.
```

Linux (Debian/Ubuntu): `sudo apt install build-essential libsdl2-dev`.

If you want the **WebUI**, build its embedded bundle once (needs Node 22 — `nvm use`):

```shell
scripts/build_webui.sh     # generates src/display/webassets/web_ui.bin (git-ignored)
```

The sim `.incbin`s that `web_ui.bin`, so it must exist before building with WebUI.
(`build_webui.sh` is also what the device build uses.)

## 2. Build & run

```shell
pio run -e display-sim                 # build
pio run -e display-sim -t run          # build + launch (also: ./.pio/build/display-sim/program)
```

`-t run` is a custom PlatformIO target (see `scripts/sim_run.py`). In a PlatformIO
IDE (CLion/VSCode) it shows up under the `display-sim` environment as the
**Run Simulator** task, so you can build and launch the sim with one click.

- **Interact** with the mouse (it acts as the touchscreen). Keys `1` and `2` press
  and release the brew and steam inputs of the controller board (hold both for
  the combined third button, hold one for a long press).
- **WebUI**: open <http://localhost:8080/> while it runs (port 80 is remapped to
  8080 so it needs no root). Live status streams over the WebSocket just like on
  the device.
- **Screenshot mode** (renders briefly, writes a BMP, exits — handy for
  validation/CI):

  ```shell
  ./.pio/build/display-sim/program --screenshot shot.bmp 4000   # 4000 = delay ms
  # convert to PNG on macOS: sips -s format png shot.bmp --out shot.png
  ```

- **State** (settings, profiles, shot history) persists under `sim_data/`
  (git-ignored). Delete it to start fresh.

## 3. Regression tests

After building, run `scripts/test_display_beans.sh`. It uses SDL's headless driver
and a fresh temporary state directory, so it never changes your interactive
simulator library. The suite exercises bean selection, post-brew grind saves,
barcode registration, legacy JSON storage, and repeated touch-driven exits. It
also checks that persistence waits for touch dispatch to return and WebSocket
library reads wait for the web loop; reintroducing synchronous bean notifications
fails even though the host has enough stack to survive them.

For memory and undefined-behavior checks on Linux:

```shell
pio run -e display-sim-sanitized -j 2
DISPLAY_SIM_ENV=display-sim-sanitized \
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  scripts/test_display_beans.sh
```

Leak detection is disabled because the simulator does not tear down firmware
globals/tasks at exit. UBSan alignment checks are also excluded: EEZ's generated
assets use 4-byte ESP32 packing while the 64-bit host requires 8-byte alignment
for some values. Other undefined behavior and invalid accesses still fail the
run. `DISPLAY_SIM_TEST_DIR` optionally chooses where screenshots/state are saved.
The custom firmware workflow runs **display-simulator / bean-regressions** on
pushes to master, pull requests targeting master, and manual builds/releases.
Firmware builds/releases depend on it passing.

This is behavioral and host-memory coverage, not an ESP32 stack budget test.
FreeRTOS tasks are no-ops here; host ABI, filesystems, flash IPC and interrupts
differ. Compiler RAM/flash summaries cannot detect exhaustion of a task's stack.
Changes to nested callbacks or storage paths still need a device test with
`uxTaskGetStackHighWaterMark()` sampled after exercising them (ESP-IDF reports
bytes). Exercise repeated bean confirmations and post-brew saves with a web
client connected, then check minimum stack headroom, rather than only checking
for a reboot. A future hardware CI runner can enforce that headroom; shrinking
the desktop stack to the ESP32 allocation would not be a reliable substitute.

## 4. How it works

Everything simulator-only lives here in `sim/`; the firmware in `src/` is built
unchanged except for a few small `#ifndef GAGGIMATE_SIM` guards.

| Folder | Role |
|---|---|
| `sim/platform/` | Host shims for the Arduino/ESP32 APIs the firmware uses — Arduino core (vendored `String`/`Print`/`Stream`), FreeRTOS, `FS`/`LittleFS`/`SPIFFS`/`SD_MMC`, `Preferences` (NVS), `WiFi`, and the `esp_*` headers. `xTaskCreate*` is a no-op so the sim drives the firmware's loop methods cooperatively on the main thread. |
| `sim/comms/` | Mock of the `GaggiMateClient` BLE facade + a `MockController` thermal/hydraulic model that reacts to the boiler/pump/relay commands the display sends and emits sensor telemetry (temperature, pressure, flow, scale weight). |
| `sim/driver/` | `SdlDriver` — an SDL2 window wired into LVGL as the display + mouse-as-touch input, plus a screenshot helper. |
| `sim/web/` | Host shim of `ESPAsyncWebServer`/`AsyncWebSocket`/`DNSServer` over a tiny non-blocking HTTP/1.1 + WebSocket server (pumped from the main loop, so handlers never race the firmware). OTA / BLE-scale endpoints are stubbed. |
| `sim/main.cpp` | Entry point: builds the `Controller`, then runs one cooperative loop (controller + UI + web server + SDL) on the main thread. |

The PlatformIO env is `[env:display-sim]` (`platform = native`) in `platformio.ini`.

## Caveats

- **Compiled out** for the desktop: MQTT/HomeAssistant, HomeKit, mDNS, the WiFi
  watchdogs, BLE scales, and firmware OTA. The core machine + brew/steam/grind +
  profiles + settings + WebUI all run.
- The mock controller is a plausible model, not a physical one — temperatures and
  pressures are illustrative, not calibrated.
- macOS only needs the main thread for SDL, which is why the whole sim is a single
  cooperative loop.
