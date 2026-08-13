/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

/* Brightness kept low (5-10% range) since the strip sits under every key. */
#define RGB_LAYER_COLOR_BRT 7

/* Indices must match the layer order in config/sofle_choc_pro.keymap
 * (BASE 0, LOWER 1, RAISE 2, ADJUST 3). */
static const struct zmk_led_hsb rgb_layer_colors[] = {
    {.h = 0, .s = 0, .b = RGB_LAYER_COLOR_BRT},    /* BASE:   white */
    {.h = 0, .s = 100, .b = RGB_LAYER_COLOR_BRT},  /* LOWER:  red */
    {.h = 240, .s = 100, .b = RGB_LAYER_COLOR_BRT},/* RAISE:  blue */
    {.h = 120, .s = 100, .b = RGB_LAYER_COLOR_BRT},/* ADJUST: green */
};

static int rgb_layer_color_listener(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();

    if (index >= ARRAY_SIZE(rgb_layer_colors)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    zmk_rgb_underglow_set_hsb(rgb_layer_colors[index]);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_layer_color, rgb_layer_color_listener);
ZMK_SUBSCRIPTION(rgb_layer_color, zmk_layer_state_changed);

/* Seed the base-layer color at boot: layer_state_changed only fires when a
 * higher layer activates/deactivates, never for the always-active base layer. */
static int rgb_layer_color_init(void) {
    zmk_rgb_underglow_set_hsb(rgb_layer_colors[0]);

    return 0;
}

SYS_INIT(rgb_layer_color_init, APPLICATION, 90);
