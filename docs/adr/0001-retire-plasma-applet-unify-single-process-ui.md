# Retire the Plasma applet; unify on a single-process tray-popup UI

Scribbleway is expanding from KDE Plasma-only to also run for colleagues on Arch/Hyprland and macOS. The current two-process architecture (Plasma applet process ↔ overlay daemon, bridged by `AppletBackend` over D-Bus) exists only because Plasma hosts applets in a process separate from the app itself — a constraint that doesn't exist, and can't be replicated, on Hyprland or macOS.

**Decision**: retire the Plasma applet entirely, including on KDE/Plasma builds. Replace it with one `QSystemTrayIcon` + anchored `Controls.Popup` UI ("Tray Popup" — see `CONTEXT.md`), living in the same process as the overlay, shared across every platform. Single-instance enforcement moves from `KDBusService` (KDE Frameworks/D-Bus) to Qt's own `QLocalServer`/`QLocalSocket`.

**Why**: maintaining a native Plasma widget *and* a second UI for every other platform means building, testing, and evolving two UIs forever, with a real risk of them drifting apart. One UI built once is simpler to maintain, at the cost of losing the "looks like a native Plasma panel widget" feel on the KDE/Plasma build.

## Considered options

- **Keep the Plasma applet for KDE/Plasma, add a second tray-icon UI only for Hyprland/macOS/Windows.** Rejected — doubles UI maintenance indefinitely for a marginal "more native" feel on one platform.

## Consequences

- `src/applet-plugin/appletbackend.{h,cpp}` (~630 lines), `applet/contents/ui/main.qml`, and `applet/metadata.json` are deleted.
- `FullRepresentation.qml` is ported from `PlasmaComponents`/`Kirigami` to plain Qt Quick Controls, hosted inside the new Tray Popup.
- The D-Bus applet↔daemon IPC purpose disappears entirely; `QLocalServer`/`QLocalSocket` only needs to cover single-instance enforcement.
- The Arch package's KF6 footprint shrinks — no longer needs `plasma-desktop` at build/runtime for applet infrastructure. `LayerShellQt` (overlay) and, on Plasma only, `KGlobalAccel` (hotkeys) remain the only KDE Frameworks dependencies.
- A prior attempt at a non-Plasma settings UI produced a fullscreen overlay instead of an anchored popup. That is a known failure mode: the Tray Popup must be a small window anchored near the tray icon, never fullscreen.
