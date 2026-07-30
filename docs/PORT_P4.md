# Porting SkyGlass to the ESP32-P4 4" round board

Target: **Waveshare ESP32-P4-WIFI6-Touch-LCD-4C** — 4" round IPS, 720×720, MIPI-DSI.

Status: **running on hardware.** Every screen, the touch panel, the ADS-B feed, the
weather and radar imagery, audio, the web config page and OTA all work on the real
board. What follows keeps the pre-hardware reasoning because most of it held up, with
the places it did not called out.

Confirmed on the board itself:

| Subsystem | Result |
|---|---|
| MIPI-DSI panel | works — vendor init sequence + timings, PHY LDO ch3 @ 2.5 V |
| Touch | works — the part is a Goodix **GT9271**, not a GT911 (see below) |
| Wi-Fi via the C6 (`esp_hosted`) | works with plain `WiFi.h`, no hosted-specific init |
| ADS-B feed, weather, RainViewer radar, EUMETSAT, fire map, photos | all fetch and render |
| NVS, OTA, `WebServer`, mDNS (`skyglass-p4.local`) | work |
| Audio (ES8311) | works |
| `/shot.bmp` framebuffer capture | works — reads the DPI framebuffer back with `esp_cache_msync` |
| Render rate | ~12 fps at 720×720 under LVGL 8's software renderer |

The one thing that did *not* survive contact: the assumption that a faster SoC would
make the sweep smoother. It is slower than the S3, and the ceiling is LVGL 8 doing all
compositing on the CPU. See the performance note.

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
   than assuming one. Axis orientation is [VENDOR] too: `gt911.cpp` sets swap_xy,
   mirror_x and mirror_y all to 0.
3. ~~**Networking.**~~ Effectively settled off-hardware. The Arduino objects compile for
   P4, and the vendor's own Arduino example for this board (`GFX_ESPWiFiAnalyzer.ino`)
   uses plain `WiFi.h` with no hosted-specific init at all — so there is no missing
   setup step to discover. Unproven at runtime, and WiFiManager's captive portal (which
   assumes a native AP mode) remains the piece most likely to need attention.
4. ~~**Peripheral fallbacks.**~~ **Done** — `src/p4_shims.cpp` supplies the `display::`
   namespace (forwarding to the DSI driver) plus honest no-ops for the absent IMU, RTC
   and PMIC. `imu_facedown()` returns -1 (unavailable, so the caller leaves state alone)
   and `battery_percent()` returns -1 (unknown, so the HUD hides the indicator) rather
   than inventing plausible-looking values.

   Two gaps are deliberate and visible: display rotation reports 0 because the S3 path
   rotates by transposing flush blocks, which does not apply to a panel that owns its
   framebuffer; and `captureFrame()` returns nullptr, so `/shot.bmp` fails honestly
   instead of serving garbage. Both are wire-ups, not blockers.

   **WiFiManager cannot build for P4** — it references
   `CONFIG_ESP32_PHY_MAX_WIFI_TX_POWER`, which only exists on a chip with a native PHY.
   It is compiled out behind `BOARD_HAS_WIFIMANAGER`; the P4 instead connects with
   credentials from NVS and raises a `SkyGlass-Setup` SoftAP when none are stored, so
   the existing config page is still the way in.
5. ~~**UI layout pass.**~~ **Done** — 96 absolute placements in `ui.cpp` now go through
   `UI_S()`, which maps the 466-wide design space onto the built panel. Fonts cannot
   follow a scale factor (LVGL sizes are discrete), so `UI_BIG_PANEL` steps the tier up
   past 600 px wide. Still worth an eye over it on the real panel: proportional scaling
   gets the geometry right, not necessarily the balance.

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

## Performance note (measured)

The prediction here was that the P4 "could easily be better than the S3". It is not.
Measured with `/diag`'s `frame_ms`: **~85 ms a frame on the radar view**, about 12 fps,
against the S3's ~10 fps at less than half the pixels.

What was tried, and what it showed:

| Change | Effect |
|---|---|
| LVGL draw buffer 40 → 160 lines | none at all — 85 ms either way, so the cost is pixel work, not chunk setup |
| Shorter sweep trail (55° → 36°) | ~25% cheaper frames, and it looked worse: the trail is the motion blur |
| Finer sweep tick (16 → 12 ms) | pointless — the timer then fires more than once per rendered frame |
| Time-based sweep advance | measurably more even, and the user reported it as *more* stuttery; reverted |

The remaining lever is the P4's PPA (2D pixel-processing accelerator), which LVGL 8
cannot reach. That needs an LVGL 9 upgrade — a real piece of work, not a flag, and it
would have to be done for both boards at once. Deliberately not started.

Two lessons worth keeping: `fps` in `/diag` originally counted *draw chunks*, not
frames, and overstated the rate 4–7×; and where a change was about how motion *looks*,
the user's eye beat the measurement three separate times.

## Board abstraction (done)

Hardware specifics now live in `src/boards/`, selected by a `-D` flag:

```
src/boards/waveshare_s3_amoled_175.h    complete, verified
src/boards/waveshare_p4_lcd_4c.h        complete, verified on hardware
```

`config.h` keeps only app-level tunables and includes the right board header. Per the
project rule, unconfirmed pins stay `-1` and the build fails rather than guessing.

## What actually bit on the bench

The predicted hazards were mostly right. These are the ones that cost real time:

1. **The demo zip contains two init sequences under the same array name.** The panel
   stayed black through five wrong hypotheses (memory guard, PSRAM draw buffer, trail
   area, transpose buffer, colour format) before the cause turned out to be that
   `p4_panel_init.h` had been transcribed from the wrong branch of `displays_config.h`.
   The tell was register `0x40`: `0x00` on the 3.4" panel, `0x04` on this one. Transcribe
   from the **SCREEN_4INCH_DSI** branch, not by matching the symbol name.
2. **The backlight is active low.** `ledc_set_duty(..., on ? 0 : 255)`. Getting this
   backwards looks exactly like a dead panel with a working driver.
3. **Ordering in the DSI bring-up is not free-form.** LDO → DSI bus → DBI IO → *create
   the DPI panel* → send the init registers → `esp_lcd_panel_init()`. Sending the init
   registers before the DPI panel exists silently does nothing.
4. **The touch controller is a GT9271, not a GT911.** The driver now accepts the Goodix
   family by checking `id[0] == '9'` rather than matching `"911"`, and it must end the
   register-address write with a STOP (`endTransmission(true)`), not a repeated START.
5. **Framebuffer readback needs a cache sync.** `/shot.bmp` returns stale pixels without
   `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_DIR_M2C)` before reading the DPI buffer.
6. **The weather image is not a layout value.** `WX_RADAR_SIZE` is the resolution of the
   fetched tile; on a 720 panel the 360 px default left a black moat, so this board keeps
   the full 512 px tile. Positions inside that image must be unscaled; positions of the
   chrome around it scale with `UI_S()`. Mixing the two put the rings 28 px off the
   canvas and dropped the centre crosshair onto the status line.

## Order of work once the board is in hand

1. ~~Capture the vendor init sequence and timings.~~ **Done** — extracted from the
   published demo zip, no hardware needed. See `src/boards/p4_panel_init.h`.
2. ~~Confirm the backlight pin.~~ **Done** — `ESP-IDF/06_displaypanel_3.4inch` in the
   same demo confirms GPIO26, and cross-confirms `lcd_rst = 27`, 2 DSI lanes, and the
   PHY LDO on channel 3 at 2500 mV. Every panel and touch value is now [VENDOR].
3. ~~Flash the `esp32-p4-lcd-4c` env and look for `[p4lcd] DSI up:`.~~ **Done** — after
   fixing the init-sequence branch and the active-low backlight (see above).
4. ~~Touch.~~ **Done** — announces itself as `[gt911] found at 0x5D`, product id `9271`.
   Axis orientation from the vendor header was correct; no mirroring needed.
5. ~~Prove `esp_hosted` Wi-Fi.~~ **Done** — plain `WiFi.h` works, and every HTTPS client
   in the firmware runs unmodified.
6. ~~Then the UI.~~ **Done** — `UI_S()` got the geometry close, but a screenshot pass over
   all ten views was needed to find the collisions proportional scaling cannot fix
   (fonts step in discrete tiers, and unscaled fixed-size images do not move with it).

There is no captive portal on this board (WiFiManager needs a native PHY). Get it onto
a network with `wifi <ssid> <password>` over serial, or join the `SkyGlass-P4-Setup`
SoftAP; after that the web page at `skyglass-p4.local` is the way in.
