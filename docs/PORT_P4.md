# Porting SkyGlass to the ESP32-P4 4" round board

Target: **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C** — 4" round IPS, 720×720, MIPI-DSI.

Status: **display and touch drivers written and compiling; nothing hardware-verified.**
`pio run -e esp32-p4-lcd-4c` builds the whole firmware for RISC-V at 720x720 -- the UI,
every data client, a MIPI-DSI panel driver and a GT911 touch driver. It has never run
on hardware; the board is not here.

Verified on this toolchain, no hardware required:

| Probe | Result |
|---|---|
| Arduino framework targets ESP32-P4 | works, on the platform version already pinned |
| `WiFi` / `WiFiClientSecure` / `HTTPClient` | compile -- `esp_hosted` is transparent at the Arduino API |
| `Preferences` / `Update` / `WebServer` / `ESPmDNS` | compile, so NVS, OTA, the config page and mDNS should port |
| LVGL 8.4 on RISC-V at 720x720 | works |
| Full SkyGlass UI + clients | **compiles and links**: RAM 30.8%, flash 2.47 MB |

That retires the biggest listed risk. Wi-Fi existing at all on a radio-less P4 was the
open question; the Arduino objects are present, so the port is a display/touch problem
rather than a networking rewrite. Compiling is not running -- the C6 must be flashed
with hosted firmware and that can only be confirmed on the bench.

## What this board actually is

Worth being blunt up front, because it is not simply "a bigger S3":

| | 1.75" board (working) | 4C board (new) |
|---|---|---|
| SoC | ESP32-S3, Xtensa, dual 240 MHz | **ESP32-P4, RISC-V**, dual + 40 MHz LP core |
| PSRAM / flash | 8 MB / 16 MB | **32 MB / 32 MB** |
| Panel | CO5300 AMOLED 466×466, **QSPI** | IPS 720×720, **MIPI-DSI 2-lane** |
| Radio | native Wi-Fi + BLE | **none** — ESP32-C6-MINI-1 over SDIO |
| Touch | CST9217 @ 0x5A | **GT911** (driver written) |
| IMU / RTC / PMIC | QMI8658 / PCF85063 / AXP2101 | none of them |
| Audio | ES8311 | ES8311 (same part) |
| I2C | SDA 15, SCL 14 | **SDA 7, SCL 8** (confirmed in vendor wiki) |

720×720 is **2.4× the pixels** of the current panel.

## What ports for free

Most of the firmware. These are pure logic or portable LVGL and should need nothing
beyond a recompile — they already build for the native SDL simulator, which is the
existing proof of portability:

`radar_view` · `ui` · `geo` · `aircraft_types` · `adsb_client` (parsing) · `route` ·
`photo` · `airline` · `firemap` · `coastline` · `wildfire` · `vessel` · `map_bg` ·
`weather` · `wx_radar` · `cloud_image` · `airports` · `updater`

## What has to be written

1. ~~**Display driver.**~~ **Written** — `src/display_p4.cpp` drives the panel through
   ESP-IDF's `esp_lcd_mipi_dsi`: PHY LDO, DSI bus, DBI channel for the init registers,
   DPI video panel with DMA2D blitting, LEDC backlight, and an LVGL flush. Compiles.
   **Unverified against hardware**, and the two panel-specific pieces — the init
   sequence and the video timings — are deliberately isolated so the vendor demo's
   values drop in without touching driver logic. See the hazard section below.
2. ~~**Touch driver.**~~ **Written** — `src/touch_gt911.cpp`. GT911's register map is
   documented, so the protocol is not guesswork. It probes both possible addresses
   (0x5D and 0x14, strapped by INT at reset) and verifies the `911` product id rather
   than assuming one, which removes a common bring-up dead end. Axis mirroring still
   needs confirming on hardware.
3. **Networking.** No longer the main risk: the Arduino networking objects compile for
   P4 (see the table above), so `esp_hosted` is transparent at the API level. Still
   unproven at runtime — the C6 needs hosted firmware, and WiFiManager's captive portal
   assumes a native AP mode, which is the piece most likely to need replacing.
4. **Peripheral fallbacks.** No IMU, RTC or PMIC. Face-down sleep, the pre-WiFi clock and
   battery reporting must degrade gracefully rather than be assumed present — hence the
   `BOARD_HAS_*` flags.
5. **UI layout pass.** Geometry is now parameterised, but many alignment offsets in
   `ui.cpp` are absolute pixels tuned for 466×466. They will render small and
   off-balance at 720×720 until they are proportional.

## The vendor demo is published — use it

`ESP32-P4-WIFI6-Touch-LCD-XC-Demo.zip` (linked from the wiki, ~118 MB) contains an
`Arduino/` tree, and it answers everything that looked hardware-blocked:

- `Arduino/libraries/displays/displays_config.h` — the `4INCH-DSI` config: timings,
  `lcd_rst = 27`, `i2c_sda = 7`, `i2c_scl = 8`, `i2c_clock_speed = 100000`, and a
  **197-command init sequence**, now transcribed verbatim into
  `src/boards/p4_panel_init.h`.
- `Arduino/libraries/displays/gt911.h` — confirms GT911, addresses 0x5D / 0x14, and
  `TOUCH_RST = GPIO23` with `TOUCH_INT` not connected.
- `Arduino/libraries/GFX_Library_for_Arduino/src/databus/Arduino_ESP32DSIPanel.cpp` —
  a MIPI-DSI databus for Arduino_GFX. An earlier claim in these notes that
  "Arduino_GFX has no DSI path" was **wrong**. This port still goes straight to
  `esp_lcd_mipi_dsi`, which avoids depending on the vendor's library fork.

Two corrections worth carrying forward:

- The timings previously flagged SUSPECT here are **the vendor's own values**. The
  reasoning that produced that flag (implied refresh looked high for a 4" panel) was
  simply wrong.
- I2C runs at **100 kHz** on this board, not the 400 kHz that would be a natural default.

## Known hazard: the MIPI-DSI panel

Someone running **this exact board** under ESPHome got audio, microphones, the media
player, the voice assistant and the **touchscreen** all working, and never got a single
pixel out of the display. Their logs reported success throughout. Worth knowing before
losing an evening to it:

- They reused the **10.1" WAVESHARE-P4-NANO** panel driver, assuming "same LCD driver,
  minor timing differences". The numbers argue otherwise: their porches give
  800 x 760 total at 80 MHz pclk, about **131 Hz**, which is not a 4" panel figure.
- Nobody in that thread solved it. ESPHome's `mipi_dsi` component is new and needs a
  per-panel driver that this display does not yet have.
- The important detail: they said **the vendor's own examples worked** before they
  started modifying things. So the panel is drivable — by ESP-IDF's `esp_lcd_mipi_dsi`
  with Waveshare's init sequence. That is the route this port should take, and it is
  *more* likely to work than the ESPHome path, not less.
- The DSI PHY runs off an internal LDO (**channel 3 at 2.5 V**) that has to be switched
  on explicitly. Omitting it produces exactly the "everything logs fine, screen stays
  black" symptom.

Practical consequence: **get the vendor ESP-IDF demo running first and capture its init
sequence and timings verbatim.** Do not port timings from another panel.

## Performance note

We spent real effort making the sweep smooth on the S3 and landed at ~10 fps with a
fixed 3°/frame step (see `radar_view.cpp`). Do **not** assume that carries over:

- Against it: 2.4× the pixels.
- For it: the P4 is substantially faster, has a 2D pixel-processing accelerator, far more
  memory bandwidth, and MIPI-DSI beats QSPI for pushing frames.

It could easily be better than the S3. Measure with `/diag`'s `step_avg` / `step_max`
before tuning anything — that telemetry is already in place and is what found the real
problem last time.

## Board abstraction (done)

Hardware specifics now live in `src/boards/`, selected by a `-D` flag:

```
src/boards/waveshare_s3_amoled_175.h    complete, verified
src/boards/waveshare_p4_lcd_4c.h        720×720 + I2C 7/8 confirmed; rest are -1
```

`config.h` keeps only app-level tunables and includes the right board header. Per the
project rule, unconfirmed pins stay `-1` and the build fails rather than guessing.

## Order of work once the board is in hand

1. ~~Capture the vendor init sequence and timings.~~ **Done** — extracted from the
   published demo zip, no hardware needed. See `src/boards/p4_panel_init.h`.
2. Confirm the one remaining [COMMUNITY] value, the backlight pin (GPIO26), against the
   vendor demo or the schematic PDF.
3. Flash the `esp32-p4-lcd-4c` env and look for `[p4lcd] DSI up:` on serial. The init
   sequence and timings are already in place from the vendor demo, so this may simply
   work; if it does not, re-extract rather than tweak.
4. Touch should announce itself as `[gt911] found at 0x..`; fix axis mirroring in the
   board header if the coordinates come out flipped.
5. Prove `esp_hosted` Wi-Fi with a plain HTTPS GET before wiring the ADS-B client.
6. Then the UI, which should largely already work.
