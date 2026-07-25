// Airline logo for the detail card.
//   source : images.kiwi.com/airlines/{IATA}.png — free, no API key, transparent PNG
//   proxy  : images.weserv.nl re-encodes to a baseline JPEG at our canvas size and
//            flattens the alpha onto the card colour (TJpgDec handles neither PNG nor
//            progressive JPEG, and this is the same trick photo_client already uses).
// Device-only. All network runs on core 0; the UI just reads the decoded buffer.
#include "airline_client.h"
#include "airline.h"
#include "net_fetch.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>

#define AL_UA "CapsuleRadar/1.0 (+https://github.com/socquique/capsule-radar)"
#define AL_CARD_BG "0C160F"   // UI_PANEL in ui.cpp — the logo is flattened onto this

static lv_color_t *s_dst = nullptr;
static int s_dstW = 0, s_dstH = 0;

static bool logo_out(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bmp) {
    for (int j = 0; j < h; ++j) {
        const int yy = y + j;
        if (yy < 0 || yy >= s_dstH) continue;
        for (int i = 0; i < w; ++i) {
            const int xx = x + i;
            if (xx < 0 || xx >= s_dstW) continue;
            s_dst[yy * s_dstW + xx].full = bmp[j * w + i];
        }
    }
    return true;
}

bool airline_logo_fetch(const char *iata) {
    if (!iata || !iata[0] || WiFi.status() != WL_CONNECTED) {
        airline_logo_commit(0, 0, iata);
        return false;
    }
    // Same guard as photo_fetch: a TLS handshake plus a JPEG decode needs a contiguous
    // internal block. If memory is tight, skip rather than risk the live feed.
    if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < 28000) {
        Serial.println("[logo] low memory, skipping");
        airline_logo_commit(0, 0, iata);
        return false;
    }

    int maxW = 0, maxH = 0;
    lv_color_t *dst = airline_logo_buffer(&maxW, &maxH);
    if (!dst) { airline_logo_commit(0, 0, iata); return false; }

    char url[224];
    snprintf(url, sizeof(url),
             "https://images.weserv.nl/?url=images.kiwi.com%%2Fairlines%%2F128%%2F%s.png"
             "&w=%d&h=%d&fit=inside&output=jpg&bg=%s",
             iata, maxW, maxH, AL_CARD_BG);

    uint8_t *img = nullptr; size_t ilen = 0;
    if (!net_fetch_psram(url, AL_UA, &img, &ilen, 32768, 3000, 6000)) {
        Serial.printf("[logo] %s: download failed\n", iata);
        airline_logo_commit(0, 0, iata);
        return false;
    }

    uint16_t jw = 0, jh = 0;
    if (TJpgDec.getJpgSize(&jw, &jh, img, ilen) != JDR_OK || jw == 0 || jh == 0) {
        Serial.printf("[logo] %s: getJpgSize failed\n", iata);
        heap_caps_free(img);
        airline_logo_commit(0, 0, iata);
        return false;
    }
    uint8_t scale = 1;
    while ((jw / scale) > (uint16_t)maxW || (jh / scale) > (uint16_t)maxH) {
        scale <<= 1;
        if (scale >= 8) break;
    }
    s_dstW = (int)(jw / scale); if (s_dstW > maxW) s_dstW = maxW;
    s_dstH = (int)(jh / scale); if (s_dstH > maxH) s_dstH = maxH;
    s_dst = dst;
    for (int i = 0; i < s_dstW * s_dstH; ++i) s_dst[i].full = 0;

    TJpgDec.setJpgScale(scale);
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(logo_out);
    const JRESULT jr = TJpgDec.drawJpg(0, 0, img, ilen);
    heap_caps_free(img);

    if (jr != JDR_OK) { airline_logo_commit(0, 0, iata); return false; }
    airline_logo_commit(s_dstW, s_dstH, iata);
    Serial.printf("[logo] %s: %dx%d\n", iata, s_dstW, s_dstH);
    return true;
}
