# Capsule Radar 🛩️

<p align="center">
  <a href="https://silentwolf75.github.io/capsule-radar/"><img src="https://img.shields.io/badge/Flash%20in%20browser-FF6D00?logo=googlechrome&logoColor=white" alt="Flash in browser"></a>
  <a href="https://makerworld.com/en/models/2907695-capsule-radar-live-flight-radar-desk-gadget"><img src="https://img.shields.io/badge/MakerWorld-3D%20case-1A8917?logo=bambulab&logoColor=white" alt="MakerWorld – 3D case"></a>
  <img src="https://img.shields.io/badge/board-ESP32--S3%20round%20AMOLED-E7352C?logo=espressif&logoColor=white" alt="Board: ESP32-S3 round AMOLED">
  <a href="https://github.com/SilentWolf75/capsule-radar/releases"><img src="https://img.shields.io/github/v/tag/SilentWolf75/capsule-radar?label=firmware&color=7B42BC" alt="Firmware version"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/code-MIT-2088FF" alt="License: MIT"></a>
  <img src="https://img.shields.io/github/languages/count/SilentWolf75/capsule-radar?label=languages&color=FFC107" alt="Languages">
  <a href="https://github.com/SilentWolf75/capsule-radar/stargazers"><img src="https://img.shields.io/github/stars/SilentWolf75/capsule-radar?style=social" alt="GitHub stars"></a>
</p>

> A fork of [socquique/capsule-radar](https://github.com/socquique/capsule-radar) with features
> adapted from [yashmulgaonkar/FlightScnr_Pi](https://github.com/yashmulgaonkar/FlightScnr_Pi):
> per-type aircraft icons, pinch-zoom, a clock view, tracked flights, airline identity,
> map-tile basemap, wildfire and marine-AIS layers, quiet hours, and configurable alert
> sounds with WAV upload. See [`docs/FEATURES.md`](docs/FEATURES.md).

## Screens

Captured from the device itself — these are the real framebuffer, not photographs
(`python tools/grab_screens.py`, which pulls `/shot.bmp` off the running firmware).

| Radar | Detail card | Contacts |
|:---:|:---:|:---:|
| <img src="docs/img/screens/radar.png" width="240"> | <img src="docs/img/screens/detail.png" width="240"> | <img src="docs/img/screens/list.png" width="240"> |
| Aircraft drawn by type, altitude-coloured, with airports labelled by ICAO/IATA ident | Tap a contact for callsign, type, altitude, speed, heading, squawk — plus the airline and route looked up automatically | Nearest-first contact list |

| Stats | Precipitation | Satellite |
|:---:|:---:|:---:|
| <img src="docs/img/screens/stats.png" width="240"> | <img src="docs/img/screens/wx-radar.png" width="240"> | <img src="docs/img/screens/wx-cloud.png" width="240"> |
| Traffic summary and how to reach the config page | RainViewer echoes, aviation-style overlays | EUMETSAT cloud-type imagery |

| Forecast | Tracked flight | Clock |
|:---:|:---:|:---:|
| <img src="docs/img/screens/forecast.png" width="240"> | <img src="docs/img/screens/tracked.png" width="240"> | <img src="docs/img/screens/clock.png" width="240"> |
| Three days with vector weather icons | Follow one aircraft: route progress and ETA | Watch face with a seconds arc and current conditions |

| Fire map |
|:---:|
| <img src="docs/img/screens/firemap.png" width="240"> |
| Active fires across North America, sized and coloured by radiative power — tap a cluster to zoom in and switch to the higher-resolution VIIRS feed |

<p align="center">
  <img src="docs/img/device.JPG" width="330" alt="Capsule Radar — a live flight on the device">
</p>
<p align="center"><sub>A real flight on the device: callsign, type, altitude/speed, <b>route</b> (Lisbon → Abu Dhabi) and the <b>aircraft photo</b> — all looked up automatically.</sub></p>

A live **ADS-B aircraft radar** for the **Waveshare ESP32-S3-Touch-AMOLED-1.75** — a round 466×466 AMOLED with capacitive touch. It pulls nearby aircraft from a free online feed over WiFi and plots them on a touch radar scope centered on your location, with live flight details and selectable visual skins.

> Visual reference: open [`assets/plane_radar_2.0_mockup.html`](assets/plane_radar_2.0_mockup.html) in a browser.

<p align="center"><img src="docs/img/radar.gif" width="360" alt="Capsule Radar live scope"></p>

| Phosphor | Orb | Amber CRT | Military |
|:--:|:--:|:--:|:--:|
| ![Phosphor](docs/img/radar.png) | ![Orb](docs/img/orb.png) | ![Amber](docs/img/amber.png) | ![Military](docs/img/military.png) |

<sub>Captured from the bundled desktop simulator (the device screen is round; the square corners are off-panel).</sub>

## Features

- **Live traffic** from [airplanes.live](https://airplanes.live) (free, non-commercial; fallback adsb.lol), updated every couple of seconds. Memory-safe streaming parser with a hard aircraft cap.
- **Four themes** (long-press the screen to cycle, or pick on the web; remembered across reboots):
  - **Phosphor** — green-on-black radar scope: rings, animated sweep, aircraft glyphs rotated by heading and color-coded by altitude, fading trails, emergency halo.
  - **Orb** — green gradient + grid scope: the 7 nearest aircraft as yellow orbs emitting waves, off-range traffic as edge arrows pointing its way, orange target rings.
  - **Amber CRT** and **Military** — the same scope retinted (warm amber / night-vision green).
- **Touch** (CST9217): tap an aircraft → detail card (callsign, type, altitude, vertical speed, ground speed, distance, heading, squawk, **airline name**, and **origin → destination** looked up from adsbdb, cached in NVS). **Double-tap** to cycle zoom range, or **pinch with two fingers** to zoom continuously. Swipe between **Radar / List / Stats / Weather / Tracked / Clock / Fires** (circular layouts).
- **Tracked flight**: press **TRACK** on any aircraft's card to follow it — the Tracked view shows its route, a **progress bar** along the great circle, distance remaining and **ETA** from its ground speed. A tracked contact is pinned, so the on-screen aircraft cap never drops it as it flies away.
- **Map background** (optional): dark or light [CARTO](https://carto.com/attribution/) basemap tiles under the scope, resampled through the radar's own projection so coastlines and roads line up with the range rings at every zoom. Tiles download one at a time so the live feed never stalls. A **visibility slider** (0–100%) on the config page fades the basemap live against the scope, so you can dial in exactly how much map shows through without rebuilding the tiles.
- **Aircraft icons by type**: the ICAO type code picks a silhouette — swept-wing jets in three sizes, straight-wing props, helicopters (with a rotor disc), fighters and gliders — all rotated by track. Falls back to the generic glyph for unknown types, and the whole thing can be switched off.
- **Marine traffic** (optional, needs a free [aisstream.io](https://aisstream.io) key): switch the scope from **Aircraft** to **Marine vessels** and it plots AIS contacts as cyan hulls oriented by course, with ship names; tap one for a card with MMSI, speed over ground, course, distance and bearing. The two are separate pictures rather than one overlay, and the list view follows the setting.
- **12- or 24-hour clock**: applies to the HUD, the clock face and the weather-imagery timestamps.
- **Wildfire markers** (optional, needs a free [NASA FIRMS](https://firms.modaps.eosdis.nasa.gov/api/map_key/) key): active fire detections plotted on the scope, sized and coloured by fire radiative power.
- **Continental fire map** (same FIRMS key): a whole extra screen covering the US, Canada and Mexico, with coastlines and state borders. Detections are binned into clusters so a continent's worth of fires fits on a round 466 px panel. **Tap any area to zoom in** — the zoomed view re-queries the higher-resolution VIIRS feed instead of the MODIS overview, and you can keep tapping to re-centre. Refreshes every 15 minutes.
- **Self-update over WiFi**: the device checks this repo's GitHub Pages build for a newer version and can install it itself — no cable, no toolchain. Check and install on demand from the config page, or leave the 6-hourly auto-check on. Writes to the inactive OTA slot, so a failed download can't brick a working install.
- **Clock view**: a full watch face with day/date, current conditions and a three-day strip — the natural thing to leave on screen overnight.
- **Quiet hours**: a configurable window that dims the screen, switches it off, or forces the clock view; a touch wakes it for 15 seconds.
- **Boot splash** + **alert sounds** (ES8311 speaker): a soft cue when a new aircraft enters range, an urgent one for emergency/military. Choose between **Chime, Sonar ping, Marimba, Aircraft warning, Beep**, or **upload your own WAV from the config page** — it's converted on the device (stereo downmixed, any 8–48 kHz rate resampled to 16 kHz, volume normalised) and stored in flash, so it survives reboots without rebuilding. Volume, mute and per-cue test buttons on the web page.
- **Smooth motion**: aircraft glyphs glide between polls (interpolated) instead of jumping, using cheap partial redraws.
- **Top HUD**: WiFi status (amber if the data feed is failing), in-range aircraft count, NTP/RTC clock, **battery %** (charging bolt, red when low), and the date. The Stats view footer shows how to reach the config page (`capsuleradar.local` + IP).
- **Battery aware** (AXP2101): shows charge level, warns when low, and slows the feed poll rate on battery to save power.
- **Real-time clock** (PCF85063): keeps the time/date across power loss, so the clock is right even before/without WiFi; re-synced from NTP when online.
- **Smart brightness**: configurable idle auto-dim (no touch), and **face-down sleep** (QMI8658 IMU — flip it over to turn the screen off).
- **GPS auto-location** (optional **-G** board variant): the Waveshare `-G` board has an onboard GPS (Quectel LC76G). Turn it on from the web page and the radar **sets its own center point automatically**, with an on-screen **satellite status icon** (amber while acquiring, green once it has a fix). Standard boards simply enter their location manually.
- **Configuration web page** at `http://capsuleradar.local/` — center point (map picker), display range, theme, **time zone** (auto-detected from your browser), live brightness slider, map background, marine/wildfire layers and their API keys, quiet hours, sound, WiFi reset, and over-the-air firmware update. Settings persist in NVS.
- **First-boot WiFi setup** via a captive portal (`CapsuleRadar-Setup`).

## Hardware

Waveshare **ESP32-S3-Touch-AMOLED-1.75**: ESP32-S3R8 (8 MB PSRAM, 16 MB flash), **CO5300** AMOLED over QSPI, **CST9217** touch, **QMI8658** IMU, **PCF85063** RTC, **AXP2101** PMIC, **ES8311** audio + speaker, microSD. All pins are in [`src/config.h`](src/config.h) (sourced from the board definition; no guessing).

## Build & flash (PlatformIO)

```bash
pio run -e esp32-s3-amoled-175 -t upload     # build + flash over USB-C
pio device monitor -b 115200                  # serial log
```
On first flash you may need to hold **BOOT** then tap **RESET**. After flashing, on first boot connect your phone to the **`CapsuleRadar-Setup`** WiFi and enter your home network — real aircraft appear within seconds.

## Flash from your browser (no toolchain)

Flash without installing anything using **ESP Web Tools** (Chrome or Edge on desktop):

1. Open the **[web flasher](https://silentwolf75.github.io/capsule-radar/)**.
2. Plug the board in with a USB-C **data** cable and click **Install**.

> Leave **Erase device** unticked to keep your WiFi credentials, settings and uploaded
> alert sound; tick it for a clean install or to recover a confused device. Updating over
> WiFi preserves everything too and needs no cable: open `capsuleradar.local/update` and
> upload `firmware.bin`, or run `pio run -e esp32-s3-amoled-175-ota -t upload`.
>
> *(Before v1.8.2 the flasher wrote one merged image starting at `0x0`, whose `0xFF`
> padding covered the NVS region at `0x9000`. That wiped settings on every web flash
> regardless of the erase checkbox. It now writes the four images to their own offsets.)*

The flasher is built and published automatically by GitHub Actions
([`.github/workflows/webflasher.yml`](.github/workflows/webflasher.yml)) on every push to
`main`, and served from GitHub Pages. Each build merges bootloader + partition table +
application into a single image and stamps it with `FW_VERSION` from
[`src/config.h`](src/config.h), so the version shown in the browser always matches what
gets written. Tagged releases (`git tag v1.6.3 && git push origin v1.6.3`) also attach a
ready-to-flash `CapsuleRadar-esp32s3.bin` to a **GitHub Release** via
[`release.yml`](.github/workflows/release.yml). To preview the flasher locally:

```bash
./scripts/build_webflasher.sh                      # build + merge into web/flash/
python3 -m http.server -d web/flash 8000           # serve (Web Serial works on localhost)
# open http://localhost:8000
```

## Desktop simulator

The whole UI is portable LVGL and runs on your computer (SDL2) — great for iterating without hardware:
```bash
pio run -e native -t exec     # opens a 466×466 window (needs SDL2: `brew install sdl2`)
```
Mouse = touch · `T` = switch theme · close the window to quit.

## Configuration

Browse to `http://capsuleradar.local/` (or the device IP) on the same WiFi to set the **center lat/lon**, **display range**, **theme** and **brightness**, or to **reset WiFi**. Saving restarts the device to apply.

## Repo layout

```
src/
  config.h           pins + tunables (Dénia, Spain by default)
  main.cpp           tasks, WiFi/NTP, web config, brightness/IMU glue
  display.*          CO5300 (Arduino_GFX) + LVGL bring-up
  radar_view.*       the radar scope, aircraft, themes
  aircraft_types.*   ICAO type designator -> drawing silhouette
  ui.*               views (radar/list/stats/weather/tracked/clock) + cards + HUD
  touch_cst9217.*    capacitive touch driver (single touch + 2-point pinch)
  map_bg.* map_client.*    CARTO basemap: tile fetch, stitch, reproject to the scope
  vessel.* ais_client.*    AIS marine traffic (aisstream.io WebSocket)
  wildfire.* wildfire_client.*  NASA FIRMS active-fire markers
  airline.* airline_client.*    operator name (offline table) + logo download
  imu_qmi8658.*      accelerometer (face-down sleep)
  battery.*          AXP2101 battery gauge
  rtc_pcf85063.*     PCF85063 real-time clock
  adsb_client.*      airplanes.live fetch + parse
  route*.* route.*   origin→destination lookup (adsbdb)
  sim_main.cpp       native SDL simulator (not flashed)
include/lv_conf.h    LVGL config (v8)
web/flash/           browser web-flasher (ESP Web Tools) for makers
scripts/             build_webflasher.sh (merge firmware -> single .bin)
docs/                hardware / data-source / architecture notes
```

## Community ports & forks

- **[Capsule Radar for the Waveshare ESP32-S3-Touch-LCD-2.1](https://github.com/alexzogh/capsule-radar/tree/port/esp32-s3-lcd-21)** by **@alexzogh (STLWarehouse)** — a full port to the 2.1" round LCD (ST7701), plus new features: **double-tap to track an aircraft** (scope re-centres on it), a **clock face on idle**, and the busy-airspace **query-radius fix** now merged back into this firmware. Ships its own binaries per release.

## Data & license

**Firmware / code: [MIT](LICENSE)** — fork and build on it freely (keep the notice). Aircraft data: **airplanes.live** (free, **non-commercial / educational** — exactly this project; be polite with request cadence). Routes: **adsbdb.com** (free). Basemap tiles: **CARTO** / **OpenStreetMap** contributors. Marine AIS: **aisstream.io** (own key). Active fires: **NASA FIRMS** (own key). Airline logos are fetched on demand and remain the property of their respective owners. Personal/hobby project. The 3D-printed enclosure is published on [MakerWorld](https://makerworld.com/en/models/2907695-capsule-radar-live-flight-radar-desk-gadget) (enclosure + this firmware).
