# Porting SkyGlass to the ESP32-P4 4" round board

Target: **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C** — 4" round IPS, 720×720, MIPI-DSI.

Status: **scaffolding only.** The board abstraction is in place and the existing 1.75"
build is unaffected. Nothing P4-specific has been compiled or run yet.

## What this board actually is

Worth being blunt up front, because it is not simply "a bigger S3":

| | 1.75" board (working) | 4C board (new) |
|---|---|---|
| SoC | ESP32-S3, Xtensa, dual 240 MHz | **ESP32-P4, RISC-V**, dual + 40 MHz LP core |
| PSRAM / flash | 8 MB / 16 MB | **32 MB / 32 MB** |
| Panel | CO5300 AMOLED 466×466, **QSPI** | IPS 720×720, **MIPI-DSI 2-lane** |
| Radio | native Wi-Fi + BLE | **none** — ESP32-C6-MINI-1 over SDIO |
| Touch | CST9217 @ 0x5A | not yet identified |
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

1. **Display driver.** `Arduino_GFX` has no MIPI-DSI path, so `display.cpp` cannot be
   reused. The panel needs ESP-IDF's `esp_lcd_mipi_dsi` driven from the Arduino
   framework (legal — Arduino-ESP32 3.x exposes IDF APIs). This is the largest single
   piece of work.
2. **Touch driver.** Controller not yet identified; GT911 is the usual Waveshare choice
   but that is a guess and must be read off the vendor demo, not assumed.
3. **Networking.** The P4 has no radio. Wi-Fi is an ESP32-C6 over SDIO via `esp_hosted`.
   Whether the Arduino `WiFi` / `WiFiClientSecure` / `HTTPClient` objects work unchanged
   on top of that is the single biggest open question — WiFiManager's captive portal in
   particular assumes a native AP mode.
4. **Peripheral fallbacks.** No IMU, RTC or PMIC. Face-down sleep, the pre-WiFi clock and
   battery reporting must degrade gracefully rather than be assumed present — hence the
   `BOARD_HAS_*` flags.
5. **UI layout pass.** Geometry is now parameterised, but many alignment offsets in
   `ui.cpp` are absolute pixels tuned for 466×466. They will render small and
   off-balance at 720×720 until they are proportional.

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

1. Flash a vendor demo first and confirm the panel lights up — establishes the toolchain
   and gives the real pin values.
2. Copy the confirmed pins into `waveshare_p4_lcd_4c.h`.
3. Bring up MIPI-DSI + LVGL with a static test pattern.
4. Bring up touch.
5. Prove `esp_hosted` Wi-Fi with a plain HTTPS GET before wiring the ADS-B client.
6. Then the UI, which should largely already work.
