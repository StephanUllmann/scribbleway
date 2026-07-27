# Phase 2: Arch/Hyprland Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Confirm the Phase-1 single-process Tray Popup build actually works on Arch/Hyprland, fix what's broken, and ship a real, up-to-date Arch package.

**Architecture:** No new subsystems. This phase updates `packaging/arch/` to match Phase 1's dependency changes, then runs the resulting binary on real Hyprland hardware against a concrete verification checklist. Per the grilling session's phase order, this was expected to be "mostly a testing pass" — that held for the overlay/rendering path (`LayerShellQt` is protocol-generic, confirmed unmodified-and-working on Hyprland during Phase 1 planning), but research for this plan surfaced one real, previously-unflagged risk: **`QSystemTrayIcon` requires a running `StatusNotifierHost`** (e.g. Waybar's tray module), which Hyprland does not provide out of the box and which has well-documented reliability problems even when configured (see Sources). Combined with Wayland global hotkeys being deferred (Q5 of the grilling session), a Hyprland user with no tray host configured would have **no way to reach Scribbleway's controls at all**. This plan treats that as the primary risk to validate, not an afterthought.

**Tech Stack:** Same as Phase 1 — C++20, Qt6, KF6::GlobalAccel (inert on Hyprland, not removed), LayerShellQt, CMake/ECM, Docker (for the Arch package build).

## Global Constraints

- Assumes Phase 1 (`docs/superpowers/plans/2026-07-27-cross-platform-phase1-tray-popup.md`) is fully merged — this plan's code references (`OverlayController` properties, `TrayPopup.qml`, `main.cpp`'s tray-icon wiring) are the ones that plan produces.
- No GNOME/Wayland, no X11 (unchanged from Phase 1).
- Wayland global hotkeys via `xdg-desktop-portal` remain explicitly deferred — this phase does not implement them (that's the user's own follow-up spike, per Q5). This phase only makes sure their absence is survivable.
- Some tasks in this plan are empirical validation on real hardware, not new code with a knowable-in-advance correct implementation. Where that's true, the task is a concrete checklist with a defined outcome ("all pass" / "list what failed"), not a fabricated fix for a bug that may not exist. Any failure found becomes its own follow-up commit using `superpowers:systematic-debugging`, not something this plan pre-writes.

---

## File Structure

**Modify:**
- `packaging/arch/PKGBUILD` — drop `plasma-desktop` and `kdbusaddons` from `depends`, matching Phase 1's CMake changes.
- `packaging/arch/Dockerfile.arch` — drop the same packages from the `pacman -S` build-dependency list.
- `src/overlay/main.cpp` — add a `QSystemTrayIcon::isSystemTrayAvailable()` guard with a clear diagnostic message (Task 3).
- `README.md` — add a short "Running on Hyprland" note about the tray-host requirement (Task 5).

---

### Task 1: Update Arch packaging to match Phase 1's dependency changes

**Files:**
- Modify: `packaging/arch/PKGBUILD`
- Modify: `packaging/arch/Dockerfile.arch`

**Interfaces:** None — packaging metadata only.

- [ ] **Step 1: Update `PKGBUILD`**

Change:

```
depends=('plasma-desktop' 'layer-shell-qt' 'kglobalaccel' 'kdbusaddons' 'qt6-declarative' 'qt6-base')
```

to:

```
depends=('layer-shell-qt' 'kglobalaccel' 'qt6-declarative' 'qt6-base')
```

(`qt6-base` on Arch already provides `QtWidgets`, needed for `QSystemTrayIcon` since Phase 1 — no new dependency entry required.)

- [ ] **Step 2: Update `Dockerfile.arch`**

Change:

```dockerfile
RUN pacman -Sy --noconfirm archlinux-keyring && \
    pacman -Syu --noconfirm && \
    pacman -S --noconfirm base-devel git cmake extra-cmake-modules plasma-desktop layer-shell-qt kglobalaccel kdbusaddons qt6-declarative qt6-base
```

to:

```dockerfile
RUN pacman -Sy --noconfirm archlinux-keyring && \
    pacman -Syu --noconfirm && \
    pacman -S --noconfirm base-devel git cmake extra-cmake-modules layer-shell-qt kglobalaccel qt6-declarative qt6-base
```

- [ ] **Step 3: Build the package via the existing Docker flow**

```bash
./packaging/arch/build_arch_docker.sh
```

Expected: succeeds, produces `packaging/arch/scribbleway-git-*.pkg.tar.zst` without pulling in `plasma-desktop` or `kdbusaddons`.

- [ ] **Step 4: Commit**

```bash
git add packaging/arch/PKGBUILD packaging/arch/Dockerfile.arch
git commit -m "packaging(arch): drop plasma-desktop and kdbusaddons, matching Phase 1"
```

(Don't commit the built `.pkg.tar.zst` artifacts — they're already tracked in git today, which is unusual for build output; leave that as-is unless you want to `git rm --cached` them separately, outside this plan's scope.)

---

### Task 2: Install on the real Arch/Hyprland machine

**Files:** None — deployment step.

- [ ] **Step 1: Install the package built in Task 1**

```bash
sudo pacman -U packaging/arch/scribbleway-git-*.pkg.tar.zst
```

- [ ] **Step 2: Launch it directly and confirm it doesn't crash**

```bash
scribbleway-overlay
```

Expected: process starts and stays running (check with `pgrep -f scribbleway-overlay` in another terminal). If it exits immediately, capture the terminal output — this is the first real signal about what, if anything, breaks under Hyprland.

---

### Task 3: System tray availability guard

Add this defensively **before** the manual verification in Task 4, since if the tray genuinely isn't reachable, you want a clear log line telling you why, not silent nothing.

**Files:**
- Modify: `src/overlay/main.cpp`

**Interfaces:**
- Consumes: `QSystemTrayIcon::isSystemTrayAvailable()` (Qt6::Widgets, already linked since Phase 1).

- [ ] **Step 1: Add the guard**

In `main.cpp`, immediately before `trayIcon.show();` (the line added in Phase 1's Task 5):

```cpp
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "No system tray detected. Scribbleway's controls (color, tools, "
                      "shortcuts, shape list) will not be reachable without one. "
                      "On Hyprland, enable a StatusNotifierHost — e.g. Waybar's "
                      "\"tray\" module — then restart Scribbleway.";
    }
    trayIcon.show();
```

`trayIcon.show()` is called regardless — `isSystemTrayAvailable()` can be a false negative on some setups (the Waybar/Hyprland `StatusNotifierHost` registration has a documented timing race — see Sources), so refusing to show the icon at all would risk permanently hiding a tray that becomes available a moment later.

- [ ] **Step 2: Rebuild and reinstall**

```bash
cmake --build build --target scribbleway-overlay -j"$(nproc)"
sudo pacman -U packaging/arch/scribbleway-git-*.pkg.tar.zst   # after re-running Task 1 Step 3 to rebuild the package
```

Or, for a faster local-only loop while iterating, just run the freshly built binary directly: `./build/bin/scribbleway-overlay` (no need to repackage for every check).

- [ ] **Step 3: Commit**

```bash
git add src/overlay/main.cpp
git commit -m "feat: warn when no system tray host is available"
```

---

### Task 4: Manual verification checklist on Hyprland

This is the core of Phase 2. Run through every item; record pass/fail for each rather than stopping at the first failure, since later items are independent of earlier ones.

- [ ] **Tray icon appears** in whatever tray host is configured (Waybar or otherwise). If it doesn't: check `journalctl --user -f` or the terminal running `scribbleway-overlay` for the warning added in Task 3, then check whether a `StatusNotifierHost` is actually running (`busctl --user list | grep StatusNotifierHost`).
- [ ] **Clicking the tray icon opens the popup** at a fixed, non-fullscreen size near the bottom-right of the screen (same behavior verified on KDE/Wayland during Phase 1).
- [ ] **Clicking the tray icon again closes the popup.** Clicking outside it also closes it.
- [ ] **Popup controls work**: change color, stroke width, tool; confirm the change is visible in the overlay.
- [ ] **Undo/redo/clear/delete/raise/lower** all work from the popup.
- [ ] **The shape list** in the popup reflects shapes drawn on screen, and clicking a list entry selects the corresponding shape.
- [ ] **Global keyboard shortcut** (`Meta+Shift+X` — `action_activate_select_mode`, registered via `KGlobalAccel` in `main.cpp`) is **expected to silently do nothing** on Hyprland (no `kglobalacceld` daemon running outside Plasma). Confirm: (a) it does nothing, and (b) — critically — this does **not** crash or hang the app; check `pgrep -f scribbleway-overlay` still shows the process after pressing it.
- [ ] **The overlay itself** (transparent fullscreen layer-shell window, draw/select mode, shape rendering, click-through in passthrough mode) behaves identically to the KDE/Wayland build. This exercises `LayerShellQt`, which was already confirmed protocol-generic and unmodified — this is a confirmation pass, not expected to surface anything new.
- [ ] **Quitting** (however you choose to expose it — check whether Phase 1 added a quit path to the tray icon's context menu; if not, note that as a gap, not a Phase 2 blocker) doesn't leave orphaned processes or corrupt `~/.config` state for next launch.

- [ ] **Record the results.** If everything passes, proceed to Task 5. If something fails, stop here and open a `superpowers:systematic-debugging` pass on the specific failure before continuing — don't guess at a fix inside this plan.

---

### Task 5: Document the tray-host requirement

**Files:**
- Modify: `README.md`

**Interfaces:** None — documentation only.

- [ ] **Step 1: Add a short section**

```markdown
## Running on Hyprland (or other non-Plasma Wayland compositors)

Scribbleway's controls live in a system tray popup, which requires a running
`StatusNotifierHost`. Hyprland doesn't provide one itself — enable Waybar's
`tray` module (or an equivalent) before launching Scribbleway, or the tray
icon won't appear anywhere.

Global keyboard shortcuts are currently KDE/Plasma-only; on Hyprland, use the
tray popup to switch modes and tools.
```

Place it near the existing installation/usage instructions (read `README.md`'s current structure first to match its heading level and tone).

- [ ] **Step 2: Commit**

```bash
git add README.md
git commit -m "docs: note the StatusNotifierHost requirement for Hyprland"
```

---

## Self-Review

**Spec coverage:**
- "Mostly a testing pass" for Hyprland (per the grilling session's Phase 2 expectation) — Task 4 is exactly that, structured as a checklist rather than fabricated fixes. ✓
- Real Arch package, not just "builds" — Tasks 1–2 update and rebuild the actual `packaging/arch` artifacts and install them on real hardware. ✓
- Global hotkeys deferred, must degrade gracefully, not crash — explicit checklist item in Task 4. ✓
- New risk surfaced during this plan's own research (tray host availability) — Task 3 (code) and Task 5 (docs), not silently absorbed into "should be fine." ✓

**Explicitly out of scope:** implementing `xdg-desktop-portal` GlobalShortcuts (the user's own follow-up spike), a quit menu item if Phase 1 didn't add one (flagged as a gap to note, not fixed here), macOS (Phase 3), Windows, the release/distribution GitHub Actions workflow.

**Placeholder scan:** clean — every step is a real command, real diff, or a checklist item with a defined pass/fail outcome.

Sources for the `StatusNotifierHost`/Hyprland tray risk:
- [Tray module not visible after some time — Waybar #3468](https://github.com/Alexays/Waybar/issues/3468)
- [System tray icon missing in Waybar on Hyprland — KDE Bugtracker #477347](https://bugs.kde.org/show_bug.cgi?id=477347)
- [Missing DBus service for tray — Waybar #2437](https://github.com/Alexays/Waybar/issues/2437)
