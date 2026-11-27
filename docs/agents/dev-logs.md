# Reading the dev logs

When Scribbleway runs with `--dev`, it writes what it does to one file per day. The user describes buggy behaviour, often as "right after mark 3", and the log shows what the app actually did at that moment.

## Steps

1. **Open the day file.** The logs are in `~/.local/state/scribbleway/logs/YYYY-MM-DD.log` (or `$XDG_STATE_HOME/scribbleway/logs/`), one per day, and files older than seven days are deleted. If there is no file for that day, the app wasn't running with `--dev`. Ask the user to run `./dev-reinstall.sh --dev` and reproduce the bug.
2. **Find the session.** Each start of the app writes a `=== session start` header. Mark numbers start again from 1 in every session, so "mark 3" means the third mark *after the most recent header before it*. Read the header lines that follow it: the build (git hash), Qt version, desktop, and every screen with its geometry and scale.
3. **Find the moment.** Search for `--- MARK n ---`, or for the clock time the user gives. The user presses the mark *after* seeing the bug, so the cause sits before the mark. Read back to the last `toggle requested` or `mode:` line before it; that is usually where the gesture began.
4. **Match the description to the lines.** You are done when you can quote the lines that show the bug, or name the line that *should* be there and isn't (see "What the log leaves out" before treating a missing line as evidence).

## Line format

```
HH:mm:ss.zzz level    [category] message
```

The level is `info`, `warning`, `critical`, `fatal` or `debug`. The `[category]` tag appears only on lines from other code, such as `[layershellqt]` or `[qt.*]`. Warnings and criticals include every Qt and QML runtime error, such as a `TypeError` or a binding loop, with its `qrc:/scribbleway/qml/<file>:<line>`.

To find the code that writes a given line, grep for its literal text. Every dev line is a `qCInfo(lcDev)` call in C++.

## Vocabulary

**Modes.** `mode: passthrough (click-through)`, `mode: select` and `mode: draw` are the overlay's three modes. In passthrough, clicks and keys go to the apps underneath. `toggle requested` means the global shortcut (`Meta+Shift+X`) or `scribbleway-overlay --toggle` fired. `active tool: "rectangle"` (or `"(none)"`) is the drawing tool.

**Input and focus.** `keyboard interactivity: exclusive` means the overlay holds the keyboard and its hotkeys work; `none` means it has let go of it. `input mask: N rect(s), bounds QRect(...)` is the clickable area of the overlay: `QRect(0,0 1x1)` means click-through everywhere, and a full-screen rect means the overlay takes every click. `tray popup open` and `tray popup closed` belong together, and each `open` should be followed by a `closed`. When the app closes the popup itself, `popup close requested: <reason>` comes just before `closed` (the reason is `escape or canvas click`, `focus moved elsewhere` or `tray icon`). A `closed` with no request before it means something outside the app hid the popup, such as the compositor. `popup exclusion` is the strip carved out of the input mask so that the popup stays clickable. `popup wants keyboard` is true while a text field in the popup has focus.

**Screens.** `overlay placed on` is the overlay's first screen. `overlay remapped to X wanted Y` is printed after a screen change or a Target Screen choice; when X and Y differ, the overlay landed on the wrong screen. `screen added`, `screen removed` and `primary screen now` are the raw events, and the screen list after a remap shows how things settled.

**Shapes.** Each shape is written as its type, the first 8 characters of its id, and either `(x,y wxh)` or, for lines and arrows, `(fromX,fromY)->(toX,toY)`. Coordinates are logical pixels relative to the overlay's own screen, not global. Follow one shape through the log by its id.

- `add`, `delete`, `copy N element(s)`, `paste N shape(s)`, `undo`/`redo, shapes now N`, `clear N shape(s)`
- `edit <before> => <after>`, one line per shape that changed during a drag or resize, written when the gesture ends. This includes arrows pulled along by a bound shape.
- `bind <arrow> start -> <id> end -> <id>`: the endpoints that snapped when the arrow was drawn. An empty id means that end stayed free.
- `unbind <arrow> start|end from <id>`, `resnap <arrow> start|end to <id>`
- `shapes hidden` and `shapes shown` (the `Y` hotkey, or shapes reappearing on their own)

## What the log leaves out

A missing line is only evidence when that thing is logged in the first place. These things are not logged at all: the text typed into shapes, clipboard contents, freehand points, each mouse move, arrow-key nudges, selection changes, and colour, width and other style changes. These are logged only when they change, so repeats are silent: the input mask, keyboard interactivity, the popup exclusion and `popup wants keyboard`.

If the bug needs evidence the log doesn't record, add a `qCInfo(lcDev)` line where the action finishes, not on every move. Describe shapes with `describeShape()` in `overlaycontroller.cpp`, which leaves out text and points. Then ask the user to reproduce it.
