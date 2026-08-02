# What belongs on the presenter hot path?

Type: grilling
Status: resolved
Blocked by: — (01 resolved)
Map: ../map.md

## Question

Write the 60-second story for a presenter mid-screen-share, and from it derive which
actions must be reachable **without reading the screen** — and, by subtraction, what is
left for the tray popup to hold.

This is the decision that shrinks the panel. Everything downstream (the design language,
the prototype, the fog around disclosure and the shapes list) hangs on it.

Settle at least:

- The minimum viable presenter loop: arm the overlay → pick a tool → pick a colour →
  draw → undo → clear → get out of the way. Which of those steps are currently more than
  one action, and which must become one?
- Which controls, if any, move **onto the overlay canvas** — and how that survives the
  constraint that the overlay must not steal input it shouldn't.
- Which controls stay in the tray popup, which become hotkey-only, and which are cut.
- Whether a fifth bucket applies: **front-loaded setup**. Ticket 01 found Gromit-MPX and
  ZoomIt both move configuration out of the running session entirely — to a config file
  and a one-time dialog respectively — leaving nothing on screen at runtime. Some of our
  twenty controls may be "set once, never during a presentation" rather than "hidden in a
  popup". Watch the tension: the standing constraint says no preferences window, so if
  this bucket is used it needs a home that isn't one — settle that here, not in 04.
- What a first-time user needs on screen before their first stroke, given they will not
  have read anything.
- Where the existing single-letter tool shortcuts and `ToolCursorBadge` fit — they are
  already half of a hot path that nobody is told about.

**Ruled out:** a cursor-anchored pop-up palette in Krita's style (surfaced as prior art
by ticket 01) is out of scope — ruled out explicitly by the dev. Do not re-propose it.
Every control goes on the canvas, in the tray popup, on a key, into front-loaded setup,
or gets cut.

Reads ticket 01's findings first. The output is a written verdict on the split
(canvas / popup / hotkey / cut) for every control currently in
`src/overlay/qml/FullRepresentation.qml`, not a layout.

## Answer

### The finding that reframes the ticket

**The presenter hot path already exists.** It is not missing — it is invisible. The
overlay registers 22 local shortcuts (`overlaycontroller.cpp:41-64`), wired in
`main.qml:794-827`, live whenever `shortcutGuard` holds — `currentMode !== "passthrough"`
and no text editor open (`main.qml:46`):

| Loop step | Binding | Note |
|---|---|---|
| Arm the overlay | `Meta+Shift+X` | global via KGlobalAccel, plus the `--toggle` socket for non-Plasma compositors (`main.cpp:64-88`) |
| Pick a tool | `F` `R` `E` `L` `A` `T` | freehand / rectangle / ellipse / line / arrow / text |
| Pick a colour | `1`–`6`, `Q` cycles | |
| Width | `+` / `-` | strokeWidth, or fontSize on a text shape (`overlaycontroller.cpp:1274`) |
| Undo / redo / clear | `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Delete` | |
| Select mode | `X`; `Delete`/`Backspace` deletes | plus `Escape`, copy/paste, arrow-key nudge |

Critically, `updateProperties()` falls through to the matching `default<Key>` property
when nothing is selected (`overlaycontroller.cpp:722-730`), so pressing `3` with no
selection **arms the next stroke** rather than doing nothing. The blind loop
`arm → tool → colour → draw → undo → clear` is therefore already complete and correct.

So the diagnosed defect is **discoverability, not allocation**. Nothing tells the user
these keys exist, and the popup actively competes with them by offering a slower path to
the same actions. Ticket 04 and the eventual spec should treat the keyboard as the
existing hot path to be *revealed*, not one to be designed.

### The decisions

1. **The tray popup is a rare escape hatch**, not the primary path — ticket 01's fourth
   recurring pattern (Epic Pen's hideable toolbar, Gromit-MPX's tray-only UI). A fluent
   presenter stays on keys; the popup is the fallback for the one control they forgot.
   Because it *is* used under pressure, it must stay fast to scan — a hard constraint on
   04's density rules.

2. **Every popup row carries its own key**, read live from
   `controller.localShortcutSequences` — the same source `ToolCursorBadge.qml:164-171`
   already uses, so it can never lie about a rebound or deliberately cleared binding.
   This is the discoverability fix, and it has the property that *every fallback to the
   escape hatch teaches the key that removes the need to fall back next time*. The
   escape hatch trains the user out of itself.

3. **The canvas carries display only, never controls.** `ToolCursorBadge` grows from
   "active tool + its key" to also show current colour and width with their keys. The
   overlay-must-not-steal-input constraint is satisfied by construction — the badge is
   already `enabled: false` and documented "never intercept pointer events". One existing
   widget then answers three questions at once: what tool am I on, what key changes it,
   and — by its mere presence — **is the overlay armed**.

4. **The fifth bucket (front-loaded setup) exists**, as a visually demoted section
   *inside* the popup. No new surface, no preferences window, constraint satisfied.
   Its axis is **frequency, not target**: these controls still edit a selected shape when
   there is one, so the tier is named **rarely-touched**, not "Defaults".

5. **The selection-vs-default routing stays as built** (`FullRepresentation.qml:38-55`) —
   it is correct behaviour and costs zero code. But "am I editing a selection or arming
   the next stroke" must be readable *without reading a label*; today one bold label flips
   text while every control below it silently changes meaning. 04 owns the form of that
   signal.

6. **No new hotkeys.** 22 bindings already exceed what anyone memorises, and the defect is
   that existing ones are invisible, not that there are too few — each key added makes the
   teaching problem it must then solve slightly worse. Revisit only if ticket 05's
   prototype exposes a step that genuinely cannot be done blind.

### The split, control by control

Every control in `FullRepresentation.qml`. A control appearing in both a key column and
the escape hatch is not a duplicate — that is what makes it an escape hatch.

**Canvas** — `ToolCursorBadge`, extended as above. Nothing else, nothing clickable.

**Cut (4)**

| Control | Why |
|---|---|
| "Scribbleway" header (`:96`) | You opened it from the Scribbleway tray icon |
| Active Tool combo (`:204`) | Duplicate; hides 5 of 6 options behind a click and has nowhere to show keys — the worst possible escape hatch |
| Draw button (`:115`) | Folded into the tool row |
| Drawn Shapes Manager, label + list (`:573-660`) | A debug view wearing a UI; a presenter selects a shape by clicking it on canvas, not by finding "Rectangle 4" in a list. Largest single block reclaimed. Its Lock button is rehomed, not lost |

**Popup — escape-hatch tier** (every row labelled with its live key)

| Control | Key |
|---|---|
| Tool row: Freehand / Rectangle / Ellipse / Line / Arrow / Text | `F` `R` `E` `L` `A` `T` |
| Select | `X` |
| Undo / Redo / Clear All | `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Delete` |
| Colour swatches ×6 + Custom | `1`–`6`, `Q` cycles |
| Width | `+` / `-` |
| Opacity | — |
| Fill swatches + Fill Op *(rectangle/ellipse only)* | — |

**Popup — rarely-touched tier** (demoted; still edits a selection when there is one)

| Control | Key |
|---|---|
| Roughness | `S` |
| Radius *(rectangle only)* | `ü` / `ä` |
| Glow | — |
| Smoothing | — |
| Font family, Font size *(text only)* | `+`/`-` for size |
| Target Screen | — |

**Selection row** (`:170-193`, unchanged, appears only on selection): Delete
(`Delete`/`Backspace`), Raise, Lower, **plus Lock** rehomed from the cut list —
`setShapeLocked()` is reachable nowhere else in the app.

Note the tiering axis is frequency, not key-existence: Opacity has no key yet sits in the
escape-hatch tier, while Roughness has one and is demoted.

### Handoff notes

- **What 04 inherits**: the escape hatch must be scannable under pressure; each row needs
  a place for a key hint; "editing selection" vs "arming next stroke" needs a non-textual
  signal; the rarely-touched tier needs a demotion treatment that is not a preferences
  window.
- **Out of scope, observed in passing**: `cycleRoughness()`
  (`overlaycontroller.cpp:1241-1247`) enters select mode instead of cycling roughness
  whenever a tool is active, so `S` does not do what its label claims. A code bug, not a
  design decision — logged on the map, not fixed here.
