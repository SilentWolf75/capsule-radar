// ES8311 codec "ping" generator. See audio.h for the core/bus discipline.
//
// Uses the IDF I2S driver directly (the Arduino ESP_I2S wrapper hit an IRAM-safe
// GDMA/interrupt mismatch in the precompiled libs: "Register tx callback failed").
// The ES8311 register init below is the canonical DAC-playback sequence; if the
// speaker stays silent, cross-check it against the Waveshare 08_ES8311 demo — only
// that table is board-specific, the rest is independent.
#include "audio.h"
#include "alert_sample.h"
#include "config.h"
#include <LittleFS.h>
#include <Arduino.h>
#include <Wire.h>
#include "driver/i2s.h"
#include "esp_heap_caps.h"
#include <math.h>

#define ES8311_ADDR   0x18
#define SR            16000          // playback sample rate (a beep; pitch-tolerant)
#define I2S_PORT      I2S_NUM_0

static bool s_ok = false;
static int16_t *s_buf = nullptr;     // tone scratch in PSRAM (keeps internal RAM free for TLS)
static const size_t S_BUF_LEN = SR / 2 * 2;   // up to 500 ms, stereo interleaved
static volatile int  s_vol = 60;     // 0..100
static volatile bool s_muted = false;
static volatile int  s_cue = -1;
static volatile int  s_packs[2] = { AUDIO_PACK_CHIME, AUDIO_PACK_CHIME };  // [new, alert]
// Uploaded sound, loaded into PSRAM. Takes precedence over the compiled-in array so a
// user can replace the sound from the web page without rebuilding the firmware.
static int16_t      *s_userPcm = nullptr;
static size_t        s_userLen = 0;
static SemaphoreHandle_t s_sem = nullptr;

static void es_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}
static uint8_t es_read(uint8_t reg) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    if (Wire.requestFrom((int)ES8311_ADDR, 1) != 1) return 0;
    return Wire.read();
}

static void es_update(uint8_t reg, uint8_t andmask, uint8_t ormask) {
    es_write(reg, (uint8_t)((es_read(reg) & andmask) | ormask));
}

// ES8311 init replicated faithfully from the vendor esp_codec_dev driver for this
// exact board: DAC playback, I2S slave, external MCLK = 256*fs, fs = 16 kHz, 16-bit,
// internal reference (ADCL+DACR). Mirrors es8311_open + config_sample + config_fmt +
// set_bits + start + volume/unmute.
static void es8311_init() {
    // --- open() ---
    es_write(0x0D, 0xFA);                 // power up system
    es_write(0x44, 0x08);                 // (written twice: ES8311 first-write quirk)
    es_write(0x44, 0x08);
    es_write(0x01, 0x30);
    es_write(0x02, 0x00);
    es_write(0x03, 0x10);
    es_write(0x16, 0x24);
    es_write(0x04, 0x10);
    es_write(0x05, 0x00);
    es_write(0x0B, 0x00);
    es_write(0x0C, 0x00);
    es_write(0x10, 0x1F);
    es_write(0x11, 0x7F);
    es_write(0x00, 0x80);                 // reset csm/clock, slave mode
    es_write(0x00, 0x80);                 // slave: bit6=0
    es_write(0x01, 0xBF);                 // clk src = SCLK/BCLK-derived (no external MCLK needed)
    es_update(0x06, (uint8_t)~0x20, 0x00); // SCLK not inverted
    es_write(0x13, 0x10);
    es_write(0x1B, 0x0A);
    es_write(0x1C, 0x6A);
    es_write(0x44, 0x58);                 // internal reference (ADCL + DACR) -> drives DAC

    // --- config_sample(): MCLK 4.096 MHz / 16 kHz coeff {pre=1,mult=1,adc=1,dac=1,osr 0x10/0x20,lrck 0xFF,bclk 4} ---
    es_write(0x02, 0x18);                 // pre_div=1, pre_multi=8 (BCLK*8 = DIG_MCLK, use_mclk=false)
    es_write(0x05, 0x00);                 // adc_div=1, dac_div=1
    es_write(0x03, 0x10);                 // fs_mode=0, adc_osr=0x10
    es_write(0x04, 0x20);                 // dac_osr=0x20
    es_update(0x07, 0xC0, 0x00);          // lrck_h=0
    es_write(0x08, 0xFF);                 // lrck_l=0xFF
    es_update(0x06, 0xE0, 0x03);          // bclk_div=4 (preserves SCLK-invert bit)

    // --- config_fmt(NORMAL) + set_bits(16) -> SDP in/out = standard I2S, 16-bit ---
    es_write(0x09, 0x0C);
    es_write(0x0A, 0x0C);

    // --- start() (DAC, slave) ---
    es_write(0x00, 0x80);
    es_write(0x01, 0xBF);                 // keep BCLK-derived clock
    es_write(0x09, 0x0C);                 // DAC iface enabled (bit6=0)
    es_write(0x0A, 0x0C);
    es_write(0x17, 0xBF);
    es_write(0x0E, 0x02);
    es_write(0x12, 0x00);                 // enable DAC
    es_write(0x14, 0x1A);
    es_write(0x0D, 0x01);
    es_write(0x15, 0x40);
    es_write(0x37, 0x08);
    es_write(0x45, 0x00);

    // --- volume + unmute ---
    es_write(0x32, 0xBF);                 // DAC volume ~0 dB
    es_update(0x31, 0x9F, 0x00);          // unmute DAC
}

static bool i2s_setup() {
    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = SR;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;       // stereo
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    cfg.bits_per_chan = I2S_BITS_PER_CHAN_16BIT;
    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

    i2s_pin_config_t pins = {};
    pins.mck_io_num   = PIN_I2S_MCLK;
    pins.bck_io_num   = PIN_I2S_BCLK;
    pins.ws_io_num    = PIN_I2S_LRCLK;
    pins.data_out_num = PIN_I2S_DOUT;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;
    if (i2s_set_pin(I2S_PORT, &pins) != ESP_OK) { i2s_driver_uninstall(I2S_PORT); return false; }
    i2s_zero_dma_buffer(I2S_PORT);
    return true;
}

// Synthesize one beep (freq Hz, ms) with a short fade in/out, into a stereo buffer.
static size_t gen_beep(int16_t *buf, size_t cap, float freq, int ms, float amp) {
    const size_t n = (size_t)((long)SR * ms / 1000);
    const size_t fade = SR / 200;                 // ~5 ms ramps (anti-click)
    size_t i = 0;
    for (; i < n && (i * 2 + 1) < cap; ++i) {
        float env = 1.0f;
        if (i < fade)            env = (float)i / fade;
        else if (i > n - fade)   env = (float)(n - i) / fade;
        const int16_t s = (int16_t)(amp * env * sinf(2.0f * (float)M_PI * freq * i / SR));
        buf[i * 2] = s; buf[i * 2 + 1] = s;       // L = R
    }
    return i * 2;                                  // samples written (stereo interleaved)
}

// Synthesize a musical note: pitch glides f0 -> f1, amplitude decays exponentially,
// and h2/h3 mix in the 2nd/3rd harmonics. That envelope is what stops it sounding like
// a beep — a plain tone holds a flat level, a struck/plucked sound starts loud and
// falls away. Phase is accumulated (not computed from f*i) so the glide stays smooth.
// True peak of sin(x) + h2*sin(2x) + h3*sin(3x) over one period. Dividing by
// (1 + h2 + h3) instead — the naive guess — assumes every harmonic peaks at the same
// instant, which they never do. That made harmonically rich packs up to twice as quiet
// as the plain beep, so Marimba and Aircraft-warning sounded broken rather than just
// different. Sampling the actual waveform equalises perceived loudness across packs.
static float wave_peak(float h2, float h3) {
    float peak = 0.0f;
    for (int k = 0; k < 64; ++k) {
        const float x = 2.0f * (float)M_PI * (float)k / 64.0f;
        const float v = fabsf(sinf(x) + h2 * sinf(2.0f * x) + h3 * sinf(3.0f * x));
        if (v > peak) peak = v;
    }
    return (peak > 0.01f) ? peak : 1.0f;
}

static size_t gen_note(int16_t *buf, size_t cap, float f0, float f1, int ms,
                       float amp, float h2, float h3, float decay) {
    const size_t n = (size_t)((long)SR * ms / 1000);
    if (n == 0) return 0;
    const size_t atk = SR / 400;                  // ~2.5 ms attack (anti-click)
    const size_t rel = SR / 300;                  // ~3 ms release to guarantee zero at the end
    const float norm = 1.0f / wave_peak(h2, h3);
    float phase = 0.0f;
    size_t i = 0;
    for (; i < n && (i * 2 + 1) < cap; ++i) {
        const float t = (float)i / (float)n;
        const float f = f0 + (f1 - f0) * t;
        phase += 2.0f * (float)M_PI * f / (float)SR;
        if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;

        float env = expf(-decay * t);
        if (i < atk)            env *= (float)i / (float)atk;
        else if (i > n - rel)   env *= (float)(n - i) / (float)rel;

        const float w = sinf(phase) + h2 * sinf(2.0f * phase) + h3 * sinf(3.0f * phase);
        const int16_t s = (int16_t)(amp * env * norm * w);
        buf[i * 2] = s; buf[i * 2 + 1] = s;
    }
    return i * 2;
}

// One note of a cue: frequency glide, length, harmonic mix, decay rate and the gap
// that follows it. A cue is up to three of these played back to back.
struct Note { float f0, f1; int ms; float h2, h3, decay; int gapMs; };

// Sound packs. Indexed [pack][cue], cue 0 = new contact, 1 = alert.
static const struct { Note n[6]; uint8_t count; } PACKS[AUDIO_PACK_COUNT][2] = {
    // --- CHIME: the familiar two-tone cabin chime; alert adds a third, urgent note ---
    { { { {784.0f, 784.0f, 190, 0.35f, 0.08f, 3.0f, 10},
          {1047.0f, 1047.0f, 340, 0.30f, 0.06f, 2.4f, 0} }, 2 },
      { { {1047.0f, 1047.0f, 130, 0.30f, 0.10f, 3.6f, 25},
          {784.0f,  784.0f,  130, 0.30f, 0.10f, 3.6f, 25},
          {1047.0f, 1047.0f, 260, 0.30f, 0.10f, 2.8f, 0} }, 3 } },

    // --- SONAR: descending ping with a long tail, the classic radar echo ---
    { { { {1150.0f, 880.0f, 380, 0.14f, 0.03f, 3.2f, 0} }, 1 },
      { { {1400.0f, 1000.0f, 240, 0.16f, 0.04f, 3.4f, 80},
          {1400.0f, 1000.0f, 320, 0.16f, 0.04f, 3.0f, 0} }, 2 } },

    // --- PLUCK: wooden marimba note. Decay eased from 5.5 to 3.4 — at 5.5 the note was
    // down 99% within ~80 ms, which on this speaker read as silence rather than "short".
    { { { {880.0f, 880.0f, 320, 0.50f, 0.25f, 3.4f, 0} }, 1 },
      { { {1174.0f, 1174.0f, 200, 0.50f, 0.25f, 3.8f, 30},
          {1568.0f, 1568.0f, 340, 0.50f, 0.25f, 3.2f, 0} }, 2 } },

    // --- WARN: cockpit master-caution character. Heavy 2nd/3rd harmonics make it buzzy
    // rather than pure, and the near-zero decay holds each tone flat so the hi-lo warble
    // reads as urgent. New contacts get a single short blip; only real alerts warble,
    // otherwise every passing airliner would sound like an emergency.
    { { { {1000.0f, 1000.0f, 220, 0.60f, 0.35f, 0.5f, 0} }, 1 },
      { { {1000.0f, 1000.0f, 115, 0.60f, 0.40f, 0.3f, 12},
          {750.0f,  750.0f,  115, 0.60f, 0.40f, 0.3f, 12},
          {1000.0f, 1000.0f, 115, 0.60f, 0.40f, 0.3f, 12},
          {750.0f,  750.0f,  115, 0.60f, 0.40f, 0.3f, 12},
          {1000.0f, 1000.0f, 150, 0.60f, 0.40f, 0.4f, 0} }, 5 } },

    // --- BEEP: the original flat tones, kept for anyone who preferred them ---
    { { { {880.0f, 880.0f, 160, 0.0f, 0.0f, 0.0f, 0} }, 1 },
      { { {1320.0f, 1320.0f, 80, 0.0f, 0.0f, 0.0f, 40},
          {1320.0f, 1320.0f, 80, 0.0f, 0.0f, 0.0f, 0} }, 2 } },
};

static void play_cue(int cue) {
    if (!s_ok || !s_buf || (s_muted && cue != 2) || s_vol <= 0) return;
    int16_t *buf = s_buf;
    const float amp = (s_vol / 100.0f) * 17000.0f;
    digitalWrite(PIN_AUDIO_PA, HIGH);              // enable speaker amp
    delay(8);                                      // let the amp power up
    size_t bw;
    if (cue == 2) {                                // self-test: ~2 s continuous tone, PA held
        size_t ns = gen_beep(buf, S_BUF_LEN, 1000.0f, 480, amp);
        for (int k = 0; k < 4; ++k) i2s_write(I2S_PORT, buf, ns * 2, &bw, portMAX_DELAY);
    } else if (s_packs[(cue == AUDIO_ALERT) ? 1 : 0] == AUDIO_PACK_CUSTOM && audio_has_sample()) {
        // An uploaded sound wins over the compiled-in one. Both are mono while the I2S
        // stream is stereo, so frames are duplicated into the scratch buffer in chunks
        // rather than keeping a stereo copy around.
        const int16_t *pcm = s_userPcm ? s_userPcm : ALERT_SAMPLE;
        const size_t   len = s_userPcm ? s_userLen : (size_t)ALERT_SAMPLE_LEN;
        Serial.printf("[audio] cue=%d pack=custom(%s) sample=%u frames (%.2fs)\n",
                      cue, s_userPcm ? "uploaded" : "built-in",
                      (unsigned)len, len / (double)ALERT_SAMPLE_RATE);
        const float gain = s_vol / 100.0f;
        size_t pos = 0;
        while (pos < len) {
            size_t frames = len - pos;
            if (frames > S_BUF_LEN / 2) frames = S_BUF_LEN / 2;
            for (size_t i = 0; i < frames; ++i) {
                const int16_t v = (int16_t)(pcm[pos + i] * gain);
                buf[i * 2] = v; buf[i * 2 + 1] = v;
            }
            i2s_write(I2S_PORT, buf, frames * 2 * sizeof(int16_t), &bw, portMAX_DELAY);
            pos += frames;
        }
    } else {
        const int ci = (cue == AUDIO_ALERT) ? 1 : 0;
        int pk = s_packs[ci];
        if (pk < 0 || pk >= AUDIO_PACK_COUNT) pk = AUDIO_PACK_CHIME;
        if (pk == AUDIO_PACK_CUSTOM) pk = AUDIO_PACK_WARN;   // no sample installed
        const auto &seq = PACKS[pk][ci];
        Serial.printf("[audio] cue=%d pack=%d notes=%d first=%.0fHz decay=%.1f\n",
                      cue, pk, (int)seq.count, (double)seq.n[0].f0, (double)seq.n[0].decay);
        for (int k = 0; k < seq.count; ++k) {
            const Note &nt = seq.n[k];
            size_t ns;
            if (nt.decay <= 0.0f)   // flat tone: the original beep envelope
                ns = gen_beep(buf, S_BUF_LEN, nt.f0, nt.ms, amp);
            else
                ns = gen_note(buf, S_BUF_LEN, nt.f0, nt.f1, nt.ms, amp, nt.h2, nt.h3, nt.decay);
            i2s_write(I2S_PORT, buf, ns * 2, &bw, portMAX_DELAY);
            if (nt.gapMs) delay(nt.gapMs);
        }
    }
    delay(90);                                     // let the DMA clock the tail out before cutting the amp
    digitalWrite(PIN_AUDIO_PA, LOW);               // mute amp between pings (saves power, kills hiss)
}

static void audio_task(void *) {
    for (;;) {
        if (xSemaphoreTake(s_sem, portMAX_DELAY) == pdTRUE) play_cue(s_cue);
    }
}

bool audio_begin() {
    pinMode(PIN_AUDIO_PA, OUTPUT);
    digitalWrite(PIN_AUDIO_PA, LOW);

    const uint8_t id1 = es_read(0xFD), id2 = es_read(0xFE);   // expect 0x83, 0x11
    if (id1 != 0x83) {
        Serial.printf("[audio] ES8311 not found (id=0x%02X 0x%02X)\n", id1, id2);
        s_ok = false;
        return false;
    }
    es8311_init();

    const uint8_t dr[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x09,0x0A,0x0D,0x0E,0x12,0x14,0x31,0x32,0x37,0x44};
    Serial.print("[audio] ES8311 regs:");
    for (uint8_t r : dr) Serial.printf(" %02X=%02X", r, es_read(r));
    Serial.println();

    if (!i2s_setup()) {
        Serial.println("[audio] I2S init failed");
        s_ok = false;
        return false;
    }
    s_buf = (int16_t *)heap_caps_malloc(S_BUF_LEN * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    if (!s_buf) {
        Serial.println("[audio] tone buffer alloc failed");
        s_ok = false;
        return false;
    }
    s_sem = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, nullptr, 1, nullptr, 0);  // I2S only -> core 0
    s_ok = true;
    Serial.println("[audio] ES8311 ready");
    return true;
}

bool audio_present() { return s_ok; }
void audio_set_volume(int pct) { s_vol = constrain(pct, 0, 100); }
void audio_set_muted(bool m) { s_muted = m; }
void audio_set_pack_for(int cue, int pack) {
    const int c = (cue == AUDIO_ALERT) ? 1 : 0;
    s_packs[c] = (pack >= 0 && pack < AUDIO_PACK_COUNT) ? pack : AUDIO_PACK_CHIME;
}
int audio_pack_for(int cue) { return s_packs[(cue == AUDIO_ALERT) ? 1 : 0]; }
bool   audio_has_sample() { return s_userLen > 0 || ALERT_SAMPLE_LEN > 0; }
size_t audio_sample_len() { return s_userLen > 0 ? s_userLen : (size_t)ALERT_SAMPLE_LEN; }

void audio_adopt_sample(int16_t *pcm, size_t len) {
    int16_t *old = s_userPcm;
    s_userPcm = nullptr;          // stop playback reading it while we swap
    s_userLen = 0;
    if (old) heap_caps_free(old);
    s_userPcm = pcm;
    s_userLen = len;
}

bool audio_load_sample() {
    if (!LittleFS.exists(AUDIO_SAMPLE_PATH)) return false;
    File f = LittleFS.open(AUDIO_SAMPLE_PATH, "r");
    if (!f) return false;
    const size_t bytes = f.size();
    if (bytes < 2 || bytes > 16000 * 2 * 8) { f.close(); return false; }   // sanity: <=8 s
    int16_t *pcm = (int16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!pcm) { f.close(); return false; }
    const size_t got = f.read((uint8_t *)pcm, bytes);
    f.close();
    if (got != bytes) { heap_caps_free(pcm); return false; }
    audio_adopt_sample(pcm, bytes / sizeof(int16_t));
    Serial.printf("[audio] loaded uploaded sound: %u samples (%.2fs)\n",
                  (unsigned)s_userLen, s_userLen / (double)ALERT_SAMPLE_RATE);
    return true;
}

void audio_clear_sample() {
    int16_t *old = s_userPcm;
    s_userPcm = nullptr;
    s_userLen = 0;
    if (old) heap_caps_free(old);
    LittleFS.remove(AUDIO_SAMPLE_PATH);
}

void audio_play(AudioCue cue) {
    if (!s_ok || s_muted) return;
    s_cue = (int)cue;
    if (s_sem) xSemaphoreGive(s_sem);
}

void audio_selftest() {   // ~2 s continuous tone, ignores mute, PA held on
    if (!s_ok) return;
    s_cue = 2;
    if (s_sem) xSemaphoreGive(s_sem);
}
