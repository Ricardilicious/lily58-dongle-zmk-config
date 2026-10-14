/*
 * Lily58 dongle - clean status screen (128x64 SH1106 OLED)
 *
 *   USB                    42 WPM
 *   ------------------------------
 *               BASE
 *   ------------------------------
 *   L [################----]  85%
 *   R [###########---------]  62%
 *
 * L = first half that paired with the dongle, R = second
 * (power on the LEFT half first when pairing).
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>
#include <zmk/endpoints.h>
#include <zmk/ble.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/wpm_state_changed.h>

#define W 128
#define NUM_HALVES 2
#define SMALL (&lv_font_unscii_8)
#define BIG (&lv_font_montserrat_16)

static lv_obj_t *output_lbl;
static lv_obj_t *wpm_lbl;
static lv_obj_t *layer_lbl;
static lv_obj_t *bat_bar[NUM_HALVES];
static lv_obj_t *bat_pct[NUM_HALVES];

/* ---------------- helpers ---------------- */

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, const char *txt) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_label_set_text(l, txt);
    return l;
}

static void make_rule(lv_obj_t *parent, lv_coord_t y) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, W, 1);
    lv_obj_set_pos(r, 0, y);
    lv_obj_set_style_bg_color(r, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
}

static void make_battery_row(lv_obj_t *parent, int idx, const char *name, lv_coord_t y) {
    lv_obj_t *n = make_label(parent, SMALL, name);
    lv_obj_set_pos(n, 0, y);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 80, 8);
    lv_obj_set_pos(bar, 11, y);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_border_color(bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    bat_bar[idx] = bar;

    lv_obj_t *p = make_label(parent, SMALL, "--");
    lv_obj_align(p, LV_ALIGN_TOP_RIGHT, 0, y);
    bat_pct[idx] = p;
}

/* ---------------- layer ---------------- */

struct layer_state {
    const char *name;
    uint8_t index;
};

static void layer_update_cb(struct layer_state s) {
    if (layer_lbl == NULL) {
        return;
    }
    char buf[16];
    if (s.name != NULL && s.name[0] != '\0') {
        size_t i = 0;
        for (; s.name[i] != '\0' && i < sizeof(buf) - 1; i++) {
            buf[i] = toupper((unsigned char)s.name[i]);
        }
        buf[i] = '\0';
    } else {
        snprintf(buf, sizeof(buf), "LAYER %u", s.index);
    }
    lv_label_set_text(layer_lbl, buf);
    lv_obj_align(layer_lbl, LV_ALIGN_TOP_MID, 0, 15);
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_state){
        .index = index,
        .name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index)),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer, struct layer_state, layer_update_cb, layer_get_state)
ZMK_SUBSCRIPTION(widget_layer, zmk_layer_state_changed);

/* ---------------- output (USB / BT) ---------------- */

struct output_state {
    struct zmk_endpoint_instance ep;
    bool connected;
    bool bonded;
};

static void output_update_cb(struct output_state s) {
    if (output_lbl == NULL) {
        return;
    }
    char buf[16];
    if (s.ep.transport == ZMK_TRANSPORT_USB) {
        snprintf(buf, sizeof(buf), "USB");
    } else if (!s.bonded) {
        snprintf(buf, sizeof(buf), "BT%d PAIR", s.ep.ble.profile_index + 1);
    } else if (s.connected) {
        snprintf(buf, sizeof(buf), "BT%d", s.ep.ble.profile_index + 1);
    } else {
        snprintf(buf, sizeof(buf), "BT%d ...", s.ep.ble.profile_index + 1);
    }
    lv_label_set_text(output_lbl, buf);
}

static struct output_state output_get_state(const zmk_event_t *eh) {
    return (struct output_state){
        .ep = zmk_endpoints_selected(),
        .connected = zmk_ble_active_profile_is_connected(),
        .bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output, struct output_state, output_update_cb, output_get_state)
ZMK_SUBSCRIPTION(widget_output, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(widget_output, zmk_ble_active_profile_changed);

/* ---------------- WPM ---------------- */

struct wpm_state {
    int wpm;
};

static void wpm_update_cb(struct wpm_state s) {
    if (wpm_lbl == NULL) {
        return;
    }
    lv_label_set_text_fmt(wpm_lbl, "%d WPM", s.wpm);
    lv_obj_align(wpm_lbl, LV_ALIGN_TOP_RIGHT, 0, 1);
}

static struct wpm_state wpm_get_state(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = eh ? as_zmk_wpm_state_changed(eh) : NULL;
    return (struct wpm_state){.wpm = ev ? ev->state : 0};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm, struct wpm_state, wpm_update_cb, wpm_get_state)
ZMK_SUBSCRIPTION(widget_wpm, zmk_wpm_state_changed);

/* ---------------- halves' batteries ---------------- */

/* Remember every half's level, so two reports arriving together never get lost */
static int16_t bat_level[NUM_HALVES] = {-1, -1};

struct bat_state {
    int16_t level[NUM_HALVES];
};

static void bat_update_cb(struct bat_state s) {
    for (int i = 0; i < NUM_HALVES; i++) {
        if (bat_bar[i] == NULL || s.level[i] < 0) {
            continue;
        }
        int lvl = s.level[i] > 100 ? 100 : s.level[i];
        lv_bar_set_value(bat_bar[i], lvl, LV_ANIM_OFF);
        lv_label_set_text_fmt(bat_pct[i], "%d%%", lvl);
        lv_obj_align(bat_pct[i], LV_ALIGN_TOP_RIGHT, 0, lv_obj_get_y(bat_bar[i]));
    }
}

static struct bat_state bat_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        eh ? as_zmk_peripheral_battery_state_changed(eh) : NULL;
    if (ev != NULL && ev->source < NUM_HALVES) {
        bat_level[ev->source] = ev->state_of_charge;
    }
    struct bat_state s;
    for (int i = 0; i < NUM_HALVES; i++) {
        s.level[i] = bat_level[i];
    }
    return s;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bat, struct bat_state, bat_update_cb, bat_get_state)
ZMK_SUBSCRIPTION(widget_bat, zmk_peripheral_battery_state_changed);

/* ---------------- screen ---------------- */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);
    lv_obj_set_style_text_font(screen, SMALL, 0);

    /* top bar */
    output_lbl = make_label(screen, SMALL, "USB");
    lv_obj_set_pos(output_lbl, 0, 1);
    wpm_lbl = make_label(screen, SMALL, "0 WPM");
    lv_obj_align(wpm_lbl, LV_ALIGN_TOP_RIGHT, 0, 1);
    make_rule(screen, 11);

    /* layer name, big and centred */
    layer_lbl = make_label(screen, BIG, "BASE");
    lv_obj_align(layer_lbl, LV_ALIGN_TOP_MID, 0, 15);
    make_rule(screen, 36);

    /* batteries */
    make_battery_row(screen, 0, "L", 41);
    make_battery_row(screen, 1, "R", 54);

    widget_output_init();
    widget_layer_init();
    widget_wpm_init();
    widget_bat_init();

    return screen;
}
