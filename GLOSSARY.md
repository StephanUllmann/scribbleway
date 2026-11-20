# Scribbleway

A vector-based drawing overlay application that allows users to sketch on top of their screen, edit shapes, and copy/paste elements.

## Language

**Shape**:
A distinct drawing element placed on the canvas, such as a rectangle, ellipse, line, arrow, freehand path, or text block.
_Avoid_: Drawing object, canvas element

**Selection**:
The set of Shapes the user has picked, held by id outside the Shapes themselves, so it never enters the undo history or the clipboard. One selected Shape is the **primary**: the last one picked, which the Tray Popup shows and raise/lower move.
_Avoid_: Selected flag, current shape

**Draft**:
The Shape the user is drawing, from the first drag until release adds it to the canvas or it is abandoned. It is drawn by the same delegates as every other Shape, never enters the undo history, and is never selected.
_Avoid_: Preview, ghost shape

**Clipboard**:
The system clipboard interface used for sharing serialized vector shapes within Scribbleway or with external applications.
_Avoid_: Pasteboard

**Excalidraw Compatibility**:
The bidirectional translation of serialized shapes between Scribbleway's internal format and Excalidraw's clipboard schema.
_Avoid_: External paste

**Roughness**:
The degree of hand-drawn aesthetic asymmetry applied to a shape's stroke, matching Excalidraw's sloppiness levels (0 for neat/Architect, 1 for medium/Artist, 2 for cartoonish/Cartoonist).
_Avoid_: Sloppiness, hand-drawn level

**Tray Popup**:
The anchored, non-fullscreen popup window shown when the user clicks the system tray icon, containing the tool/shape/settings controls formerly hosted by the Plasma applet. Toggles open/closed on repeated clicks, dismisses like a native tray menu. Replaces the Plasma applet across all platforms (KDE included).
_Avoid_: Applet, Plasma widget, popup menu
