#pragma once
// Stand-ins for the handful of Arduino facilities the portable UI touches, so ui.cpp and
// radar_view.cpp compile for the native simulator without being peppered with #ifdefs at
// every use site.
//
// This exists because the simulator had quietly rotted: nothing in CI ever built it, so
// a WiFi.h include and a few millis()/micros() calls crept into the "portable" layer and
// broke the native target without anyone noticing. The screenshot job builds it on every
// push now, which is what keeps this file honest.
#if !defined(ARDUINO)

#include <stdint.h>
#include <string>
#include <chrono>

static inline uint32_t millis() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - t0).count();
}

static inline uint32_t micros() {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return (uint32_t)duration_cast<microseconds>(steady_clock::now() - t0).count();
}

// The simulator has no radio. Reporting "not connected" is the honest answer, and it
// makes the Stats and About screens render their offline state deterministically --
// which matters, because those screens are compared pixel-for-pixel in CI.
enum { WL_CONNECTED = 3 };

struct NativeIP {
    std::string toString() const { return std::string("0.0.0.0"); }
};

struct NativeWiFi {
    int      status()   const { return 0; }          // never WL_CONNECTED
    int      RSSI()     const { return 0; }
    NativeIP localIP()  const { return NativeIP(); }
};

static NativeWiFi WiFi;

// PSRAM allocation. There is no PSRAM here, and plain malloc has the same contract, so
// the capability flag is simply ignored.
#include <stdlib.h>
enum { MALLOC_CAP_SPIRAM = 1, MALLOC_CAP_8BIT = 2, MALLOC_CAP_INTERNAL = 4 };
static inline void *heap_caps_malloc(size_t n, int /*caps*/) { return malloc(n); }
static inline void  heap_caps_free(void *p) { free(p); }

// Chip identity for the About screen. Named for what it is rather than impersonating a
// board, so a simulator screenshot can never be mistaken for one taken off hardware.
struct NativeESP {
    const char *getChipModel()     const { return "native-sim"; }
    uint32_t    getPsramSize()     const { return 0; }
    uint32_t    getFlashChipSize() const { return 0; }
    uint32_t    getCpuFreqMHz()    const { return 0; }
};

static NativeESP ESP;

#endif  // !ARDUINO
