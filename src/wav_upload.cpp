#include "wav_upload.h"
#include "audio.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

#define OUT_RATE      16000
#define OUT_MAX_SECS  4
#define OUT_MAX_LEN   (OUT_RATE * OUT_MAX_SECS)   // samples
#define TMP_PATH      "/alert.tmp"

// Header parsing runs as a small state machine because chunk boundaries fall wherever
// the network decides, not where the file structure would like them to.
enum WavState { W_RIFF, W_CHUNK_HDR, W_FMT, W_SKIP, W_DATA, W_DONE, W_ERROR };

static WavState s_state;
static uint8_t  s_hdr[40];
static size_t   s_hdrGot;
static uint32_t s_chunkLeft;
static char     s_chunkId[5];
static const char *s_err;

static uint16_t s_channels, s_bits;
static uint32_t s_rate;

// resampler state
static float    s_ratio;        // source samples per output sample
static float    s_nextOut;      // position (in source samples) of the next output sample
static int32_t  s_srcIndex;     // index of the most recent source sample
static int16_t  s_prev, s_cur;
static bool     s_haveFirst;

// per-frame accumulation for stereo downmix
static int32_t  s_frameAcc;
static int      s_frameCh;
static uint8_t  s_byteHi;
static bool     s_haveHi;

static File     s_file;
static uint32_t s_written;      // output samples written
static bool     s_truncated;

static inline uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void fail(const char *why) {
    s_err = why;
    s_state = W_ERROR;
    if (s_file) s_file.close();
}

void wav_upload_begin(void) {
    s_state = W_RIFF;
    s_hdrGot = 0;
    s_chunkLeft = 0;
    s_err = "";
    s_channels = s_bits = 0;
    s_rate = 0;
    s_nextOut = 0.0f;
    s_srcIndex = -1;
    s_prev = s_cur = 0;
    s_haveFirst = false;
    s_frameAcc = 0;
    s_frameCh = 0;
    s_haveHi = false;
    s_written = 0;
    s_truncated = false;
    LittleFS.remove(TMP_PATH);
    s_file = LittleFS.open(TMP_PATH, "w");
    if (!s_file) fail("could not open storage on the device");
}

// Emit output samples for the span between the previous and current source sample.
static void push_source_sample(int16_t v) {
    s_prev = s_cur;
    s_cur = v;
    ++s_srcIndex;
    if (!s_haveFirst) { s_haveFirst = true; s_prev = v; }

    // Linear interpolation between s_prev (index s_srcIndex-1) and s_cur (s_srcIndex).
    while (s_nextOut <= (float)s_srcIndex && s_written < OUT_MAX_LEN) {
        const float frac = s_nextOut - (float)(s_srcIndex - 1);
        const float f = (frac < 0.0f) ? 0.0f : (frac > 1.0f ? 1.0f : frac);
        const int32_t out = (int32_t)((float)s_prev * (1.0f - f) + (float)s_cur * f);
        int16_t o = (int16_t)(out < -32768 ? -32768 : (out > 32767 ? 32767 : out));
        s_file.write((const uint8_t *)&o, sizeof(o));
        ++s_written;
        s_nextOut += s_ratio;
    }
    if (s_written >= OUT_MAX_LEN) s_truncated = true;
}

// Feed one raw byte of the data chunk through downmix + resample.
static void push_data_byte(uint8_t b) {
    if (!s_haveHi) { s_byteHi = b; s_haveHi = true; return; }
    const int16_t sample = (int16_t)((uint16_t)s_byteHi | ((uint16_t)b << 8));
    s_haveHi = false;

    s_frameAcc += sample;
    if (++s_frameCh >= (int)s_channels) {
        push_source_sample((int16_t)(s_frameAcc / (int)s_channels));
        s_frameAcc = 0;
        s_frameCh = 0;
    }
}

void wav_upload_data(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (s_state == W_ERROR || s_state == W_DONE) return;
        const uint8_t b = data[i];

        switch (s_state) {
        case W_RIFF:
            s_hdr[s_hdrGot++] = b;
            if (s_hdrGot == 12) {
                if (memcmp(s_hdr, "RIFF", 4) != 0 || memcmp(s_hdr + 8, "WAVE", 4) != 0) {
                    fail("not a WAV file (missing RIFF/WAVE header)");
                    return;
                }
                s_hdrGot = 0;
                s_state = W_CHUNK_HDR;
            }
            break;

        case W_CHUNK_HDR:
            s_hdr[s_hdrGot++] = b;
            if (s_hdrGot == 8) {
                memcpy(s_chunkId, s_hdr, 4);
                s_chunkId[4] = 0;
                s_chunkLeft = rd32(s_hdr + 4);
                s_hdrGot = 0;
                if (memcmp(s_chunkId, "fmt ", 4) == 0) {
                    if (s_chunkLeft < 16) { fail("malformed WAV (short fmt chunk)"); return; }
                    s_state = W_FMT;
                } else if (memcmp(s_chunkId, "data", 4) == 0) {
                    if (!s_rate) { fail("malformed WAV (data before fmt)"); return; }
                    s_state = W_DATA;
                } else {
                    s_state = s_chunkLeft ? W_SKIP : W_CHUNK_HDR;
                }
            }
            break;

        case W_FMT:
            if (s_hdrGot < sizeof(s_hdr)) s_hdr[s_hdrGot] = b;
            ++s_hdrGot;
            if (--s_chunkLeft == 0) {
                const uint16_t fmt = rd16(s_hdr);
                s_channels = rd16(s_hdr + 2);
                s_rate     = rd32(s_hdr + 4);
                s_bits     = rd16(s_hdr + 14);
                // WAVE_FORMAT_EXTENSIBLE (0xFFFE) still carries PCM here; allow it.
                if (fmt != 1 && fmt != 0xFFFE) {
                    fail("compressed WAV - export as uncompressed PCM");
                    return;
                }
                if (s_bits != 16) { fail("need 16-bit WAV - re-export as 16-bit PCM"); return; }
                if (s_channels < 1 || s_channels > 2) { fail("need mono or stereo audio"); return; }
                if (s_rate < 8000 || s_rate > 48000) { fail("sample rate must be 8-48 kHz"); return; }
                s_ratio = (float)s_rate / (float)OUT_RATE;
                s_hdrGot = 0;
                s_state = W_CHUNK_HDR;
                Serial.printf("[wav] %u Hz, %u ch, %u-bit\n",
                              (unsigned)s_rate, (unsigned)s_channels, (unsigned)s_bits);
            }
            break;

        case W_SKIP:
            if (--s_chunkLeft == 0) s_state = W_CHUNK_HDR;
            break;

        case W_DATA:
            push_data_byte(b);
            if (s_chunkLeft && --s_chunkLeft == 0) s_state = W_DONE;
            break;

        default:
            return;
        }
    }
}

bool wav_upload_end(void) {
    if (s_state == W_ERROR) return false;
    if (s_file) s_file.close();

    if (s_written == 0) {
        s_err = "no audio found in the file";
        LittleFS.remove(TMP_PATH);
        return false;
    }

    // Second pass: normalise. Peak can't be known while streaming, and a quiet source
    // would otherwise produce an alert nobody hears. The file is <=128 KB, so read it
    // back through PSRAM, scale, and write the final result.
    const size_t bytes = (size_t)s_written * sizeof(int16_t);
    int16_t *pcm = (int16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!pcm) { s_err = "not enough memory to finish"; LittleFS.remove(TMP_PATH); return false; }

    File in = LittleFS.open(TMP_PATH, "r");
    if (!in || in.read((uint8_t *)pcm, bytes) != (int)bytes) {
        if (in) in.close();
        heap_caps_free(pcm);
        s_err = "could not read back the converted audio";
        LittleFS.remove(TMP_PATH);
        return false;
    }
    in.close();

    int32_t peak = 0;
    for (size_t i = 0; i < s_written; ++i) {
        const int32_t a = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (a > peak) peak = a;
    }
    if (peak > 0) {
        const float gain = (32767.0f * 0.89f) / (float)peak;   // headroom so the amp won't clip
        for (size_t i = 0; i < s_written; ++i) {
            int32_t v = (int32_t)(pcm[i] * gain);
            pcm[i] = (int16_t)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
        }
    }
    // Fade the last 10 ms so a truncated clip cannot end on a click.
    const size_t fade = OUT_RATE / 100;
    if (s_written > fade)
        for (size_t i = 0; i < fade; ++i)
            pcm[s_written - 1 - i] = (int16_t)((int32_t)pcm[s_written - 1 - i] * (int32_t)i / (int32_t)fade);

    File out = LittleFS.open(AUDIO_SAMPLE_PATH, "w");
    if (!out) { heap_caps_free(pcm); s_err = "could not save the sound"; LittleFS.remove(TMP_PATH); return false; }
    const size_t wrote = out.write((const uint8_t *)pcm, bytes);
    out.close();
    LittleFS.remove(TMP_PATH);

    if (wrote != bytes) {
        heap_caps_free(pcm);
        LittleFS.remove(AUDIO_SAMPLE_PATH);
        s_err = "ran out of space on the device";
        return false;
    }

    // Hand the buffer straight to the audio layer — no need to read it back again.
    audio_adopt_sample(pcm, s_written);
    Serial.printf("[wav] saved %u samples (%.2fs)%s\n", (unsigned)s_written,
                  s_written / (double)OUT_RATE, s_truncated ? " [truncated]" : "");
    return true;
}

const char *wav_upload_error(void) { return s_err && s_err[0] ? s_err : "upload failed"; }
