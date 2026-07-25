#include "weather_icons.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// WMO 4677 groups, collapsed to what is worth distinguishing at ~34 px.
enum IconKind { IC_CLEAR, IC_PARTLY, IC_CLOUD, IC_FOG, IC_DRIZZLE, IC_RAIN, IC_SNOW, IC_STORM };

#define COL_SUN   lv_color_hex(0xFFC24D)
#define COL_MOON  lv_color_hex(0xE6EDF5)
#define COL_CLOUD lv_color_hex(0xC2D2E4)
#define COL_RAIN  lv_color_hex(0x4DDCFF)
#define COL_SNOW  lv_color_hex(0xEAF6FF)
#define COL_BOLT  lv_color_hex(0xFFD84D)

static IconKind kind_for(int code) {
    if (code <= 0)  return IC_CLEAR;
    if (code <= 2)  return IC_PARTLY;
    if (code == 3)  return IC_CLOUD;
    if (code >= 45 && code <= 48) return IC_FOG;
    if (code >= 51 && code <= 57) return IC_DRIZZLE;
    if (code >= 61 && code <= 67) return IC_RAIN;
    if (code >= 71 && code <= 77) return IC_SNOW;
    if (code >= 80 && code <= 82) return IC_RAIN;
    if (code >= 85 && code <= 86) return IC_SNOW;
    if (code >= 95) return IC_STORM;
    return IC_CLOUD;
}

static void fill_circle(lv_draw_ctx_t *d, lv_coord_t cx, lv_coord_t cy, lv_coord_t r,
                        lv_color_t c, lv_opa_t opa) {
    lv_draw_rect_dsc_t s;
    lv_draw_rect_dsc_init(&s);
    s.bg_color = c; s.bg_opa = opa; s.radius = LV_RADIUS_CIRCLE;
    lv_area_t a = { (lv_coord_t)(cx - r), (lv_coord_t)(cy - r),
                    (lv_coord_t)(cx + r), (lv_coord_t)(cy + r) };
    lv_draw_rect(d, &s, &a);
}

static void line(lv_draw_ctx_t *d, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2, lv_coord_t y2,
                 lv_color_t c, lv_coord_t w, lv_opa_t opa) {
    lv_draw_line_dsc_t s;
    lv_draw_line_dsc_init(&s);
    s.color = c; s.width = w; s.opa = opa; s.round_start = 1; s.round_end = 1;
    lv_point_t p1 = { x1, y1 }, p2 = { x2, y2 };
    lv_draw_line(d, &s, &p1, &p2);
}

// A cloud: three lobes over a slab, so the silhouette reads even at small sizes.
static void draw_cloud(lv_draw_ctx_t *d, lv_coord_t cx, lv_coord_t cy, lv_coord_t s,
                       lv_color_t col, lv_opa_t opa) {
    fill_circle(d, (lv_coord_t)(cx - s / 3), cy, (lv_coord_t)(s / 3), col, opa);
    fill_circle(d, (lv_coord_t)(cx + s / 4), cy, (lv_coord_t)(s / 4), col, opa);
    fill_circle(d, cx, (lv_coord_t)(cy - s / 4), (lv_coord_t)(s / 3), col, opa);
    lv_draw_rect_dsc_t slab;
    lv_draw_rect_dsc_init(&slab);
    slab.bg_color = col; slab.bg_opa = opa; slab.radius = (lv_coord_t)(s / 6);
    lv_area_t a = { (lv_coord_t)(cx - s / 2), cy,
                    (lv_coord_t)(cx + s / 2), (lv_coord_t)(cy + s / 3) };
    lv_draw_rect(d, &slab, &a);
}

static void draw_sun(lv_draw_ctx_t *d, lv_coord_t cx, lv_coord_t cy, lv_coord_t r, bool night) {
    if (night) {
        // Crescent: a disc with a second disc punched out by overdrawing in black.
        fill_circle(d, cx, cy, r, COL_MOON, LV_OPA_COVER);
        fill_circle(d, (lv_coord_t)(cx + r / 2), (lv_coord_t)(cy - r / 3),
                    (lv_coord_t)(r * 0.85f), lv_color_black(), LV_OPA_COVER);
        return;
    }
    fill_circle(d, cx, cy, r, COL_SUN, LV_OPA_COVER);
    for (int i = 0; i < 8; ++i) {
        const float a = (float)i * (float)M_PI / 4.0f;
        const float c = cosf(a), s = sinf(a);
        line(d, (lv_coord_t)(cx + c * (r + 3)), (lv_coord_t)(cy + s * (r + 3)),
                (lv_coord_t)(cx + c * (r + 7)), (lv_coord_t)(cy + s * (r + 7)),
             COL_SUN, 2, 220);
    }
}

static void icon_draw_cb(lv_event_t *e) {
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *d = lv_event_get_draw_ctx(e);
    const uint32_t packed = (uint32_t)(uintptr_t)lv_obj_get_user_data(o);
    const int code = (int)(packed & 0xFFFF);
    const bool night = (packed & 0x10000) != 0;
    if (code < 0) return;

    lv_area_t box;
    lv_obj_get_coords(o, &box);
    const lv_coord_t w = lv_area_get_width(&box), h = lv_area_get_height(&box);
    const lv_coord_t cx = (lv_coord_t)(box.x1 + w / 2), cy = (lv_coord_t)(box.y1 + h / 2);
    const lv_coord_t s = (lv_coord_t)((w < h ? w : h));      // nominal size

    switch (kind_for(code)) {
    case IC_CLEAR:
        draw_sun(d, cx, cy, (lv_coord_t)(s / 4), night);
        break;
    case IC_PARTLY:
        draw_sun(d, (lv_coord_t)(cx + s / 5), (lv_coord_t)(cy - s / 5), (lv_coord_t)(s / 6), night);
        draw_cloud(d, (lv_coord_t)(cx - s / 10), (lv_coord_t)(cy + s / 10), (lv_coord_t)(s * 0.62f),
                   COL_CLOUD, LV_OPA_COVER);
        break;
    case IC_CLOUD:
        draw_cloud(d, cx, cy, (lv_coord_t)(s * 0.72f), COL_CLOUD, LV_OPA_COVER);
        break;
    case IC_FOG:
        draw_cloud(d, cx, (lv_coord_t)(cy - s / 8), (lv_coord_t)(s * 0.62f), COL_CLOUD, 210);
        for (int i = 0; i < 3; ++i) {
            const lv_coord_t y = (lv_coord_t)(cy + s / 4 + i * (s / 8));
            line(d, (lv_coord_t)(cx - s / 3), y, (lv_coord_t)(cx + s / 3), y, COL_CLOUD, 2, 190);
        }
        break;
    case IC_DRIZZLE:
    case IC_RAIN: {
        draw_cloud(d, cx, (lv_coord_t)(cy - s / 6), (lv_coord_t)(s * 0.62f), COL_CLOUD, LV_OPA_COVER);
        const int n = (kind_for(code) == IC_DRIZZLE) ? 2 : 3;
        for (int i = 0; i < n; ++i) {
            const lv_coord_t x = (lv_coord_t)(cx - s / 5 + i * (s / 5));
            line(d, x, (lv_coord_t)(cy + s / 5), (lv_coord_t)(x - s / 12), (lv_coord_t)(cy + s / 2.4f),
                 COL_RAIN, 2, 235);
        }
        break;
    }
    case IC_SNOW:
        draw_cloud(d, cx, (lv_coord_t)(cy - s / 6), (lv_coord_t)(s * 0.62f), COL_CLOUD, LV_OPA_COVER);
        for (int i = 0; i < 3; ++i) {
            const lv_coord_t x = (lv_coord_t)(cx - s / 5 + i * (s / 5));
            const lv_coord_t y = (lv_coord_t)(cy + s / 3);
            fill_circle(d, x, y, 2, COL_SNOW, LV_OPA_COVER);
        }
        break;
    case IC_STORM: {
        draw_cloud(d, cx, (lv_coord_t)(cy - s / 6), (lv_coord_t)(s * 0.62f), COL_CLOUD, LV_OPA_COVER);
        lv_point_t bolt[4] = {
            { (lv_coord_t)(cx + s / 12), (lv_coord_t)(cy + s / 8) },
            { (lv_coord_t)(cx - s / 8),  (lv_coord_t)(cy + s / 2.6f) },
            { (lv_coord_t)(cx + s / 40), (lv_coord_t)(cy + s / 2.9f) },
            { (lv_coord_t)(cx - s / 14), (lv_coord_t)(cy + s / 1.9f) },
        };
        lv_draw_line_dsc_t bl;
        lv_draw_line_dsc_init(&bl);
        bl.color = COL_BOLT; bl.width = 3; bl.opa = LV_OPA_COVER;
        for (int i = 1; i < 4; ++i) lv_draw_line(d, &bl, &bolt[i - 1], &bolt[i]);
        break;
    }
    }
}

void weather_icon_attach(lv_obj_t *obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(obj, (void *)(uintptr_t)0xFFFF);   // -1 => draw nothing yet
    lv_obj_add_event_cb(obj, icon_draw_cb, LV_EVENT_DRAW_MAIN, nullptr);
}

void weather_icon_set(lv_obj_t *obj, int wmoCode, bool night) {
    if (!obj) return;
    uint32_t packed = (wmoCode < 0) ? 0xFFFF : ((uint32_t)wmoCode & 0xFFFF);
    if (night && wmoCode >= 0) packed |= 0x10000;
    lv_obj_set_user_data(obj, (void *)(uintptr_t)packed);
    lv_obj_invalidate(obj);
}
