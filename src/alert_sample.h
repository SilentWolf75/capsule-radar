#pragma once
// Optional compile-time alert sample.
//
// Empty by default, and normally left that way: the config page can upload a WAV at
// runtime (converted on the device and stored in LittleFS), which survives reboots and
// reflashes without a rebuild. That is the path most people want.
//
// This header exists for the case where you'd rather bake a sound into the firmware
// image itself — a kiosk build, or a unit that ships with its sound already set:
//
//     python tools/gen_alert_sound.py my-warning.wav > src/alert_sample.h
//
// It emits 16 kHz signed 16-bit mono PCM (the codec's native rate). Keep it short; this
// fires on live traffic events, and a long clip both eats flash and outlasts the thing
// it is announcing. An uploaded sound always takes precedence over this one.
//
// Licensing: whatever you put here becomes data inside the firmware image and, if you
// push it, inside your repository. Plenty of stock-audio licences allow use in a project
// but not redistribution as an asset — check before committing a generated copy.
#include <stdint.h>

#define ALERT_SAMPLE_RATE 16000

// Empty by default. The generator replaces both of these.
#define ALERT_SAMPLE_LEN 0
static const int16_t ALERT_SAMPLE[1] = { 0 };
