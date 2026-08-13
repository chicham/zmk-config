# zmk-config — Keebart Sofle Choc Pro BT, BÉPO

Personal ZMK firmware config for a Keebart Sofle Choc Pro BT, forked from
`Keebart/zmk-config`. Keymap is a BÉPO layout ported from a ZSA Voyager (QMK)
config, reconciled against the official BÉPO 1.1 standard (NF Z71-300).

## Repo layout

- `config/sofle_choc_pro.keymap` — the keymap, all 4 layers. ASCII diagrams
  above each `bindings` block show the physical layout; read those before
  editing blind.
- `config/west.yml` — west manifest. Includes `joelspadin/zmk-locales` for
  the `FR_*` AZERTY keycodes (`#include <locale/keys_fr.h>`).
- `boards/shields/rgb_layer_color/` — custom local shield: recolors the RGB
  underglow strip based on the active layer. See "RGB per layer" below.
- `boards/shields/nice_view_disp/` — display hardware **and** the custom
  status screen (Keebart's own fork of ZMK's nice!view canvas widgets, not
  stock ZMK's simple built-in labels). See "Status screens" below.
- `build.yaml` — GitHub Actions build matrix. Currently `sofle_choc_pro` only.

## Firmware workflow (jj, not git)

This repo uses **jj**, not git. Full procedure lives in the `/jj` skill —
load it before any graph mutation. Quick reference for this repo:

```bash
cd /Users/hicham.randrianarivo/Work/zmk-config/.workspaces/sofle-bepo   # maker workspace
jj tip-add -m "type(scope): description"   # describe BEFORE editing (describe-first)
# ...edit files...
jj bookmark set sofle-bepo -r 'my_tip()'
jj git push --bookmark sofle-bepo
```

**Gotcha:** if `jj git push` fails with `Won't push commit ... since it has
no description`, an empty auto-created commit is sitting in the chain
(usually left over from `jj workspace add` or a fixup). Find it in
`jj --no-pager log -r 'chain(@)'` and `jj abandon <that-change-id>` — not the
tip — then push again.

## Building firmware (GitHub Actions, not local)

No local ZMK/Zephyr toolchain — CI is the only build path.

```bash
gh workflow run "Build ZMK firmware" --repo chicham/zmk-config --ref sofle-bepo
sleep 5
gh api "repos/chicham/zmk-config/actions/runs?branch=sofle-bepo" \
  --jq '.workflow_runs[0] | {status,conclusion,id}'
```

Wait ~2-3 minutes, then check again. On failure:
`gh run view --repo chicham/zmk-config <run-id> --log-failed`.

**Fork's GitHub Actions must be manually enabled once** per fork
(github.com/chicham/zmk-config/actions → "I understand my workflows, go
ahead and enable them") — forks have Actions disabled by default and a
push before enabling silently produces zero runs.

## Flashing

```bash
gh run download <run-id> --repo chicham/zmk-config --dir ~/Downloads/firmware-latest
```

Produces 4 `.uf2` files (left/right × normal + `settings_reset`). Only the
`nice_view_disp rgb_layer_color-sofle_choc_pro_{left,right}-zmk.uf2` pair is
the normal firmware — `settings_reset` variants only get used if BLE
pairing breaks (clears bonds, needs re-pairing after).

Per half:
1. Connect via USB-C (data-capable cable).
2. Double-tap the reset button (small SMD button on the PCB, sometimes only
   reachable through a case cutout) until a `KEEBART` volume mounts.
3. `cp "<file>.uf2" /Volumes/KEEBART/` — the board flashes and reboots
   instantly, unmounting mid-copy. macOS then complains
   `could not copy extended attributes: Device not configured` — that's
   expected/cosmetic, not a failed flash. `ls /Volumes/` losing `KEEBART`
   confirms it took.

**Flash both halves together after any firmware change**, not just one —
some fixes (e.g. the RGB split-sync fix) only take effect on the central
(left) half, but stale firmware on the other half can still cause visibly
broken behavior (e.g. wrong color) until both are updated.

**Left = central, right = peripheral.** ZMK's full keymap/layer-state APIs
only exist on the central build — code in `rgb_layer_color` that calls
`zmk_keymap_highest_layer_active()` is compiled central-only (see its
`CMakeLists.txt`); the peripheral only forwards raw key events.

macOS input source must be **French - PC (AZERTY)** — the `FR_*` keycodes
assume it (System Settings → Keyboard → Input Sources).

## Layer overview

Layers: `BASE` (BÉPO) → `LOWER` (NUM) → `RAISE` (SYM) → `ADJUST` (auto,
LOWER+RAISE held together, via `conditional_layers` in the keymap).

**BASE** — BÉPO letters ported from Voyager and reconciled against the
official BÉPO 1.1 spec (comma on the home row, `v d l j z w` in BÉPO order,
apostrophe + ç on the main grid, circumflex dead-key for ê/â/î/ô/û). True
single-key `ê` isn't reachable over AZERTY/French-PC — needs the circumflex
dead-key + `e`, same as real AZERTY keyboards.
Left thumb (outer→inner): `SHIFT CTRL ALT GUI SPACE`.
Right thumb (inner→outer): `ENTER GUI ALT CTRL SHIFT`.
LOWER/RAISE are on the two extra row-4 keys below each screen (not on the
thumb — moved there deliberately to free up thumb slots).

**NUM** — numbers/symbols, entirely on the right hand. Left hand is mostly
`&trans` (falls through to BASE) except `ù` (home row).

**SYM** — extra punctuation + RGB brightness, left hand. Right thumb has a
dedicated arrow cluster (`LEFT DOWN UP RIGHT`) since row 1's `UP`/`DOWN`
positions are shadowed by RGB brightness keys.

**ADJUST** — Bluetooth profile select/clear, RGB hue/sat/effect/toggle,
`studio_unlock` (left hand); `F1`-`F12` (right hand, rows 1-2).

Known open items (not urgent): 3 bare `&none` main-grid slots (R1C4/C5
right, R4C1 left), no Delete key, no Caps Lock, no true single-key `ê`.

## RGB per layer

`boards/shields/rgb_layer_color/` — solid underglow color by active layer,
both halves in sync:
- BASE white · LOWER red · RAISE blue · ADJUST green
- Brightness: `RGB_LAYER_COLOR_BRT` in
  `boards/shields/rgb_layer_color/src/rgb_layer_color.c` (currently 10%)

**Must go through the `&rgb_ug` behavior, never call
`zmk_rgb_underglow_set_hsb()` directly** — that function only updates local
state and never reaches the peripheral half. `&rgb_ug` is declared
`BEHAVIOR_LOCALITY_GLOBAL`, so invoking it via `zmk_behavior_invoke_binding()`
(see `rgb_layer_color_apply()` in the source) is what forwards the color
over the split transport to both halves. This is documented ZMK behavior,
not a workaround — confirmed against ZMK's own split-keyboard docs.

**Boot-time race:** the boot color-forward runs from `SYS_INIT` at priority
90, which is early enough that the BLE split link to the peripheral usually
isn't up yet — that first command gets silently dropped and the peripheral
shows whatever color it last had (visible as "right half stays blue after a
fresh flash").

First fix attempt was wrong: subscribing to `zmk_split_peripheral_status_changed`
on the central side to re-forward on reconnect. That event is only ever
raised from `peripheral.c`, a peripheral-only compiled file — there is no
central-side "a peripheral connected" event in stock ZMK, so the
subscription silently never fired (compiled fine, dead code). Actual fix:
`rgb_layer_color_init()` applies immediately (correct for the central's own
LEDs right away) and also schedules a `k_work_delayable` that retries the
forward 3 times, 2 seconds apart — by the last retry the split link is
reliably up, and a command reaching an already-correct peripheral is a
harmless no-op.

True per-key color (like the original Voyager QMK `ledmap`) was considered
and explicitly declined: needs an undocumented LED-to-key wiring map for
this exact board, a custom driver bypassing `rgb_underglow` entirely, and
has no existing sync path to the peripheral half. Not worth it — revisit
only if that calculus changes.

## Status screens

`boards/shields/nice_view_disp/` is Keebart's own fork of ZMK's fancy
nice!view canvas UI (older LVGL API: `lv_canvas_draw_*`/`lv_img_*`, not the
newer `lv_draw_*`/`lv_image_*` used upstream) — **not** just a hardware
devicetree shield. Its `Kconfig.defconfig` already sets
`CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y`, so this is the code that was
actually driving the physical screen from day one (the stock ZMK built-in
label widgets were never in use). `custom_status_screen.c` picks
`widgets/status.c` (central) or `widgets/peripheral_status.c` (peripheral)
via `CMakeLists.txt`, same central/peripheral split pattern as
`rgb_layer_color`.

- **Central (left)** — `widgets/status.c`: three stacked 68×68 canvases —
  top (battery + output icon + WPM graph), middle (BT profile — a single
  "BT n" line, originally 5 connection-state circles that ate the whole
  canvas), bottom (layer name).
- **Peripheral (right)** — `widgets/peripheral_status.c`: one canvas
  (battery + split-link icon). The balloon/mountain decorative art that used
  to sit next to it was removed — the peripheral has nothing else to show,
  so padding the screen with random art wasn't worth keeping.

**What each half can actually know** — hard split, not a layout choice:
peripheral only ever gets its own battery level and split-link status
(`zmk_split_bt_peripheral_is_connected()`); it never receives keymap state,
so layer name, BT profile, WPM, and output status are central-only by
construction (ZMK's layer/output/WPM APIs don't exist on peripheral builds).

**Gotcha — don't add a competing shield for this.** A prior attempt added a
separate local shield (`status_screen_bepo`) using stock ZMK's generic
`zmk/display/widgets/*` (`battery_status`, `output_status`, `layer_status`,
`wpm_status`, `peripheral_status`) with its own `CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y`
and `zmk_display_status_screen()`. That fails to link: `nice_view_disp`
already provides both, and Kconfig's `imply` on the generic widgets pulls in
a second copy of the same symbols nice_view_disp's own widget code already
defines (`multiple definition of 'widget_battery_status_mutex'`, etc.), plus
two competing `zmk_display_status_screen()` definitions. **Edit
`nice_view_disp`'s own `custom_status_screen.c`/`widgets/*.c` directly** —
that is the real status screen, not a stand-in for it.

Considered and declined: forwarding RGB underglow color to the peripheral
screen. ZMK's `rgb_underglow` module fires no state-changed event — only
`zmk_rgb_underglow_get_state(bool*)` (on/off, not color) exists, and it's a
getter, not something a widget can subscribe to. Getting real color data
onto the peripheral screen would mean patching the vendored
`rgb_underglow.c` to add a proper event, the same class of effort as the
host-notification idea (also declined). Not worth it for what it'd show.
