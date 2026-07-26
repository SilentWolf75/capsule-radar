#pragma once
// Self-update from the project's own GitHub Pages site (device-only).
//
// The web-flasher workflow already publishes manifest.json (carrying the version it
// built) and firmware.bin next to it, so the device can poll that manifest, compare it
// with FW_VERSION, and stream the binary straight into the inactive OTA slot. No cable
// and no computer.
//
// Safe by construction: Update writes to whichever of ota_0/ota_1 is not running and
// only flips otadata after the image verifies, so an interrupted download or a power
// cut leaves the current firmware bootable.
#include <stddef.h>
#include <stdint.h>

void updater_begin(void);                       // load settings; no network yet
bool updater_auto_check(void);                  // is periodic checking enabled?
void updater_set_auto_check(bool on);
bool updater_auto_install(void);                // install without asking? (off by default)
void updater_set_auto_install(bool on);

// Network task: run periodically. Handles both the scheduled check and any install the
// UI has requested. Returns true while it has work in flight.
bool updater_poll(void);

void updater_request_check(void);               // UI: check now
void updater_request_install(void);             // UI: install the version we found

// UI: latest version seen on the server ("" until a check succeeds), and a short status
// line for the config page.
void updater_state(char *latest, size_t ln, char *status, size_t sn, bool *updateAvailable);
