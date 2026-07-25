#pragma once
// Browser WAV upload -> 16 kHz mono PCM in LittleFS (device-only).
//
// The web server hands us the body in ~1.4 KB chunks, so everything here is streaming:
// the RIFF header is parsed incrementally, then samples are downmixed and resampled on
// the fly and appended to /alert.pcm. Nothing larger than a chunk is ever buffered, so
// a 5 MB source WAV costs no RAM.
//
// Accepts uncompressed 16-bit PCM, mono or stereo, 8-48 kHz — the formats a normal
// "export as WAV" produces. Anything else is rejected with a specific reason rather
// than a generic failure, because "it didn't work" is useless when converting audio.
#include <stddef.h>
#include <stdint.h>

void        wav_upload_begin(void);
void        wav_upload_data(const uint8_t *data, size_t len);
bool        wav_upload_end(void);          // true on success; finalises and normalises
const char *wav_upload_error(void);        // human-readable reason when end() returns false
