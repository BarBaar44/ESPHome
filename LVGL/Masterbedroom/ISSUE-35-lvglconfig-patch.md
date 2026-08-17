# Issue #35 — `lvglconfig.yml` widget patch

The backing logic for this issue (the `restart` button `btn_restart` and the
`sync_settings_from_ha` script) is already on this branch in
`lvglsensors.yml`. This file describes the remaining half: the LVGL widgets
and the left-column split.

**This is a temporary file. Apply the edit below locally and delete this file
in the same commit.**

## Why it isn't applied directly

The change is roughly 40 inserted lines inside a 70 KB file. The tooling used
to open this PR can only write whole files, so applying it would have meant
re-emitting the other ~69 KB of a working panel config from scratch — a
silent typo in the fan card or the weather grid would be far more expensive
than a manual paste. The logic half was safe to automate; this half wasn't.

## What changes

One widget block in `lvglconfig.yml` is replaced: `livingroom_light_obj`, the
left-hand card on `second_page`. The issue asks for that left cell to be split
into two, light switch on top and the two new buttons below.

`main_grid_2` is already a 2-column grid with a single row. The cleanest split
is to give the left cell its own 2-row grid, exactly mirroring how `left_col`
on `main_page` nests a grid inside a grid cell.

### Step 1 — wrap the existing card in a 2-row container

Find this block (in the `MAIN GRID (second page)` section):

```yaml
              # =========================
              # LIVING ROOM LIGHT
              # =========================
              - obj:
                  id: livingroom_light_obj
                  bg_color: 0xFFFFFF
                  radius: 6
                  pad_all: 6
                  scrollable: false
                  scrollbar_mode: "off"

                  grid_cell_row_pos: 0
                  grid_cell_column_pos: 0
                  grid_cell_x_align: STRETCH
                  grid_cell_y_align: STRETCH

                  widgets:
                    ...
```

Replace the four `grid_cell_*` lines on `livingroom_light_obj` with the two
below, so it becomes row 0 of the new inner grid instead of the cell itself.
Everything inside its `widgets:` list stays exactly as it is:

```yaml
                  grid_cell_row_pos: 0
                  grid_cell_column_pos: 0
                  grid_cell_x_align: STRETCH
                  grid_cell_y_align: STRETCH
```

...and wrap it in this new container, which takes over the outer grid cell:

```yaml
              # =========================
              # LEFT COLUMN (second page)
              #
              # Split into two rows per issue #35: the living room light
              # switch on top, the system buttons below. Same
              # grid-inside-a-grid-cell pattern as left_col on main_page.
              #
              # FR(2)/FR(1) rather than an even split: the light card was
              # designed as a full-height card and looks sparse if halved,
              # while two buttons need very little room.
              # =========================
              - obj:
                  id: left_col_2
                  scrollable: false
                  scrollbar_mode: "off"

                  grid_cell_row_pos: 0
                  grid_cell_column_pos: 0
                  grid_cell_x_align: STRETCH
                  grid_cell_y_align: STRETCH

                  layout:
                    type: GRID
                    pad_row: 8
                    pad_column: 0
                    grid_rows:
                      - FR(2)   # row 0: living room light card
                      - FR(1)   # row 1: system buttons card
                    grid_columns:
                      - FR(1)

                  widgets:

                    # ---- row 0: the existing living room light card ----
                    # (unchanged apart from its grid_cell_* keys)
                    - obj:
                        id: livingroom_light_obj
                        ...

                    # ---- row 1: the new system buttons card ----
```

### Step 2 — the new buttons card

This goes in row 1 of `left_col_2`, immediately after `livingroom_light_obj`:

```yaml
                    - obj:
                        id: system_buttons_obj
                        bg_color: 0xFFFFFF
                        radius: 6
                        pad_all: 6
                        scrollable: false
                        scrollbar_mode: "off"

                        grid_cell_row_pos: 1
                        grid_cell_column_pos: 0
                        grid_cell_x_align: STRETCH
                        grid_cell_y_align: STRETCH

                        widgets:

                          - label:
                              text: "System"
                              align: TOP_LEFT
                              x: 10
                              y: 5
                              text_font: montserrat_20
                              text_color: 0x0B0A0A

                          # Sync settings with Home Assistant.
                          # Re-applies HA's curtain time / position helpers
                          # to the Settings widgets; HA always wins. See
                          # sync_settings_from_ha in lvglsensors.yml.
                          - button:
                              id: btn_sync_settings
                              width: 130
                              height: 45
                              x: -75
                              y: 20
                              align: CENTER
                              widgets:
                                - label:
                                    text: "Sync HA"
                                    align: CENTER
                                    text_font: montserrat_14
                              on_click:
                                - script.execute: sync_settings_from_ha

                          # Reboot the panel. Deliberately styled red and
                          # placed away from the sync button - the restart
                          # platform reboots immediately, with no
                          # confirmation step.
                          - button:
                              id: btn_reboot_display
                              width: 130
                              height: 45
                              x: 75
                              y: 20
                              align: CENTER
                              bg_color: 0xC62828
                              bg_opa: COVER
                              widgets:
                                - label:
                                    text: "Reboot"
                                    align: CENTER
                                    text_font: montserrat_14
                              on_click:
                                - button.press: btn_restart
```

## Notes

- **`on_click: button.press: btn_restart`** reuses the `restart` button already
  added in `lvglsensors.yml` on this branch, so the panel can also be rebooted
  from HA. `- lambda: 'App.safe_reboot();'` would work too but wouldn't give
  you the HA-side entity.

- **No confirmation dialog.** The issue didn't ask for one, so this doesn't add
  one — hence the red styling and the gap between the two buttons. If a stray
  tap rebooting the panel turns out to be annoying in practice, the usual fix
  is a `checkable:` arm-then-confirm button or an LVGL msgbox; say the word and
  I'll add it.

- **`x: -75` / `x: 75`** centres two 130px-wide buttons with a 20px gap in the
  ~400px-wide left column. If the card ends up looking cramped once flashed,
  the numbers to adjust are those two and the `FR(2)`/`FR(1)` row weights.

- **Scope of "resync everything".** `sync_settings_from_ha` covers the five
  HA-backed cover helpers (open time, close time, default-times boolean,
  pre-open %, door-open %) plus the cover position slider. Screen timeout and
  night-mode start/end are device-local ESPHome globals with no HA counterpart,
  so there is nothing to sync them against. If you *do* have HA helpers
  mirroring those, tell me the entity IDs and I'll extend the script.
