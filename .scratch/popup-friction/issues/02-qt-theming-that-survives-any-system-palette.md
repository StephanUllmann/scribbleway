# What does Qt Quick give us for theming that survives an arbitrary system palette?

Type: research
Status: resolved
Blocked by: —
Map: ../map.md

## Question

The reported symptom: icons and labels in the tray popup are illegible depending on the
system theme. The root cause is not yet established. `FullRepresentation.qml` reads
`palette.text` / `palette.base` / `palette.midlight` directly and loads fixed-colour SVGs
from `qrc:/icons/*.svg` via `icon.source`; `ToolCursorBadge.qml` sidesteps the whole
problem by drawing glyphs as `Canvas` geometry with an explicit accent colour and its own
dark chrome ("pure geometry; no icon theme").

Establish the facts a design-language decision (ticket 04) depends on. Against primary
sources — Qt 6 documentation and Qt source where the docs are thin:

1. **Palette**: what `palette` roles does Qt Quick Controls actually populate on Linux
   (Breeze light/dark, Adwaita light/dark), on macOS, and on Windows? Which roles are
   reliable across all of them and which are effectively undefined? Is `palette.midlight`
   — used for hover today — one of the reliable ones?
2. **Icon recolouring**: what does `icon.color` on a `Controls.Button` do to an SVG, and
   what are its limits (multi-colour SVGs, strokes vs fills, `currentColor`)? What are the
   alternatives without new dependencies — `QtQuick.Effects` `MultiEffect`, `ShaderEffect`,
   `Image` + colour overlay, or hand-drawn `Canvas`/`Shapes` glyphs?
3. **Contrast**: is there anything built in for deriving a legible foreground from an
   unknown background colour, or must it be computed (luminance) in QML?
4. **Own-chrome escape hatch**: what breaks if a popup ignores the system palette entirely
   and paints its own surface, like `ToolCursorBadge` does? Consider accessibility settings
   (high contrast, forced colours) and platform expectations on macOS/Windows.
5. **Dark/light change at runtime**: does the popup re-render correctly when the system
   theme flips while running, and what signal drives that?

Deliverable: one Markdown file in `docs/` (match `docs/cross-platform-findings.md`),
citing each claim's source, ending with a plain statement of which approaches are
available to us under the "no new dependencies" constraint. Do not pick one — ticket 04
picks. Report what is possible.

## Answer

Findings: [docs/qt-theming-findings.md](../../../docs/qt-theming-findings.md)

**Root cause, as far as it is established.** Two independent contributors, both verified
against this repo:

1. `icon.color` defaults to `Qt::transparent` (confirmed in `QQuickIconPrivate` source)
   and is never set anywhere in `FullRepresentation.qml` — verified by grep. So icon
   recolouring is a no-op today. All 16 SVGs in `src/overlay/icons/` are
   `fill="none" stroke="currentColor"` — verified by grep — and QtSvg's `currentColor`
   support is *undocumented*. Whether `stroke="currentColor"` renders black or invisible
   is **not established**; QTBUG-141102 and forum reports cover `fill="currentColor"`
   falling back to black, which is a different case. Either way the icon takes no colour
   from the palette.
2. `palette.midlight`, used for hover, is documented only as a legacy 3D-bevel role
   ("between `button` and `light`"). Qt guarantees no distinctness from `base`/`window`,
   and flat themes like Breeze and Adwaita have no reason to populate it distinctly.

**Setting `icon.color` sidesteps the ambiguity entirely**: it recolours by post-raster
`CompositionMode_SourceIn` masking (`qquickiconimage.cpp`), indifferent to whether
`currentColor` ever resolved.

**Constraints on the option space:**

- Palette population on Linux comes from out-of-tree plugins (`plasma-integration`,
  `QGnomePlatform`), not Qt. On a GNOME box without `QGnomePlatform` installed, the app
  falls back to Qt's generic palette — Adwaita colours are not guaranteed to arrive.
- Qt ships **no** contrast or luminance helper. `Qt.lighter`/`darker`/`tint` never
  reference a background. Any "legible foreground against unknown background" rule is
  hand-written maths.
- Version gates against the project's Qt 6.6 floor: live colour-scheme change
  (`QStyleHints::colorSchemeChanged`) is 6.5+, so **available**. OS high-contrast /
  forced-colours detection is 6.10+, so **not reachable** without raising
  `QT_MIN_VERSION`.
- `Qt5Compat.GraphicalEffects.ColorOverlay` is a confirmed new dependency — ruled out by
  the no-new-dependencies constraint. `QtQuick.Effects`/`MultiEffect` probably needs no
  new `find_package`, but the research flags that as inferred, not confirmed.

Not established from primary sources: whether `stroke="currentColor"` renders black or
invisible in QtSvg; whether a bare `color: palette.text` binding is documented to
live-update on theme change; whether Plasma's high-contrast scheme surfaces through the
Qt 6.10 accessibility hint.
