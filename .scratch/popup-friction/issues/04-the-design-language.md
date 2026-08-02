# The design language

Type: grilling
Status: open
Blocked by: 03 (02 resolved)
Map: ../map.md

## Question

Scribbleway has no design language — that is the stated root of "no hierarchy, illegible
icons, everything crammed". Produce one page of rules that any future contributor can
apply without design judgement.

Decide, given ticket 02's facts and ticket 03's split:

- **Colour source**: does the UI follow the system palette, own its chrome entirely (the
  `ToolCursorBadge` approach), or a stated hybrid — and what is the rule for foreground
  contrast against whatever background it lands on?
- **Iconography**: themed SVG with recolouring, or hand-drawn `Canvas`/`Shapes` glyphs?
  One approach, applied everywhere, including the tool badge that already exists.
- **Spacing and size scale**: the `theme` QtObject in `FullRepresentation.qml` already
  declares `gridUnit`/`smallSpacing`/`largeSpacing`/`iconSmall|Medium|Large` and then
  half of it goes unused. Either make the scale real or cut it.
- **Grouping and hierarchy**: how a section is signalled, how a primary action reads
  differently from a secondary one, how "this affects the selection" is distinguished
  from "this sets a default" — a distinction the current panel makes with one bold label.
- **Density**: icon-only, label-only, or both — and where each applies. Five icon+text
  buttons in one row is the current worst case.

The output is prose rules plus the concrete values, written so ticket 05 can build
against it and the eventual spec can quote it verbatim. Not a mockup.
