# Product

<!-- impeccable:product-schema 1 -->

## Platform

Desktop (Qt 6.6+ / QML). Runs on KDE Plasma 6, Hyprland and other layer-shell Wayland compositors, and macOS.

## Users

The author and colleagues who annotate their screen while presenting, demoing or explaining something. They draw on top of whatever is on screen, then adjust or remove what they drew.

## Product Purpose

Scribbleway is an Excalidraw-style drawing overlay for the desktop. Shapes (freehand, arrow, rectangle, ellipse, line, text) are drawn on a transparent full-screen layer and stay editable: they can be selected, restyled, moved, locked, reordered and deleted, and copied to and from Excalidraw.

## Positioning

Unlike pixel annotation tools, every mark is a vector Shape that stays editable and speaks Excalidraw's clipboard format, so a sketch made over a live screen can move straight into Excalidraw.

## Operating Context

- The overlay is toggled with a global hotkey (`Meta+Shift+X`). Single-letter hotkeys pick tools (F, A, R, E, L, T), 1–6 pick preset colours, Q cycles colour, S cycles roughness, Ctrl+Z undoes.
- The Tray Popup opens from the system tray icon, anchored near it, never fullscreen. It sits beside a canvas the user is actively drawing on, often during a live presentation.
- Confirmed: the popup is opened mostly to **restyle a selected Shape** (colour, fill, opacity, roughness and so on). Picking the next tool and managing the shape list come second.

## Capabilities and Constraints

- Tray Popup controls: mode (draw/select), undo/redo, clear all, delete/raise/lower selection, active tool, target screen, stroke colour (presets + custom picker), width, opacity, fill colour and fill opacity (rectangle/ellipse), glow, freehand smoothing, roughness (Neat/Artist/Cartoon), corner radius, font family and size (text), and the list of drawn Shapes with lock and delete per row.
- No new dependencies: plain QtQuick, QtQuick.Controls, QtQuick.Shapes, QtQuick.Effects and Canvas only (see `docs/qt-theming-findings.md`).
- Bundled icons use `stroke="currentColor"`, which QtSvg does not resolve; untinted they render black.
- Dropdowns must open in-window (`Popup.Item`); a separate Wayland surface would dismiss the popup.
- Confirmed: rarely-touched settings (glow, smoothing, corner radius, target screen) may move off the main view into tabs.

## Brand Commitments

- Confirmed: the Tray Popup uses its own fixed **dark** look on every platform, regardless of the system theme, following the precedent of `ToolCursorBadge.qml` on the canvas.
- Vocabulary from `GLOSSARY.md` (Shape, Selection, Draft, Roughness, Tray Popup).

## Product Principles

1. The canvas is the work; the popup serves it and gets out of the way.
2. Readable everywhere: legibility never depends on which desktop theme is installed.
3. Shapes stay editable; the popup is where that editability becomes visible.
4. Keyboard first: every popup control that has a hotkey should teach it.

## Accessibility & Inclusion

Text and icons must meet WCAG AA contrast against the popup's own background, since the popup ignores system high-contrast settings by choice.
