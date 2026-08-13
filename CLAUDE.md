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
  `boards/shields/rgb_layer_color/src/rgb_layer_color.c` (currently 15%)

**Must go through the `&rgb_ug` behavior, never call
`zmk_rgb_underglow_set_hsb()` directly** — that function only updates local
state and never reaches the peripheral half. `&rgb_ug` is declared
`BEHAVIOR_LOCALITY_GLOBAL`, so invoking it via `zmk_behavior_invoke_binding()`
(see `rgb_layer_color_apply()` in the source) is what forwards the color
over the split transport to both halves. This is documented ZMK behavior,
not a workaround — confirmed against ZMK's own split-keyboard docs.

True per-key color (like the original Voyager QMK `ledmap`) was considered
and explicitly declined: needs an undocumented LED-to-key wiring map for
this exact board, a custom driver bypassing `rgb_underglow` entirely, and
has no existing sync path to the peripheral half. Not worth it — revisit
only if that calculus changes.
