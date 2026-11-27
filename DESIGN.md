---
name: Scribbleway Tray Popup
description: A fixed ink-black control popup that shows the selected Shape as a live specimen beside the drawing canvas.
colors:
  ground: "#111111"
  well: "#0a0a0a"
  surface: "#1c1c1c"
  raised: "#2c2c2c"
  pressed: "#363636"
  line: "#2a2a2a"
  ink: "#f1efe9"
  mute: "#a3a09a"
  accent: "#a99bff"
  accent-ink: "#111111"
  selected-row: "#272335"
  danger: "#ff8a80"
typography:
  title:
    fontFamily: "sans-serif"
    fontSize: "13px"
    fontWeight: 600
  body:
    fontFamily: "sans-serif"
    fontSize: "13px"
    fontWeight: 400
  label:
    fontFamily: "monospace, Menlo"
    fontSize: "11px"
    fontWeight: 600
    letterSpacing: "0.8px"
rounded:
  sm: "5px"
  md: "7px"
  well: "10px"
spacing:
  gap: "8px"
  pad: "12px"
  label-column: "78px"
  value-column: "40px"
components:
  tool-button:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.md}"
    height: "38px"
  tool-button-current:
    backgroundColor: "{colors.accent}"
    textColor: "{colors.accent-ink}"
    rounded: "{rounded.md}"
    height: "38px"
  icon-button:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.md}"
    size: "30px"
  icon-button-hover:
    backgroundColor: "{colors.raised}"
  text-button:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.sm}"
    padding: "0 10px"
    height: "26px"
  text-button-danger:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.danger}"
    rounded: "{rounded.sm}"
    padding: "0 10px"
    height: "26px"
  choice-bar:
    backgroundColor: "{colors.surface}"
    rounded: "{rounded.md}"
    padding: "2px"
    height: "30px"
  segment-checked:
    backgroundColor: "{colors.raised}"
    textColor: "{colors.ink}"
    typography: "{typography.title}"
    rounded: "{rounded.sm}"
    height: "26px"
  segment:
    textColor: "{colors.mute}"
    typography: "{typography.body}"
    rounded: "{rounded.sm}"
    height: "26px"
  swatch:
    rounded: "{rounded.sm}"
    size: "26px"
  specimen-well:
    backgroundColor: "{colors.well}"
    rounded: "{rounded.well}"
    height: "128px"
  shape-row-selected:
    backgroundColor: "{colors.selected-row}"
    textColor: "{colors.ink}"
    typography: "{typography.title}"
    height: "36px"
---

# Design System: Scribbleway Tray Popup

## Overview

**Creative North Star: "The Live Specimen"**

The Tray Popup is a small, dense, ink-black instrument that sits beside a canvas the user is actively drawing on. Its centre of gravity is the specimen well: a recessed black panel where the selected Shape (or the next Shape of the active tool) is drawn by the canvas's own Shape delegates, roughness, glow and fill included, and redraws as any control moves. Every other element is a quiet control that feeds the specimen.

The look is fixed and its own on every platform. It never reads the system palette and never inherits the desktop's UI font; the Basic controls are re-pointed at the popup's tokens so even unstyled parts (tooltips, combo box popup, text fields) match. Hierarchy comes from weight and tone, not from size: one body size, DemiBold for what is current or named, warm grey for what is secondary. A single lavender accent marks the current tool, slider fill, active tab and focus. The preset drawing colours are the only other colour on screen.

Density is that of a desktop tool panel: 432px wide, 12px outer padding, controls 26 to 38px tall, aligned label and value columns.

**Key Characteristics:**
- Ink-black ground with tonal step-ups; no shadows.
- One lavender accent, used for "current" and "focus" only.
- One 13px body size; rank by weight (Normal / Medium / DemiBold).
- Small mono for measurements, hotkey letters and hex input only.
- The specimen well is the signature: the Shape drawn exactly as the canvas draws it.
- Hotkey letters printed on tools and swatches teach the keyboard.

## Colors

A near-neutral black ramp with warm paper-white ink, one cool lavender accent and one coral warning tone.

### Primary
- **Signal Lavender** (accent): the current tool's fill, slider fill, the active tab's 2px underline, the locked-shape icon, and every focus ring. Text on it uses Accent Ink.
- **Accent Ink** (accent-ink): near-black glyph and hotkey colour on the lavender current-tool button.

### Neutral
- **Ink-Black Ground** (ground): the popup window background and the Basic `palette.window`.
- **Specimen Black** (well): the specimen well, one step below the ground so the drawn Shape sits in a recess.
- **Step-Up Surface** (surface): buttons, segmented tracks, text fields, combo boxes, hovered list rows.
- **Raised Charcoal** (raised): checked segment, button hover, slider groove, scrollbar thumb, swatch frame and hex-field border at rest.
- **Pressed Charcoal** (pressed): the pressed state of every button.
- **Hairline** (line): the 1px window edge, tab-bar rule, list footer rule, picker divider, well border.
- **Paper Ink** (ink): all primary text and icons; the selected-swatch ring; slider handle at rest.
- **Warm Mute** (mute): field labels, unchecked segments and tabs, readouts, hotkey letters on unchecked tools, disabled text (6.5:1 on surface).
- **Accent-Tinted Row** (selected-row): background of the selected row in the Shapes list.

### Tertiary
- **Coral Warning** (danger): Delete and Clear all text, the hover tint of a row's delete icon, the slash through the "no fill" swatch.

### Named Rules
**The One Lavender Rule.** The accent means "current" or "focused" and nothing else. It never decorates, never fills a panel, never colours a heading.

**The Preset-Only Colour Rule.** Apart from the accent and the coral warning, the only saturated colours in the popup are the user's preset drawing colours (supplied by the controller) and the colour picker's own gradients. Chrome stays neutral so a swatch reads as a drawing colour.

**The Own-Palette Rule.** No control reads the system palette. The popup maps every Basic palette role to these tokens; a role left unmapped is a theme waiting to break legibility.

## Typography

**Body Font:** `sans-serif` (resolves to the platform's stock sans, e.g. Noto Sans on Linux)
**Label/Mono Font:** `monospace` (Menlo on macOS)

**Character:** A plain stock sans pinned against the desktop UI font, paired with a small mono that reads as instrument readout. No font files ship.

### Hierarchy
- **Title** (DemiBold 600, 13px): the specimen name, section headings in More, checked segments and tabs, the selected list row, the empty-state headline, the picker's "Custom colour" heading.
- **Body** (Normal 400, 13px; tabs at rest Medium 500): field labels, values, list rows, tab labels, helper text. Value columns use tabular figures so numbers hold still while a slider drags.
- **Label** (mono, 600 to 700, 11px): hotkey letters on tool buttons (top-right) and swatches (bottom-left), and the specimen readout line, which is uppercase with 0.8px tracking and tabular figures. The hex field uses mono at body size.

### Named Rules
**The One Size Rule.** Body text is 13px; the only other size is the 11px mono label. Rank by weight and tone, never by a bigger font.

**The Mono-Means-Measurement Rule.** Mono appears only for numbers and keys: the readout, hotkey letters and hex input. Never for labels, headings or prose.

## Layout

A single 432px column (minimum 400 by 520, default height 550). Outer padding is 12px (`pad`); content rows inside tabs inset 14px (`pad` + 2). From the top: a mode and history row (Draw/Select segmented track left, icon buttons right), a full-width row of six equal tool buttons (4px apart, 38px tall), a tab bar (Style, Shapes with count, More; 20px between tabs, 1px hairline rule beneath), then the tab page.

Style and More are built from aligned field rows: a 78px label column in Warm Mute, a flexible control slot (7px internal gap), and an optional 40px right-aligned value column; each row is at least 34px tall with 10px between columns. Helper text in More starts at the control column (label column + 10px), not at the label.

The Shapes tab is a list with 36px rows, 2px apart, and a footer separated by a hairline: count on the left, Clear all on the right.

The colour picker, when open, is a fixed 208px panel that grows the window to the left (the surface is anchored bottom-right), separated from the menu by a 1px hairline.

## Elevation & Depth

Flat. There are no shadows. Depth is tonal: the specimen well sits one step below the ground (Specimen Black), controls sit one step above it (Step-Up Surface), and hover and checked states step up again (Raised Charcoal, then Pressed Charcoal). A 1px hairline around the whole window lets the dark popup read as a surface against a dark desktop.

### Named Rules
**The Tonal Step Rule.** Depth is expressed by moving one step along the black ramp (well, ground, surface, raised, pressed). Never by a shadow or glow on chrome.

## Shapes

Gently rounded rectangles throughout. Controls and tracks use 7px corners (`md`); inner segments, text buttons and swatches use 5px (`sm`); the specimen well uses 10px (`well`) so it reads as a container rather than a control. Slider grooves are 4px pills; handles and the picker's saturation marker are circles. Swatches carry an outer 2px ring at 8px radius, 3px outside the chip, for selected (Paper Ink), focus (lavender) and hover (Warm Mute). Focus is a 1px lavender border on most controls and 2px on tool buttons and slider handles.

## Components

### Buttons
Quiet charcoal blocks that step up on interaction.
- **Tool button:** equal-width, 38px tall, 7px corners, 20px icon on Step-Up Surface; mono hotkey letter top-right in Warm Mute. **Current:** Signal Lavender fill with Accent Ink icon and hotkey; focus ring switches to Paper Ink.
- **Icon button:** 30px square (28px inside list rows), 18px icon (16px in rows), 7px corners; surface at rest or transparent when flat; hover Raised Charcoal, pressed Pressed Charcoal. Tooltip after 600ms; tooltip text carries the hotkey.
- **Text button:** 26px tall, 10px side padding, 5px corners, on surface. Danger variant swaps the text to Coral Warning (Delete, Clear all). Disabled text falls to Warm Mute.

### Segmented controls (Choice Bar)
A 30px Step-Up Surface track at 7px corners with 2px inset; 26px segments at 5px corners. Checked segment: Raised Charcoal, Paper Ink, DemiBold. Unchecked: transparent, Warm Mute, Normal. Used for Draw/Select, Roughness, Font family and Smoothing.

### Sliders
4px Raised Charcoal groove with a Signal Lavender fill up to the value; a 16px Paper Ink circular handle that brightens to white on hover or press; 2px lavender ring on focus. One drag is one undo step.

### Swatches
26px chips at 5px corners, the preset colour as the chip, the mono hotkey digit bottom-left in whichever of near-black or white reads better on the chip (`inkOn`, WCAG luminance). The "no fill" swatch is a surface chip with a coral diagonal slash. The custom swatch shows "+" on Raised Charcoal, or the current custom colour alone when one is set.

### Inputs / Fields
Text field and combo box: 30px, 7px corners, Step-Up Surface. The hex field has a 1px Raised Charcoal border that turns lavender with focus, and a lavender selection with Accent Ink text. Combo dropdowns open inside the window.

### Navigation (Tabs)
Text-only tabs, 36px tall, 20px apart. Checked: Paper Ink DemiBold over a 2px lavender underline. Unchecked: Warm Mute Medium, Paper Ink on hover. The Shapes tab carries its count in Warm Mute. Focus draws a 4px-radius lavender outline inset 4px.

### List rows
36px, 6px corners, 16px type icon, name as "Type N". Selected: Accent-Tinted Row background and DemiBold. Hover: surface. Trailing flat icon buttons for lock (lavender when locked, mute when not) and delete (coral on hover). The empty state is a centred DemiBold headline over a Warm Mute sentence.

### Specimen Well (signature)
A 128px recessed panel (Specimen Black, 10px corners, hairline border, clipped) at the top of the Style tab. It hosts the canvas's own Shape delegates, fed a single specimen Shape built from the selection or from the style the next Shape will carry, so what it shows is exactly what the canvas draws. Top-left: the Shape's name in Title (Paper Ink when a Shape is selected, Warm Mute for "New <tool>"). Top-right, only with a selection: Raise, Lower and Delete text buttons. Bottom-left: the uppercase mono readout (width, opacity, fill opacity, roughness; or font, size, opacity for text).

## Do's and Don'ts

### Do:
- **Do** draw every new colour, size and radius from `Theme.qml`; add a token there before using a new value twice.
- **Do** map any new Basic control's palette roles (or its background and content items) to the popup's tokens, so nothing falls back to the system theme.
- **Do** rank by weight and tone at 13px: DemiBold Paper Ink for current, Normal Warm Mute for secondary.
- **Do** print the bound hotkey on any control that has one, in 11px mono, read from the live binding rather than hard-coded.
- **Do** use the aligned field row (78px label, control, 40px tabular value) for every setting.
- **Do** keep every text colour at WCAG AA or better on ground and surface; use `inkOn()` for text on arbitrary colours.

### Don't:
- **Don't** use Signal Lavender for anything but the current tool, slider fill, active tab and focus.
- **Don't** read the system palette or the desktop's UI font, or switch to the platform Controls style.
- **Don't** add shadows or glows to chrome; step along the black ramp instead.
- **Don't** introduce a larger heading size; the popup has one body size.
- **Don't** use mono for labels or prose; it is for measurements, keys and hex only.
- **Don't** open dropdowns or dialogs as separate windows; they must stay in-window.
