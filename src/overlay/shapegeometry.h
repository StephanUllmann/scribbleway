#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVariantMap>

// Every per-type geometry question about a Shape map, answered in one place. Each Shape
// type stores its geometry differently (x/y/width/height, two endpoints, or a point
// list), and the controller, Excalidraw Compatibility and QML all ask the same questions.
namespace ShapeGeometry {

// The box around a shape.
QRectF bounds(const QVariantMap &shape);
// Moves a shape by delta.
void translate(QVariantMap &shape, QPointF delta);
// Whether a rubber-band selection rect touches the shape.
bool intersects(const QVariantMap &shape, const QRectF &rect);

bool supportsAttachedText(const QString &type);
// Where a shape's attached text sits on screen, in the editor and in the Excalidraw export.
QRectF attachedTextRect(const QVariantMap &shape, int fallbackFontSize);

// Whether arrows can bind to the shape's outline (rectangles and ellipses).
bool hasOutline(const QString &type);
QPointF nearestOutlinePoint(const QVariantMap &shape, QPointF p);
// A binding's focus: how far round the outline a point sits, from 0 to 1.
double focusAt(const QVariantMap &shape, QPointF p);
QPointF pointAtFocus(const QVariantMap &shape, double focus);

} // namespace ShapeGeometry
