# First run: the cold-start problem

Type: grilling
Status: open
Blocked by: — (03 resolved)
Map: ../map.md

## Question

Ticket 03 settled how a user learns the keyboard **once the overlay is armed**: the popup
shows every row's live binding, and `ToolCursorBadge` shows the active tool's key on
canvas. Both mechanisms sit *behind* the arming step. That leaves a cold start with no
teacher:

- The badge only exists once the overlay is live, so it cannot teach the key that makes
  the overlay live.
- `Meta+Shift+X` is registered through `KGlobalAccel`, which ticket 02 and the standing
  constraints note is inert outside Plasma — non-Plasma users must bind
  `scribbleway-overlay --toggle` themselves (`src/overlay/main.cpp:64-88`). A first-run
  story that assumes the shortcut works is wrong for a large share of users.
- Nothing indicates the tray icon is the entry point, or that a tray icon exists at all
  on desktops that hide unknown status items.

Settle:

- What a user sees the first time Scribbleway runs, before any stroke — given the
  standing constraint that there is no preferences window, and 03's ruling that the
  canvas carries display only, never controls.
- How the arming step is taught, including for the compositor where the shortcut was
  never bound. Whether "no shortcut is registered" is a state the app can detect and say
  something about.
- Whether first run is a one-time thing (ZoomIt's dismissable dialog, per ticket 01) or a
  state that persists until the user has drawn once.
- Where this leaves the tray icon: entry point, status indicator, or both.

03 ruled out new hotkeys and on-canvas controls; both rulings hold here. Reads 03's
answer first.
