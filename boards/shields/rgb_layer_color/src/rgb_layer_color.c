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
#include <zmk/keymap.h>
#include <zmk/rgb_underglow.h>

/* Brightness kept low since the strip sits under every key. */
#define RGB_LAYER_COLOR_BRT 10

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

/* zmk_split_peripheral_status_changed is raised only from peripheral.c
 * (peripheral-only compiled) — there is no equivalent central-side "a
 * peripheral connected" event in stock ZMK, so subscribing to it here was
 * dead code that never fired. A boot-time forward from SYS_INIT also
 * doesn't work: it runs before the split BLE link to the peripheral is up,
 * so that first command gets silently dropped and the peripheral is stuck
 * showing whatever color it last had. Instead, retry the forward a few
 * times a couple seconds apart — by then the link is reliably up, and a
 * command arriving on an already-correct peripheral is a harmless no-op. */
static struct k_work_delayable rgb_layer_color_boot_sync_work;
static int rgb_layer_color_boot_sync_retries_left = 3;

static void rgb_layer_color_boot_sync_handler(struct k_work *work) {
    rgb_layer_color_apply_current_layer();

    if (--rgb_layer_color_boot_sync_retries_left > 0) {
        k_work_schedule(&rgb_layer_color_boot_sync_work, K_SECONDS(2));
    }
}

static int rgb_layer_color_init(void) {
    /* Apply immediately too, so the central's own LEDs are correct from the
     * first frame — this copy always lands locally regardless of the split
     * link. Only the peripheral needs the delayed retries below. */
    rgb_layer_color_apply_current_layer();

    k_work_init_delayable(&rgb_layer_color_boot_sync_work, rgb_layer_color_boot_sync_handler);
    k_work_schedule(&rgb_layer_color_boot_sync_work, K_SECONDS(2));

    return 0;
}

SYS_INIT(rgb_layer_color_init, APPLICATION, 90);
