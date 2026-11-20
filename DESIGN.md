# Weather station design

The supplied 800 x 480 screenshot and `plan.md` are the visual contract.
This is a fixed-size embedded touch surface, not a responsive website.
Browser/Lighthouse requirements do not apply to the SDL/LVGL renderer.

Layout: fixed 800 x 480 frame; 60 px header; 260 x 420 left weather panel;
540 x 150 hourly strip; 385 x 270 daily list; 155 x 270 solar/lunar column.
The main forecast does not scroll. Read-only detail overlays own their content.

The primary action is Refresh. Reading priorities are current temperature,
future hourly temperatures and the five daily ranges. Required current fields,
precipitation, sunrise/sunset, lunar phase and freshness retain their own labels.
There is no settings affordance: configuration is external-file only.

## Tokens

- Background `#0B1120`; header `#282B36`; dividers `#283149`.
- Primary text `#F5F7FC`; secondary `#8799BE`; precipitation `#5DAAFF`.
- Sun `#FFC94A`; moon `#91CDE5`; error/status `#FFC98A`.
- Font: JetBrains Mono, tabular figures, compiled Cyrillic.
- Text sizes 14, 16, 20, 28; current-temperature digits 80.
- Header buttons 44 px high; hourly targets 77 x 150; daily targets 385 x 54.
- Separators are 1 px. Header controls have 8 px corner radius.

## Interaction

Touch feedback changes the pressed background immediately. A refresh changes
only its existing label and disabled state, preserving its dimensions. No
network operation runs in an event handler. Overlays have a 44 px Close/Back
target and Escape closes the active overlay on desktop.

The UI owns a complete copied snapshot. A selected hour/day is identified by
its absolute timestamp and is looked up again after updates. If removed, the
same overlay shows unavailable data. The About Data view is read-only.

## Artwork and verification

Day/night mountain/lake images are original deterministic procedural artwork,
generated locally as RGB565 assets. A dark overlay protects text contrast.
Weather icons are original vector-like LVGL drawing commands.
The reference PNG is never embedded as an application asset.

Verify 800 x 480 renders for day, night, offline, no-cache error, loading, stale,
long names/descriptions, negative and three-digit temperatures, large text.
Check real pointer interactions and card preservation on the shared desktop UI.
