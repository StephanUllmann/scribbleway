# Map: Low-friction control surface for new users

## Destination

A written UI spec at `.scratch/popup-friction/spec.md` for Scribbleway's new-user
control surface: what sits on the live-presenter hot path, where each control lives
(overlay canvas / tray popup / hotkey), the visual language that makes it legible under
any system theme, and the first-run story. The map ends when that spec is written and
handed off — it decides, it does not build.

## Notes

**Domain**: desktop UI / interaction design for a Wayland screen-annotation overlay
(Qt6 + QML). See `CONTEXT.md` for the ubiquitous language ("Tray Popup", "Shape",
"Roughness") and `docs/adr/0001-retire-plasma-applet-unify-single-process-ui.md` for
why the popup is plain Qt Quick Controls.

**Primary user**: a live presenter/teacher, mid-screen-share, talking while drawing.
Friction is anything that takes their eyes off the content. The 20-slider property
panel is explicitly *not* on that path.

**Constraints** (all four confirmed at charting):
- No new UI dependencies. Plain `QtQuick.Controls` + `Canvas`/`Shapes` only, per
  ADR-0001. Any design language is hand-drawn, not imported — `ToolCursorBadge.qml` is
  the existing precedent ("pure geometry; no icon theme").
- Must survive macOS/Windows. Every decision states its fallback where it leans on
  layer-shell, `KGlobalAccel`, or Linux tray behaviour.
- Existing hotkeys are fixed. The spec may surface and document them; it may *add* a
  hotkey, but it may not remap `r`/`e`/etc.
- The popup stays an anchored tray popup. No preferences window, no persistent docked
  toolbar as a replacement.

**Skills every session should consult**: `/grilling` and `/domain-modeling` by default;
`/prototype` for ticket 05; `/research` for the research tickets.

## Decisions so far

<!-- one line per resolved ticket: gist + link -->

- [How do comparable tools give a live presenter a control surface?](issues/01-presenter-control-surfaces-in-comparable-tools.md)
  — four recurring patterns: blind modifier/letter switching for *tool and colour*;
  chromeless at runtime with setup front-loaded; cursor-anchored rather than
  fixed-location surfaces; a toolbar kept as escape hatch, not primary path. Excalidraw
  notably does *not* hotkey colour or width, so our compat reference does not answer this.
  Full findings: [docs/presenter-control-surfaces.md](../../docs/presenter-control-surfaces.md)
- [What does Qt Quick give us for theming that survives an arbitrary system palette?](issues/02-qt-theming-that-survives-any-system-palette.md)
  — icons take no colour from the palette at all: `icon.color` defaults to transparent
  and is never set, and all 16 SVGs are `stroke="currentColor"`, which QtSvg does not
  document. `palette.midlight` (today's hover colour) is a legacy 3D-bevel role with no
  distinctness guarantee on flat themes. Qt ships no contrast helper; `ColorOverlay`
  needs a new dependency and is therefore out. Live theme-change signal is available at
  our Qt 6.6 floor; forced-colours detection is not (6.10+).
  Full findings: [docs/qt-theming-findings.md](../../docs/qt-theming-findings.md)
- [What belongs on the presenter hot path?](issues/03-what-belongs-on-the-presenter-hot-path.md)
  — the hot path **already exists and is invisible**: 22 live shortcuts already cover
  arm → tool → colour → draw → undo → clear, and colour keys write the *default* when
  nothing is selected, so they arm the next stroke. The defect is discoverability, not
  allocation. The popup becomes a rare escape hatch whose every row shows its own live
  key, so each fallback teaches the key that removes the need to fall back. The canvas
  stays display-only — `ToolCursorBadge` extends to tool+colour+width and doubles as the
  armed indicator. A demoted **rarely-touched** tier inside the popup is the
  front-loaded-setup home (frequency, not target, is its axis). No new hotkeys. Four
  controls cut outright, including the Drawn Shapes Manager. Full split per control is in
  the ticket.

## Not yet specified

- **macOS/Windows fallbacks** for whatever 03 and 04 land on. Sharpened slightly by 03:
  the exposure now leans on `KGlobalAccel` for arming and on tray-popup anchoring, both
  named in the constraints as needing a stated fallback.

## Out of scope

- Overlay drawing behaviour, shape types, and Excalidraw compatibility — stable, and
  past this destination.
- Remapping existing hotkeys (constraint, see Notes).
- **A cursor-anchored pop-up palette** in the style of Krita's — ruled out explicitly by
  the dev at charting, after ticket 01 surfaced it as prior art. The control surface
  stays the tray popup plus whatever ticket 03 puts on the canvas or on a key; a
  right-click palette that spawns at the pointer is not on the table for this effort.
- Implementing the spec. The map produces the spec; building it is the handoff.
- **The `S` / cycleRoughness bug**, found in passing while resolving
  [What belongs on the presenter hot path?](issues/03-what-belongs-on-the-presenter-hot-path.md):
  `cycleRoughness()` at `src/overlay/overlaycontroller.cpp:1241` enters select mode
  instead of cycling roughness whenever a tool is active, so `S` does not do what its
  label claims. A code bug, not a design decision — logged here so it is not lost, and
  fixed outside this map.
- Packaging, install, and distribution flow.
