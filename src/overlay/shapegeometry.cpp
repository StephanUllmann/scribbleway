#include "shapegeometry.h"
#include "shapesmodel.h"
#include <QLineF>
#include <QPolygonF>
#include <QtMath>
#include <cmath>
#include <numbers>

namespace {

bool isLinear(const QString &type)
{
    return type == QStringLiteral("line") || type == QStringLiteral("arrow");
}

bool isBox(const QString &type)
{
    return type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text");
}

QLineF lineOf(const QVariantMap &shape)
{
    return QLineF(shape.value(QStringLiteral("fromX")).toDouble(), shape.value(QStringLiteral("fromY")).toDouble(),
                  shape.value(QStringLiteral("toX")).toDouble(), shape.value(QStringLiteral("toY")).toDouble());
}

QPointF nearestPointOnRect(const QRectF &r, QPointF p)
{
    // Nearest point on each edge, then keep the closest of the four.
    const QPointF candidates[] = {
        {qBound(r.x(), p.x(), r.x() + r.width()), r.y()},
        {qBound(r.x(), p.x(), r.x() + r.width()), r.y() + r.height()},
        {r.x(), qBound(r.y(), p.y(), r.y() + r.height())},
        {r.x() + r.width(), qBound(r.y(), p.y(), r.y() + r.height())},
    };
    auto dist2 = [p](QPointF c) { return QPointF::dotProduct(c - p, c - p); };
    QPointF best = candidates[0];
    double bestDist = dist2(best);
    for (int i = 1; i < 4; ++i) {
        const double d = dist2(candidates[i]);
        if (d < bestDist) {
            bestDist = d;
            best = candidates[i];
        }
    }
    return best;
}

QPointF nearestPointOnEllipse(const QRectF &r, QPointF p)
{
    const double cx = r.x() + r.width() / 2.0, cy = r.y() + r.height() / 2.0;
    const double a = r.width() / 2.0, b = r.height() / 2.0;
    const double lx = p.x() - cx, ly = p.y() - cy;

    if (qFuzzyIsNull(lx) && qFuzzyIsNull(ly)) {
        return QPointF(cx, cy - b);
    }

    // Newton steps on the parametric angle, starting from the point's own angle.
    double theta = qAtan2(ly / b, lx / a);
    for (int i = 0; i < 4; ++i) {
        const double ct = qCos(theta), st = qSin(theta);
        const double dx = lx - a * ct, dy = ly - b * st;
        const double dtx = -a * st, dty = b * ct;
        const double denom = dtx * dtx + dty * dty;
        if (qFuzzyIsNull(denom)) break;
        theta += (dx * dtx + dy * dty) / denom;
    }
    return QPointF(cx + a * qCos(theta), cy + b * qSin(theta));
}

} // namespace

namespace ShapeGeometry {

QRectF bounds(const QVariantMap &shape)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    if (isLinear(type)) {
        const QLineF l = lineOf(shape);
        return QRectF(l.p1(), l.p2()).normalized();
    }
    if (type == QStringLiteral("freehand")) {
        QPolygonF points;
        for (const QVariant &pv : shape.value(QStringLiteral("points")).toList()) {
            points.append(pv.toPointF());
        }
        return points.boundingRect();
    }
    return QRectF(shape.value(QStringLiteral("x")).toDouble(), shape.value(QStringLiteral("y")).toDouble(),
                  shape.value(QStringLiteral("width")).toDouble(), shape.value(QStringLiteral("height")).toDouble());
}

void translate(QVariantMap &shape, QPointF delta)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    auto shift = [&shape, delta](QLatin1String xKey, QLatin1String yKey) {
        shape[xKey] = shape.value(xKey).toDouble() + delta.x();
        shape[yKey] = shape.value(yKey).toDouble() + delta.y();
    };

    if (isBox(type)) {
        shift(QLatin1String("x"), QLatin1String("y"));
    } else if (isLinear(type)) {
        shift(QLatin1String("fromX"), QLatin1String("fromY"));
        shift(QLatin1String("toX"), QLatin1String("toY"));
    } else if (type == QStringLiteral("freehand")) {
        QVariantList newPoints;
        const QVariantList points = shape.value(QStringLiteral("points")).toList();
        for (const QVariant &pv : points) {
            newPoints.append(pv.toPointF() + delta);
        }
        shape.insert(QStringLiteral("points"), newPoints);
    }
}

bool intersects(const QVariantMap &shape, const QRectF &rect)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    if (isBox(type)) {
        const QRectF shapeRect = bounds(shape);
        return rect.intersects(shapeRect) || rect.contains(shapeRect);
    }
    if (isLinear(type)) {
        const QLineF l = lineOf(shape);
        if (rect.contains(l.p1()) || rect.contains(l.p2())) {
            return true;
        }
        const QLineF edges[] = {
            {rect.topLeft(), rect.bottomLeft()}, {rect.topRight(), rect.bottomRight()},
            {rect.topLeft(), rect.topRight()}, {rect.bottomLeft(), rect.bottomRight()},
        };
        for (const QLineF &edge : edges) {
            if (l.intersects(edge) == QLineF::BoundedIntersection) {
                return true;
            }
        }
        return false;
    }
    if (type == QStringLiteral("freehand")) {
        for (const QVariant &pv : shape.value(QStringLiteral("points")).toList()) {
            if (rect.contains(pv.toPointF())) {
                return true;
            }
        }
    }
    return false;
}

bool supportsAttachedText(const QString &type)
{
    return type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || isLinear(type);
}

QRectF attachedTextRect(const QVariantMap &shape, int fallbackFontSize)
{
    // The attached text's own size first, as the label on screen renders it.
    const int fontSize = ShapesModel::firstAttachedTextBinding(shape)
        .value(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), fallbackFontSize)).toInt();
    const QString type = shape.value(QStringLiteral("type")).toString();

    if (isLinear(type)) {
        const QLineF l = lineOf(shape);
        const double w = qMax(80.0, l.length() * 0.7);
        const double h = qMax(32.0, fontSize * 1.6);
        return QRectF(l.center().x() - w / 2.0, l.center().y() - h / 2.0, w, h);
    }

    // Inside an ellipse the text keeps to the square inscribed in it.
    const double pad = 12.0;
    const QRectF r = bounds(shape);
    const bool ellipse = type == QStringLiteral("ellipse");
    const double ix = ellipse ? r.width() * (1.0 - M_SQRT1_2) / 2.0 + pad : pad;
    const double iy = ellipse ? r.height() * (1.0 - M_SQRT1_2) / 2.0 + pad : pad;
    return QRectF(r.x() + ix, r.y() + iy, qMax(40.0, r.width() - ix * 2.0), qMax(24.0, r.height() - iy * 2.0));
}

bool hasOutline(const QString &type)
{
    return type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse");
}

QPointF nearestOutlinePoint(const QVariantMap &shape, QPointF p)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("rectangle")) {
        return nearestPointOnRect(bounds(shape), p);
    }
    if (type == QStringLiteral("ellipse")) {
        return nearestPointOnEllipse(bounds(shape), p);
    }
    return QPointF();
}

double focusAt(const QVariantMap &shape, QPointF p)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    const QRectF r = bounds(shape);
    const double rx = r.x(), ry = r.y(), rw = r.width(), rh = r.height();

    if (type == QStringLiteral("rectangle")) {
        const QPointF sp = nearestPointOnRect(r, p);
        const double perim = 2.0 * (rw + rh);
        if (perim < 1e-6) return 0.0;

        // Measured clockwise from the middle of the top edge.
        double d = 0.0;
        if (qFuzzyCompare(sp.y(), ry)) {
            d = sp.x() - (rx + rw / 2.0);
            if (d < 0) d += perim;
        } else if (qFuzzyCompare(sp.x(), rx + rw)) {
            d = rw / 2.0 + (sp.y() - ry);
        } else if (qFuzzyCompare(sp.y(), ry + rh)) {
            d = rw / 2.0 + rh + (rx + rw - sp.x());
        } else {
            d = rw / 2.0 + rh + rw + (ry + rh - sp.y());
        }
        return d / perim;
    }
    if (type == QStringLiteral("ellipse")) {
        const double a = rw / 2.0, b = rh / 2.0;
        if (a <= 0.0 || b <= 0.0) return 0.0;
        // The parametric angle, the same one pointAtFocus() turns back into a point.
        double angle = qAtan2((p.y() - (ry + b)) / b, (p.x() - (rx + a)) / a);
        if (angle < 0) angle += 2.0 * std::numbers::pi;
        return angle / (2.0 * std::numbers::pi);
    }
    return 0.0;
}

QPointF pointAtFocus(const QVariantMap &shape, double focus)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    const QRectF r = bounds(shape);
    const double rx = r.x(), ry = r.y(), rw = r.width(), rh = r.height();

    if (type == QStringLiteral("rectangle")) {
        double d = focus * 2.0 * (rw + rh);
        const double topCenter = rx + rw / 2.0;
        if (d <= rw / 2.0) return QPointF(topCenter + d, ry);
        d -= rw / 2.0;
        if (d <= rh) return QPointF(rx + rw, ry + d);
        d -= rh;
        if (d <= rw) return QPointF(rx + rw - d, ry + rh);
        d -= rw;
        if (d <= rh) return QPointF(rx, ry + rh - d);
        d -= rh;
        return QPointF(rx + d, ry);
    }
    if (type == QStringLiteral("ellipse")) {
        const double angle = focus * 2.0 * std::numbers::pi;
        return QPointF(rx + rw / 2.0 + rw / 2.0 * qCos(angle), ry + rh / 2.0 + rh / 2.0 * qSin(angle));
    }
    return QPointF();
}

} // namespace ShapeGeometry
