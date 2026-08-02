# How do comparable tools give a live presenter a control surface?

Type: research
Status: resolved
Blocked by: —
Map: ../map.md

## Question

Scribbleway's tray popup currently holds ~20 controls in one 432×550 scrolling panel.
For a presenter mid-screen-share that is the wrong shape. Before deciding what our hot
path is (ticket 03), gather how comparable tools solve the same problem.

Investigate, against primary sources (official docs, source, or the running app —
not blog round-ups):

- **Excalidraw** — already our clipboard-compat reference. How does its tool palette and
  property panel split? What's always visible, what's one keystroke away, what's hidden?
- **Gromit-MPX** — the closest Linux prior art for screen annotation. What is its entire
  control surface, and how much of it is hotkey-only?
- **Epic Pen / ZoomIt** — the Windows presenter tools people actually use live.
- **Krita's pop-up palette** — the strongest prior art for "reach a colour and a brush
  without moving your eyes", and it is Qt.
- **macOS Screen Sharing / Preview markup or OBS annotation**, if they have anything to say.

For each, answer concretely:
1. What controls are reachable **without reading the screen** (hotkey, gesture, muscle memory)?
2. What lives **on the canvas** vs in a **panel** vs **hidden entirely**?
3. How are advanced/rare properties (line style, roughness, opacity) disclosed?
4. How many decisions does a first-time user face before their first stroke?

Deliverable: one Markdown file in `docs/` (match the convention of
`docs/cross-platform-findings.md`), citing each claim's source. Do not propose a design
for Scribbleway — that is ticket 03's job. Report what exists.

## Answer

Findings: [docs/presenter-control-surfaces.md](../../../docs/presenter-control-surfaces.md)

Four patterns recur across two or more of the surveyed tools:

1. **Modifier/letter-key switching without opening a panel.** Tool *and* colour change
   blind. ZoomIt is the densest example (single-letter colours, held-modifier shapes,
   `Ctrl+scroll` for size); Gromit-MPX switches tool by holding a modifier rather than
   selecting one persistently.
2. **Chromeless at runtime, setup front-loaded.** Gromit-MPX and ZoomIt are the only two
   with no persistent on-screen panel at all — Gromit-MPX pushes all configuration to an
   external file, ZoomIt to a one-time setup dialog.
3. **Cursor-anchored, not fixed-location, control surfaces.** Krita's pop-up palette
   spawns at the cursor on right-click. Closest architectural analogue we have, and it is
   Qt.
4. **A dismissible toolbar as escape hatch, not primary path.** Epic Pen documents a
   named "Ghost Mode" for shortcut-only, panel-free operation — an explicit admission
   that a visible toolbar interrupts flow.

Notable gap in the closest reference: **Excalidraw hotkeys every tool but deliberately
does not hotkey colour or stroke width** — a still-open feature request confirms it. So
the tool we are clipboard-compatible with does not solve the presenter colour problem,
and copying it wholesale would not answer ticket 03.

Dead ends worth not re-checking: OBS has no native annotation tool; classic macOS Screen
Sharing has none either (FaceTime screen-share draws, but ephemerally with auto-fade).

Not established from primary sources, flagged in the document rather than guessed:
Epic Pen's full shortcut list and its opacity/line-style controls; Excalidraw's default
tool at launch and its narrow-viewport panel behaviour; FaceTime annotation's access
gesture and palette; Krita's launch-to-first-stroke decision count.
