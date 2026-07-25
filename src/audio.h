#pragma once
// ES8311 codec + speaker: short alert "pings". Device-only.
//
// Bus discipline: the ES8311 is configured over the shared I2C bus, so audio_begin()
// MUST run on core 1 (setup), like the other I2C devices. Playback afterwards only
// touches the I2S peripheral + the PA GPIO (no I2C), so it runs in its own task.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum AudioCue {
    AUDIO_NEW   = 0,   // new aircraft entered range (soft cue)
    AUDIO_ALERT = 1,   // emergency / military contact (urgent cue)
};

// Alert sound character. All are synthesised on the fly — no samples in flash.
enum AudioPack {
    AUDIO_PACK_CHIME  = 0,   // two-tone cabin chime (default)
    AUDIO_PACK_SONAR  = 1,   // descending sonar ping
    AUDIO_PACK_PLUCK  = 2,   // short wooden marimba note
    AUDIO_PACK_WARN   = 3,   // cockpit-style master-caution warble
    AUDIO_PACK_BEEP   = 4,   // the original plain beep
    AUDIO_PACK_CUSTOM = 5,   // embedded PCM sample (see src/alert_sample.h)
    AUDIO_PACK_COUNT  = 6
};

// Where an uploaded sound lives (16 kHz signed 16-bit mono PCM, no header).
#define AUDIO_SAMPLE_PATH "/alert.pcm"

// A custom sound can come from two places: uploaded at runtime (LittleFS, wins) or
// compiled in via src/alert_sample.h. False means AUDIO_PACK_CUSTOM falls back to tones.
bool audio_has_sample();
size_t audio_sample_len();            // samples in the active custom sound (0 = none)
bool audio_load_sample();             // load the uploaded sound at boot; false if absent
void audio_adopt_sample(int16_t *pcm, size_t len);   // take ownership of a PSRAM buffer
void audio_clear_sample();            // delete the uploaded sound and revert to tones

bool audio_begin();                 // init ES8311 + I2S + PA + playback task (call on core 1)
bool audio_present();
void audio_set_volume(int pct);     // 0..100 (software amplitude)
void audio_set_muted(bool muted);
// Each cue carries its own sound, so a gentle cue for routine traffic can sit alongside
// something urgent for emergencies. `cue` is an AudioCue (0 = new contact, 1 = alert).
void audio_set_pack_for(int cue, int pack);
int  audio_pack_for(int cue);
void audio_play(AudioCue cue);      // non-blocking: signals the playback task
void audio_selftest();              // ~2 s continuous tone for by-ear verification
