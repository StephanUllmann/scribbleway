# Scribbleway

A vector-based drawing overlay application that allows users to sketch on top of their screen, edit shapes, and copy/paste elements.

## Language

**Shape**:
A distinct drawing element placed on the canvas, such as a rectangle, ellipse, line, arrow, freehand path, or text block.
_Avoid_: Drawing object, canvas element

**Clipboard**:
The system clipboard interface used for sharing serialized vector shapes within Scribbleway or with external applications.
_Avoid_: Pasteboard

**Excalidraw Compatibility**:
The bidirectional translation of serialized shapes between Scribbleway's internal format and Excalidraw's clipboard schema.
_Avoid_: External paste

**Roughness**:
The degree of hand-drawn aesthetic asymmetry applied to a shape's stroke, matching Excalidraw's sloppiness levels (0 for neat/Architect, 1 for medium/Artist, 2 for cartoonish/Cartoonist).
_Avoid_: Sloppiness, hand-drawn level
