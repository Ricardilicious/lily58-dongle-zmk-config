/*
 * Custom status screen for the Lily58 dongle (128x64 SH1106 OLED)
 *
 *  +------------------------------+
 *  | <output: USB / BT profile>   |
 *  | L [##########------]  85%    |
 *  | R [########--------]  62%    |
 *  | <layer name>                 |
 *  +------------------------------+
 *
 * L = first peripheral that paired with the dongle, R = second.
 * (Power on / pair the LEFT half first so L really is left.)
 */
#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/status_screen.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>

#define NUM_HALVES 2

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_layer_status layer_status_widget;

static lv_obj_t *bat_bar[NUM_HALVES];
static lv_obj_t *bat_pct[NUM_HALVES];

/* ---------- peripheral (half) battery widget ---------- */

struct periph_bat_state {
    uint8_t source;
    uint8_t level;
    bool valid;
};

static void periph_bat_update_cb(struct periph_bat_state s) {
    if (!s.valid || s.source >= NUM_HALVES || bat_bar[s.source] == NULL) {
        return;
    }
    uint8_t lvl = s.level > 100 ? 100 : s.level;
    lv_bar_set_value(bat_bar[s.source], lvl, LV_ANIM_OFF);
    lv_label_set_text_fmt(bat_pct[s.source], "%u%%", lvl);
}

static struct periph_bat_state periph_bat_get_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct periph_bat_state){.valid = false};
    }
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    if (ev == NULL) {
        return (struct periph_bat_state){.valid = false};
    }
    return (struct periph_bat_state){
        .source = ev->source,
        .level = ev->state_of_charge,
        .valid = true,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_periph_battery, struct periph_bat_state, periph_bat_update_cb,
                            periph_bat_get_state)
ZMK_SUBSCRIPTION(widget_periph_battery, zmk_peripheral_battery_state_changed);

static void make_battery_row(lv_obj_t *parent, int idx, const char *name, lv_coord_t y) {
    lv_obj_t *name_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_12, 0);
    lv_label_set_text(name_lbl, name);
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 74, 11);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 13, y + 1);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    /* outline + solid fill, works on a 1-bit display */
    lv_obj_set_style_bg_color(bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    bat_bar[idx] = bar;

    lv_obj_t *pct = lv_label_create(parent);
    lv_obj_set_style_text_font(pct, &lv_font_montserrat_12, 0);
    lv_label_set_text(pct, "--");
    lv_obj_align(pct, LV_ALIGN_TOP_RIGHT, 0, y);
    bat_pct[idx] = pct;
}

/* ---------- the screen ---------- */

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_white(), 0);

    /* top: USB / Bluetooth profile */
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);

    /* middle: both halves' batteries */
    make_battery_row(screen, 0, "L", 19);
    make_battery_row(screen, 1, "R", 33);
    widget_periph_battery_init();

    /* bottom: current layer */
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return screen;
}
