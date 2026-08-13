/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <dt-bindings/zmk/rgb.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

/* Brightness kept low since the strip sits under every key. */
#define RGB_LAYER_COLOR_BRT 15

/* Indices must match the layer order in config/sofle_choc_pro.keymap
 * (BASE 0, LOWER 1, RAISE 2, ADJUST 3). */
static const struct zmk_led_hsb rgb_layer_colors[] = {
    {.h = 0, .s = 0, .b = RGB_LAYER_COLOR_BRT},    /* BASE:   white */
    {.h = 0, .s = 100, .b = RGB_LAYER_COLOR_BRT},  /* LOWER:  red */
    {.h = 240, .s = 100, .b = RGB_LAYER_COLOR_BRT},/* RAISE:  blue */
    {.h = 120, .s = 100, .b = RGB_LAYER_COLOR_BRT},/* ADJUST: green */
};

/* zmk_rgb_underglow_set_hsb() only updates local state and never reaches the
 * peripheral half. The &rgb_ug keymap behavior is declared
 * BEHAVIOR_LOCALITY_GLOBAL, so invoking it through zmk_behavior_invoke_binding()
 * forwards the command over the split transport the same way a real &rgb_ug
 * keypress does — that's the only path that reaches both halves. */
static void rgb_layer_color_apply(struct zmk_led_hsb color) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(rgb_ug)),
        .param1 = RGB_COLOR_HSB_CMD,
        .param2 = RGB_COLOR_HSB_VAL(color.h, color.s, color.b),
    };
    struct zmk_behavior_binding_event event = {
        .layer = 0,
        .position = 0,
        .timestamp = k_uptime_get(),
    };

    zmk_behavior_invoke_binding(&binding, event, true);
}

static void rgb_layer_color_apply_current_layer(void) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();

    if (index >= ARRAY_SIZE(rgb_layer_colors)) {
        return;
    }

    rgb_layer_color_apply(rgb_layer_colors[index]);
}

static int rgb_layer_color_listener(const zmk_event_t *eh) {
    rgb_layer_color_apply_current_layer();

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_layer_color, rgb_layer_color_listener);
ZMK_SUBSCRIPTION(rgb_layer_color, zmk_layer_state_changed);
/* SYS_INIT fires before the split BLE link to the peripheral is up, so the
 * boot color-forward below is silently dropped there — the peripheral keeps
 * whatever color it last had until this fires on (re)connect. */
ZMK_SUBSCRIPTION(rgb_layer_color, zmk_split_peripheral_status_changed);

/* Seed the local (central) color at boot: layer_state_changed only fires
 * when a higher layer activates/deactivates, never for the always-active
 * base layer. The peripheral gets its boot color from the reconnect
 * subscription above instead, since the split link isn't up yet here. */
static int rgb_layer_color_init(void) {
    rgb_layer_color_apply_current_layer();

    return 0;
}

SYS_INIT(rgb_layer_color_init, APPLICATION, 90);
