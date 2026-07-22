# Attached Text Binding Design

## Goal

Add Excalidraw-style attached text for `rectangle`, `ellipse`, `line`, and `arrow` shapes. Double-clicking a supported shape creates or edits a centered text box that is owned by the shape: it is not selectable on its own, moves with the shape, and follows the shape color/font styling.

## Decisions

- Use a general binding layer on shapes, not separate selectable text model rows.
- Store attached text as container-owned binding data so Scribbleway selection, movement, and deletion remain container-first.
- Preserve Excalidraw compatibility where Excalidraw supports it: rectangles, ellipses, and arrows export/import as a bound text element linked through `boundElements` and `containerId`.
- Support line-attached text locally. Excalidraw does not support bound text on plain lines, so lines export as the line plus a separate centered text element.
- Attached text style is owned by the container: stroke color, opacity, font family, and font size come from the shape.
- Empty commit removes the attached text binding and leaves the container shape intact.
- Text wraps inside the available centered text area. Overflow remains visible and never resizes the container.

## Current Code Context

- `src/overlay/qml/shapes/BaseShape.qml` already owns shape mouse interaction and emits `doubleClicked`.
- `src/overlay/qml/main.qml` already has a reusable `Controls.TextArea` editing path for standalone text shapes.
- `src/overlay/shapesmodel.h/.cpp` stores each shape as a `QVariantMap` and exposes roles for QML.
- `src/overlay/overlaycontroller.cpp` owns selection, updates, undo transactions, copy/paste, and Excalidraw JSON conversion.
- Existing arrow endpoint binding uses `startBinding`, `endBinding`, and `boundElementIds`; attached text should share cleanup/copy/delete concepts with those bindings instead of creating a second unrelated mechanism.

## Data Model

Each shape may contain a generic `bindings` list. The first binding type is attached text:

```json
{
  "type": "attachedText",
  "id": "stable-child-id",
  "text": "Hello",
  "fontFamily": "monospace",
  "fontSize": 20,
  "textAlign": "center",
  "verticalAlign": "middle"
}
```

Container fields continue to own visual style:

- `color` controls text color.
- `opacity` controls text opacity.
- `fontFamily` and `fontSize` on the container control attached text typography.

Implementation may keep existing arrow endpoint fields during the first pass, but binding helpers must become the single place for adding, removing, copying, deleting, and exporting child relationships. This avoids a parallel one-off attached-text path.

## Interaction Behavior

### Create and edit

- Double-click a supported unlocked shape in select mode.
- The clicked shape becomes selected.
- If no attached text exists, the controller creates an attached-text binding with an empty `text` value and current container font defaults.
- The existing QML text editor opens over the derived attached-text bounds and selects the current text.
- Double-clicking the same shape later opens the editor with the existing text.

### Commit

- Enter commits.
- Shift+Enter inserts a newline.
- Tab commits.
- Escape keeps the current Scribbleway behavior: it commits through `cancelInteraction()`.
- If committed text is empty after trimming, remove the attached-text binding and keep the container shape.

### Selection

- Attached text is never an independent selectable item.
- Selection handles, delete, move, nudge, copy, lock, and color/font controls target the container only.
- Multi-selection updates work because attached text style and position derive from each selected container.

## Layout Rules

### Rectangles

- Render text horizontally centered inside `x/y/width/height` minus padding.
- Wrap to `width - padding * 2`.
- Overflow can extend outside the rectangle vertically.

### Ellipses

- Render text horizontally centered inside the largest centered rectangle that fits inside the ellipse.
- Use the Excalidraw-compatible approximation: available width and height are based on `sqrt(2) / 2` of ellipse dimensions minus padding.
- Overflow remains visible.

### Lines and arrows

- Render text horizontally at the segment midpoint.
- Text angle stays `0`, matching Excalidraw arrow bound text behavior.
- Wrap to a practical label width derived from line/arrow length with a minimum width based on font size.
- Overflow remains visible.

## Excalidraw Clipboard Mapping

### Export rectangle, ellipse, and arrow attached text

When a selected container has attached text:

1. Export the container element normally.
2. Add `{ "id": attachedTextId, "type": "text" }` to the container `boundElements` array.
3. Export a sibling Excalidraw text element immediately after the container with:
   - `type: "text"`
   - `containerId: container.id`
   - `text` and `originalText` from the attached text
   - `textAlign: "center"`
   - `verticalAlign: "middle"`
   - `autoResize: true`
   - `angle: 0` for arrows
   - font/color/opacity copied from the container

### Import rectangle, ellipse, and arrow bound text

When Excalidraw clipboard JSON contains a text element whose `containerId` points to an imported rectangle, ellipse, or arrow:

1. Do not add that text element as a standalone Scribbleway text shape.
2. Convert it into the container's attached-text binding.
3. Preserve the text id as the binding id for round-trip stability within the paste operation.
4. Preserve text content and supported font settings.

### Export lines with attached text

Excalidraw has no bound text for plain line elements. Export a Scribbleway line with attached text as:

- the line element, unchanged;
- a separate centered text element at the line midpoint;
- no `containerId` link.

When exporting a line with attached text, include Scribbleway metadata on the fallback text element: `customData: { "scribbleway": { "attachedTextFor": line.id } }`. Excalidraw ignores this metadata visually. When Scribbleway imports a line and a text element with matching metadata in the same clipboard payload, convert that text element back into the line's attached-text binding instead of a standalone text shape.

## File-Level Design

### `src/overlay/shapesmodel.h/.cpp`

- Add `BindingsRole` for the full `bindings` list and `AttachedTextRole` for the first `attachedText` binding.
- Return `bindings` from `data()`.
- Add role names in `roleNames()`.
- Emit changed roles when `bindings`, `fontFamily`, or `fontSize` change.
- Keep undo/redo transaction semantics unchanged.

### `src/overlay/overlaycontroller.h/.cpp`

- Add invokable helpers for QML:
  - `QVariantMap attachedTextForShape(int index) const`
  - `QVariantMap ensureAttachedTextForShape(int index)`
  - `void setAttachedText(int index, const QString &text)`
  - `void removeAttachedText(int index)`
- Add internal helpers for binding operations:
  - find an attached-text binding by shape index;
  - create a stable attached-text id;
  - remove attached text without touching the container;
  - include attached text when copying a selected container;
  - drop or convert attached text when deleting/importing shapes.
- Update `getSelectionState()` and `setSelectedIndex()` so selected containers expose font family and font size for applet controls.
- Update Excalidraw conversion to emit/import bound text for rectangles, ellipses, and arrows.
- Preserve existing arrow endpoint bindings and migrate helper code only where it reduces duplicate cleanup/export logic.

### `src/overlay/qml/main.qml`

- Generalize `startTextEditing(shapeIndex)` into two paths:
  - standalone text shape editing;
  - attached text editing for a container shape.
- Track edit target as shape index plus edit kind, not just `editingShapeIndex`.
- Place the editor at the derived attached-text bounds.
- On commit, call `setAttachedText` or `removeAttachedText` for attached text targets.
- Keep existing standalone text behavior unchanged.

### `src/overlay/qml/shapes/BaseShape.qml`

- Render attached text as a `Text` item in the base shape layer.
- Keep the text item non-interactive (`enabled: false`) so the existing shape mouse area receives clicks.
- Call the attached-text edit path on double-click for supported container types.
- Keep selection handles above the attached text.

### Shape QML files

- `RectangleShape.qml`, `EllipseShape.qml`, `LineShape.qml`, and `ArrowShape.qml` continue to pass geometry through existing `BaseShape` properties.
- Attached-text rendering lives in `BaseShape.qml`; shape-specific files do not render text themselves.

### `tests/shapesmodeltest.cpp`

Add focused tests for:

- creating attached text on rectangle, ellipse, line, and arrow containers;
- editing existing attached text;
- committing empty text removes only the binding;
- attached text is present in model data but not as a selectable row;
- copying rectangle/ellipse/arrow attached text produces Excalidraw `boundElements` plus a `text` element with `containerId`;
- pasting Excalidraw bound text converts it into a container binding;
- copying a line with attached text produces a separate centered text element;
- undo/redo restores attached-text creation/edit/removal.

## Verification Plan

- Run the focused Qt test binary for `ShapesModelTest` after adding tests.
- Smoke test the overlay manually:
  1. draw rectangle, ellipse, line, and arrow;
  2. double-click each shape and enter text;
  3. move and nudge each shape;
  4. change stroke color, opacity, font size, and font family;
  5. double-click again and edit existing text;
  6. commit empty text and confirm only attached text disappears;
  7. copy/paste rectangle, ellipse, arrow, and line cases;
  8. paste Excalidraw JSON containing bound text and confirm it becomes attached text.
- After each successful build, rebuild the `.deb` package, matching the project preference.

## Risks

- The selected general binding approach touches more controller paths than an embedded `boundText` field. Keep the first implementation limited to attached text plus existing arrow binding cleanup.
- Excalidraw line text cannot fully round-trip as bound text because Excalidraw does not support line containers for text.
- QML text measurement can differ from Excalidraw. The acceptance target is centered, readable text with compatible JSON fields, not pixel-perfect Excalidraw layout.
- Current Escape behavior commits text through `cancelInteraction`; true cancel is outside this feature.
