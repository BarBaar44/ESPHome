# Issue #40 — `lvglconfig.yml` widget patch

The backing logic is already on this branch in `lvglsensors.yml`:
`btn_restart`, `evaluate_helper_health`, `check_ha_settings`,
`sync_settings_from_ha`, the `helpers_missing_count` global and the
`ha_helpers_missing` problem sensor.

This file describes the remaining half: the LVGL widgets and the left-column
split.

**This is a temporary file. Apply the edit below locally and delete this file
in the same commit.**

> ⚠️ **This patch is REQUIRED, not optional.** `lvglsensors.yml` on this branch
> references `btn_check_settings` and `lbl_settings_status`, so **the config
> will not compile until these widgets exist.**

*(Supersedes `ISSUE-35-lvglconfig-patch.md`, which described a "Sync HA" button
before #35 was closed as mis-diagnosed. Delete that file too.)*

## Why it isn't applied directly

The change is roughly 60 inserted lines inside a 70 KB file. The tooling used
to open this PR can only write whole files, so applying it would have meant
re-emitting the other ~69 KB of a working panel config from scratch — a
silent typo in the fan card or the weather grid would be far more expensive
than a manual paste.

## What changes

Two edits:

1. `livingroom_light_obj` on `second_page` gets wrapped in a 2-row grid, with a
   new system-buttons card below it (this is the layout split originally asked
   for in #35).
2. A status label is added to the bottom of `settings_obj`.

### Step 1 — wrap the existing card in a 2-row container

Find the `LIVING ROOM LIGHT` block in the `MAIN GRID (second page)` section.
`livingroom_light_obj` keeps all its keys exactly as they are — it becomes row
0 of the new inner grid, which is also `grid_cell_row_pos: 0`. Everything
inside its `widgets:` list stays untouched. Wrap it in this new container:

```yaml
              # =========================
              # LEFT COLUMN (second page)
              #
              # Split into two rows per the original #35 layout request:
              # the living room light switch on top, the system buttons
              # below. Same grid-inside-a-grid-cell pattern as left_col
              # on main_page.
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
                    # (unchanged, including its grid_cell_* keys)
                    - obj:
                        id: livingroom_light_obj
                        ...

                    # ---- row 1: the new system buttons card ----
```

### Step 2 — the new buttons card

Goes in row 1 of `left_col_2`, immediately after `livingroom_light_obj`:

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

                          # Check that the HA settings helpers actually
                          # exist, and re-apply their values. Runs
                          # check_ha_settings, which evaluates first and
                          # then syncs - so if helpers are missing the
                          # user sees WHICH ones in lbl_settings_status
                          # instead of a button that silently does
                          # nothing. See #40.
                          #
                          # Starts DISABLED: at boot the API is not yet
                          # connected, so there is no cached state to
                          # evaluate. The 5s update_connectivity_icons
                          # interval enables it as soon as the API comes
                          # up, and disables it again if it drops.
                          - button:
                              id: btn_check_settings
                              width: 130
                              height: 45
                              x: -75
                              y: 20
                              align: CENTER
                              state:
                                disabled: true
                              bg_color: 0x2F8CD8
                              bg_opa: COVER
                              widgets:
                                - label:
                                    text: "Check HA"
                                    align: CENTER
                                    text_font: montserrat_14
                              on_click:
                                - script.execute: check_ha_settings

                          # Reboot the panel. Deliberately styled red and
                          # placed away from the check button - the
                          # restart platform reboots immediately, with no
                          # confirmation step.
                          #
                          # NOT gated on api.connected: rebooting needs
                          # nothing from HA, and is arguably most useful
                          # when the API connection has gone bad.
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

### Step 3 — the status label on the Settings card

`evaluate_helper_health` writes into `lbl_settings_status`. It belongs on
`settings_obj`, where the affected dropdowns are — a warning about the Pre-open
% helper is useless if it isn't next to the Pre-open % row.

`settings_obj` currently has 7 grid rows (0 = title, 1–5 = settings, 6 =
ESPHome version footer). Add an eighth:

```yaml
                    grid_rows:
                      - CONTENT   # row 0: "Settings" title
                      - FR(1)     # row 1: Screen timeout
                      - FR(1)     # row 2: Night start
                      - FR(1)     # row 3: Night end
                      - FR(1)     # row 4: Pre-open %
                      - FR(1)     # row 5: Door-open %
                      - CONTENT   # row 6: helper health status
                      - CONTENT   # row 7: ESPHome version footer
```

Add this widget (row 6, spanning both columns):

```yaml
                    # ---- HA helper health status (row 6) ----
                    # Written by evaluate_helper_health in
                    # lvglsensors.yml: green "Settings OK (5/5)" when
                    # every helper is present, red "N of 5 helpers
                    # missing: ..." when they are not. Re-evaluated
                    # every 60s, so the warning persists while the
                    # problem does and clears itself once fixed.
                    #
                    # The initial text is deliberately neutral - the
                    # first real evaluation runs ~20s after boot, and
                    # claiming either OK or broken before then would be
                    # a guess.
                    - label:
                        id: lbl_settings_status
                        text: "Checking HA settings..."
                        text_font: montserrat_14
                        text_color: 0x666666
                        grid_cell_row_pos: 6
                        grid_cell_column_pos: 0
                        grid_cell_column_span: 2
                        grid_cell_x_align: START
                        grid_cell_y_align: CENTER
```

And bump the existing ESPHome version footer from row 6 to row 7:

```yaml
                    - label:
                        id: lbl_esphome_version
                        ...
                        grid_cell_row_pos: 7   # was 6
```

## Notes

- **`evaluate_helper_health` uses raw `lv_label_set_text` / `lv_obj_set_style_text_color`**
  rather than `lvgl.label.update`, because the text and colour are computed
  together in one lambda. If your ESPHome version objects to that, the
  alternative is to split it into a lambda that stores the result in globals
  plus two `lvgl.label.update` actions reading them back.

- **Font/width check.** `"2 of 5 helpers missing: Open time, Door-open %"` is
  long. At `montserrat_14` in the settings column it will likely wrap or clip.
  If it looks bad, either add `text_wrap: true` / a fixed `width:`, or shorten
  the label names in the `checks[]` array in `evaluate_helper_health`.

- **No confirmation dialog** on Reboot. Hence the red styling and the gap
  between the buttons.

- **`x: -75` / `x: 75`** centres two 130px buttons with a 20px gap in the
  ~400px-wide left column. Adjust alongside the `FR(2)`/`FR(1)` row weights if
  it looks cramped.
