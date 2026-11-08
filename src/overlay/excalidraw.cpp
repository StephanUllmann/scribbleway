#include "excalidraw.h"
#include "shapesmodel.h"
#include <QColor>
#include <QJsonArray>
#include <QPointF>
#include <QUuid>
#include <limits>

namespace {
// Excalidraw numbers its fonts: 1 hand-drawn, 2 normal, 3 code.
int excalidrawFont(const QString &fontFamily)
{
    const QString family = fontFamily.toLower();
    if (family.contains(QStringLiteral("code")) || family.contains(QStringLiteral("mono"))) {
        return 3;
    }
    if (family.contains(QStringLiteral("hand")) || family.contains(QStringLiteral("script")) || family.contains(QStringLiteral("virgil"))) {
        return 1;
    }
    return 2;
}

QJsonValue bindingToJson(const QVariantMap &binding)
{
    if (binding.isEmpty()) {
        return QJsonValue::Null;
    }
    return QJsonObject{
        {QStringLiteral("elementId"), binding.value(QStringLiteral("elementId")).toString()},
        {QStringLiteral("focus"), binding.value(QStringLiteral("focus")).toDouble()},
        {QStringLiteral("gap"), binding.value(QStringLiteral("gap")).toDouble()},
    };
}
} // namespace

namespace Excalidraw {

QJsonObject elementFromShape(const QVariantMap &shape)
{
    QJsonObject elem;
    QString type = shape.value(QStringLiteral("type")).toString();
    
    QString excalType = (type == QStringLiteral("freehand")) ? QStringLiteral("freedraw") : type;
    if (excalType != QStringLiteral("rectangle") && excalType != QStringLiteral("ellipse") &&
        excalType != QStringLiteral("text") && excalType != QStringLiteral("line") &&
        excalType != QStringLiteral("arrow") && excalType != QStringLiteral("freedraw")) {
        return elem;
    }
    elem.insert(QStringLiteral("type"), excalType);

    QString id = shape.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces).mid(0, 8);
    }
    elem.insert(QStringLiteral("id"), id);
    
    elem.insert(QStringLiteral("strokeColor"), shape.value(QStringLiteral("color"), QStringLiteral("#000000")).toString());
    elem.insert(QStringLiteral("strokeWidth"), shape.value(QStringLiteral("strokeWidth"), 1.0).toDouble());
    
    double opacityFloat = shape.value(QStringLiteral("opacity"), 1.0).toDouble();
    elem.insert(QStringLiteral("opacity"), qRound(opacityFloat * 100.0));
    
    // Fill → Excalidraw backgroundColor
    {
        const QString fillColor = shape.value(QStringLiteral("fillColor"), QStringLiteral("transparent")).toString();
        const double fillOpacity = shape.value(QStringLiteral("fillOpacity"), 0.0).toDouble();
        const bool noFill = fillColor.isEmpty()
            || fillColor.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0
            || fillOpacity <= 0.0;

        if (noFill) {
            elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
        } else {
            QColor c(fillColor);
            if (!c.isValid()) {
                elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
            } else if (fillOpacity >= 0.999) {
                elem.insert(QStringLiteral("backgroundColor"), c.name(QColor::HexRgb)); // #RRGGBB
            } else {
                c.setAlphaF(fillOpacity);
                // Qt HexArgb is #AARRGGBB; Excalidraw expects #RRGGBBAA
                const QString argb = c.name(QColor::HexArgb); // #AARRGGBB
                const QString rrggbbaa = QStringLiteral("#%1%2")
                    .arg(argb.mid(3, 6))   // RRGGBB
                    .arg(argb.mid(1, 2));  // AA
                elem.insert(QStringLiteral("backgroundColor"), rrggbbaa);
            }
        }
    }
    elem.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("roughness"), shape.value(QStringLiteral("roughness"), 1).toInt());
    elem.insert(QStringLiteral("glow"), shape.value(QStringLiteral("glow"), 0).toInt()); // Ignore/preserve
    elem.insert(QStringLiteral("angle"), 0.0);
    elem.insert(QStringLiteral("isDeleted"), false);
    elem.insert(QStringLiteral("seed"), shape.value(QStringLiteral("seed"), 123456).toInt());
    elem.insert(QStringLiteral("version"), 1);
    elem.insert(QStringLiteral("versionNonce"), 123456789);
    elem.insert(QStringLiteral("updated"), 0);
    elem.insert(QStringLiteral("locked"), shape.value(QStringLiteral("locked"), false).toBool());

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        elem.insert(QStringLiteral("x"), shape.value(QStringLiteral("x")).toDouble());
        elem.insert(QStringLiteral("y"), shape.value(QStringLiteral("y")).toDouble());
        elem.insert(QStringLiteral("width"), shape.value(QStringLiteral("width")).toDouble());
        elem.insert(QStringLiteral("height"), shape.value(QStringLiteral("height")).toDouble());
        
        if (type == QStringLiteral("rectangle")) {
            int borderRadius = shape.value(QStringLiteral("borderRadius"), 0).toInt();
            if (borderRadius > 0) {
                QJsonObject roundnessObj;
                roundnessObj.insert(QStringLiteral("type"), 3);
                roundnessObj.insert(QStringLiteral("value"), borderRadius);
                elem.insert(QStringLiteral("roundness"), roundnessObj);
            } else {
                elem.insert(QStringLiteral("roundness"), QJsonValue::Null);
            }
        } else if (type == QStringLiteral("text")) {
            elem.insert(QStringLiteral("text"), shape.value(QStringLiteral("text")).toString());
            elem.insert(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), 20).toInt());
            
            elem.insert(QStringLiteral("fontFamily"), excalidrawFont(shape.value(QStringLiteral("fontFamily")).toString()));
        }
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
            QJsonArray boundArr;
            for (const QVariant &bid : boundIds) {
                QJsonObject beObj;
                beObj.insert(QStringLiteral("id"), bid.toString());
                beObj.insert(QStringLiteral("type"), QStringLiteral("arrow"));
                boundArr.append(beObj);
            }
            const QVariantMap attached = ShapesModel::firstAttachedTextBinding(shape);
            if (!attached.isEmpty() && !attached.value(QStringLiteral("text")).toString().trimmed().isEmpty()) {
                QString attId = attached.value(QStringLiteral("id")).toString();
                if (attId.isEmpty()) {
                    attId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                }
                QJsonObject beObj;
                beObj.insert(QStringLiteral("id"), attId);
                beObj.insert(QStringLiteral("type"), QStringLiteral("text"));
                boundArr.append(beObj);
            }
            if (!boundArr.isEmpty()) {
                elem.insert(QStringLiteral("boundElements"), boundArr);
            } else {
                elem.insert(QStringLiteral("boundElements"), QJsonValue::Null);
            }
        }
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        double fromX = shape.value(QStringLiteral("fromX")).toDouble();
        double fromY = shape.value(QStringLiteral("fromY")).toDouble();
        double toX = shape.value(QStringLiteral("toX")).toDouble();
        double toY = shape.value(QStringLiteral("toY")).toDouble();
        
        elem.insert(QStringLiteral("x"), fromX);
        elem.insert(QStringLiteral("y"), fromY);
        elem.insert(QStringLiteral("width"), qAbs(toX - fromX));
        elem.insert(QStringLiteral("height"), qAbs(toY - fromY));
        
        QJsonArray pts = { QJsonArray{0.0, 0.0}, QJsonArray{toX - fromX, toY - fromY} };
        elem.insert(QStringLiteral("points"), pts);

        elem.insert(QStringLiteral("startBinding"), bindingToJson(shape.value(QStringLiteral("startBinding")).toMap()));
        elem.insert(QStringLiteral("endBinding"), bindingToJson(shape.value(QStringLiteral("endBinding")).toMap()));
        if (type == QStringLiteral("arrow")) {
            const QVariantMap attached = ShapesModel::firstAttachedTextBinding(shape);
            if (!attached.isEmpty() && !attached.value(QStringLiteral("text")).toString().trimmed().isEmpty()) {
                QString attId = attached.value(QStringLiteral("id")).toString();
                if (attId.isEmpty()) {
                    attId = QUuid::createUuid().toString(QUuid::WithoutBraces);
                }
                QJsonArray boundArr;
                QJsonObject beObj;
                beObj.insert(QStringLiteral("id"), attId);
                beObj.insert(QStringLiteral("type"), QStringLiteral("text"));
                boundArr.append(beObj);
                elem.insert(QStringLiteral("boundElements"), boundArr);
            }
        }
    } else if (type == QStringLiteral("freehand")) {
        QVariantList pointsList = shape.value(QStringLiteral("points")).toList();
        if (pointsList.isEmpty()) {
            return elem;
        }
        
        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();
        
        QList<QPointF> qpts;
        for (const QVariant &v : pointsList) {
            const QPointF p = v.toPointF();
            qpts.append(p);
            minX = qMin(minX, p.x());
            maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
        
        if (qpts.isEmpty()) {
            return elem;
        }
        
        elem.insert(QStringLiteral("x"), minX);
        elem.insert(QStringLiteral("y"), minY);
        elem.insert(QStringLiteral("width"), maxX - minX);
        elem.insert(QStringLiteral("height"), maxY - minY);
        
        QJsonArray pts;
        for (const QPointF &p : qpts) {
            QJsonArray ptVal;
            ptVal.append(p.x() - minX);
            ptVal.append(p.y() - minY);
            pts.append(ptVal);
        }
        elem.insert(QStringLiteral("points"), pts);
    }

    return elem;
}
QJsonObject attachedTextElement(const QVariantMap &shape, const QVariantMap &attachedText, int defaultFontSize)
{
    QJsonObject elem;
    const QString containerType = shape.value(QStringLiteral("type")).toString();
    const QString text = attachedText.value(QStringLiteral("text")).toString();
    if (text.trimmed().isEmpty()) {
        return elem;
    }

    QString id = attachedText.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    elem.insert(QStringLiteral("id"), id);
    elem.insert(QStringLiteral("type"), QStringLiteral("text"));
    elem.insert(QStringLiteral("strokeColor"), shape.value(QStringLiteral("color"), QStringLiteral("#000000")).toString());
    elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
    elem.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("strokeWidth"), 1.0);
    elem.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("roughness"), shape.value(QStringLiteral("roughness"), 1).toInt());
    elem.insert(QStringLiteral("opacity"), qRound(shape.value(QStringLiteral("opacity"), 1.0).toDouble() * 100.0));
    elem.insert(QStringLiteral("angle"), 0.0);
    elem.insert(QStringLiteral("isDeleted"), false);
    elem.insert(QStringLiteral("seed"), attachedText.value(QStringLiteral("seed"), 123456).toInt());
    elem.insert(QStringLiteral("version"), 1);
    elem.insert(QStringLiteral("versionNonce"), 123456789);
    elem.insert(QStringLiteral("updated"), 0);
    elem.insert(QStringLiteral("locked"), shape.value(QStringLiteral("locked"), false).toBool());
    elem.insert(QStringLiteral("text"), text);
    elem.insert(QStringLiteral("originalText"), text);
    elem.insert(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), attachedText.value(QStringLiteral("fontSize"), defaultFontSize)).toInt());
    elem.insert(QStringLiteral("fontFamily"), excalidrawFont(attachedText.value(QStringLiteral("fontFamily"), shape.value(QStringLiteral("fontFamily"))).toString()));
    elem.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
    elem.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));
    elem.insert(QStringLiteral("autoResize"), true);
    elem.insert(QStringLiteral("lineHeight"), 1.25);

    double x = 0.0;
    double y = 0.0;
    double width = 100.0;
    double height = qMax(24, elem.value(QStringLiteral("fontSize")).toInt());
    if (containerType == QStringLiteral("rectangle") || containerType == QStringLiteral("ellipse")) {
        x = shape.value(QStringLiteral("x")).toDouble();
        y = shape.value(QStringLiteral("y")).toDouble();
        width = shape.value(QStringLiteral("width")).toDouble();
        height = shape.value(QStringLiteral("height")).toDouble();
        elem.insert(QStringLiteral("containerId"), shape.value(QStringLiteral("id")).toString());
    } else if (containerType == QStringLiteral("arrow")) {
        const double fromX = shape.value(QStringLiteral("fromX")).toDouble();
        const double fromY = shape.value(QStringLiteral("fromY")).toDouble();
        const double toX = shape.value(QStringLiteral("toX")).toDouble();
        const double toY = shape.value(QStringLiteral("toY")).toDouble();
        width = qMax(80.0, qAbs(toX - fromX) * 0.7);
        x = (fromX + toX) / 2.0 - width / 2.0;
        y = (fromY + toY) / 2.0 - height / 2.0;
        elem.insert(QStringLiteral("containerId"), shape.value(QStringLiteral("id")).toString());
    } else if (containerType == QStringLiteral("line")) {
        const double fromX = shape.value(QStringLiteral("fromX")).toDouble();
        const double fromY = shape.value(QStringLiteral("fromY")).toDouble();
        const double toX = shape.value(QStringLiteral("toX")).toDouble();
        const double toY = shape.value(QStringLiteral("toY")).toDouble();
        width = qMax(80.0, qAbs(toX - fromX) * 0.7);
        x = (fromX + toX) / 2.0 - width / 2.0;
        y = (fromY + toY) / 2.0 - height / 2.0;
        elem.insert(QStringLiteral("containerId"), QString());
        elem.insert(QStringLiteral("customData"), QJsonObject{
            {QStringLiteral("scribbleway"), QJsonObject{{QStringLiteral("attachedTextFor"), shape.value(QStringLiteral("id")).toString()}}}
        });
    }

    elem.insert(QStringLiteral("x"), x);
    elem.insert(QStringLiteral("y"), y);
    elem.insert(QStringLiteral("width"), width);
    elem.insert(QStringLiteral("height"), height);
    return elem;
}

QVariantMap shapeFromElement(const QJsonObject &elem)
{
    QVariantMap shape;
    QString excalType = elem.value(QStringLiteral("type")).toString();
    
    QString type = (excalType == QStringLiteral("freedraw")) ? QStringLiteral("freehand") : excalType;
    if (type != QStringLiteral("rectangle") && type != QStringLiteral("ellipse") &&
        type != QStringLiteral("text") && type != QStringLiteral("line") &&
        type != QStringLiteral("arrow") && type != QStringLiteral("freehand")) {
        return shape;
    }
    shape.insert(QStringLiteral("type"), type);

    QString strokeColor = elem.value(QStringLiteral("strokeColor")).toString(QStringLiteral("#000000"));
    if (QColor(strokeColor) == Qt::white) {
        strokeColor = QStringLiteral("#e63946");
    }
    shape.insert(QStringLiteral("color"), strokeColor);
    shape.insert(QStringLiteral("strokeWidth"), elem.value(QStringLiteral("strokeWidth")).toDouble(1.0));
    
    double opacityInt = elem.value(QStringLiteral("opacity")).toDouble(100.0);
    shape.insert(QStringLiteral("opacity"), opacityInt / 100.0);
    
    shape.insert(QStringLiteral("selected"), true);
    shape.insert(QStringLiteral("locked"), false);
    shape.insert(QStringLiteral("roughness"), elem.value(QStringLiteral("roughness")).toInt(1));
    shape.insert(QStringLiteral("glow"), elem.value(QStringLiteral("glow")).toInt(0)); // Default to 0px
    shape.insert(QStringLiteral("seed"), elem.value(QStringLiteral("seed")).toInt(123456));
    {
        const QString bg = elem.value(QStringLiteral("backgroundColor")).toString(QStringLiteral("transparent"));
        if (bg.isEmpty() || bg.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0) {
            shape.insert(QStringLiteral("fillColor"), QStringLiteral("transparent"));
            shape.insert(QStringLiteral("fillOpacity"), 0.0);
        } else {
            QString hex = bg;
            double fillOp = 1.0;
            // Excalidraw RGBA: #RRGGBBAA (9 chars with '#')
            if (hex.startsWith(QLatin1Char('#')) && hex.size() == 9) {
                bool ok = false;
                const int alphaByte = hex.mid(7, 2).toInt(&ok, 16);
                if (ok) {
                    fillOp = alphaByte / 255.0;
                }
                hex = hex.left(7); // #RRGGBB
            } else if (hex.startsWith(QLatin1Char('#')) && hex.size() == 7) {
                fillOp = 1.0;
            }
            shape.insert(QStringLiteral("fillColor"), hex);
            shape.insert(QStringLiteral("fillOpacity"), fillOp);
        }
    }

    QString id = elem.value(QStringLiteral("id")).toString();
    if (!id.isEmpty()) {
        shape.insert(QStringLiteral("id"), id);
    }
    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        shape.insert(QStringLiteral("x"), elem.value(QStringLiteral("x")).toDouble());
        shape.insert(QStringLiteral("y"), elem.value(QStringLiteral("y")).toDouble());
        shape.insert(QStringLiteral("width"), elem.value(QStringLiteral("width")).toDouble());
        shape.insert(QStringLiteral("height"), elem.value(QStringLiteral("height")).toDouble());
        
        if (type == QStringLiteral("rectangle")) {
            QJsonValue rv = elem.value(QStringLiteral("roundness"));
            int radius = rv.isObject() ? static_cast<int>(rv.toObject().value(QStringLiteral("value")).toDouble(8.0)) : (rv.isNull() ? 0 : 8);
            shape.insert(QStringLiteral("borderRadius"), radius);
        } else if (type == QStringLiteral("text")) {
            shape.insert(QStringLiteral("text"), elem.value(QStringLiteral("text")).toString());
            shape.insert(QStringLiteral("fontSize"), static_cast<int>(elem.value(QStringLiteral("fontSize")).toDouble(20.0)));
            
            int excalFont = static_cast<int>(elem.value(QStringLiteral("fontFamily")).toDouble(2.0));
            QString family;
            if (excalFont == 3) {
                family = QStringLiteral("monospace");
            } else if (excalFont == 1) {
                family = QStringLiteral("Virgil");
            } else {
                family = QStringLiteral("sans-serif");
            }
            shape.insert(QStringLiteral("fontFamily"), family);

            const QString containerId = elem.value(QStringLiteral("containerId")).toString();
            if (!containerId.isEmpty()) {
                shape.insert(QStringLiteral("containerId"), containerId);
            }
            const QJsonValue customData = elem.value(QStringLiteral("customData"));
            if (customData.isObject()) {
                shape.insert(QStringLiteral("customData"), customData.toObject().toVariantMap());
            }
        }
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            QJsonValue beVal = elem.value(QStringLiteral("boundElements"));
            if (beVal.isArray()) {
                QVariantList boundIds;
                for (const QJsonValue &v : beVal.toArray()) {
                    QJsonObject be = v.toObject();
                    QString bid = be.value(QStringLiteral("id")).toString();
                    if (!bid.isEmpty()) {
                        boundIds.append(bid);
                    }
                }
                if (!boundIds.isEmpty()) {
                    shape.insert(QStringLiteral("boundElementIds"), boundIds);
                }
            }
        }
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        double x = elem.value(QStringLiteral("x")).toDouble();
        double y = elem.value(QStringLiteral("y")).toDouble();
        QJsonArray pts = elem.value(QStringLiteral("points")).toArray();
        
        double fromX = x;
        double fromY = y;
        double toX = x;
        double toY = y;
        
        if (pts.size() >= 2) {
            QJsonArray p1 = pts.first().toArray();
            QJsonArray p2 = pts.last().toArray();
            if (p1.size() >= 2 && p2.size() >= 2) {
                fromX = x + p1.at(0).toDouble();
                fromY = y + p1.at(1).toDouble();
                toX = x + p2.at(0).toDouble();
                toY = y + p2.at(1).toDouble();
            }
        }
        
        shape.insert(QStringLiteral("fromX"), fromX);
        shape.insert(QStringLiteral("fromY"), fromY);
        shape.insert(QStringLiteral("toX"), toX);
        shape.insert(QStringLiteral("toY"), toY);

        for (const QString &key : {QStringLiteral("startBinding"), QStringLiteral("endBinding")}) {
            const QJsonObject binding = elem.value(key).toObject();
            if (!binding.isEmpty()) {
                shape.insert(key, QVariantMap{
                    {QStringLiteral("elementId"), binding.value(QStringLiteral("elementId")).toString()},
                    {QStringLiteral("focus"), binding.value(QStringLiteral("focus")).toDouble()},
                    {QStringLiteral("gap"), binding.value(QStringLiteral("gap")).toDouble(0.0)},
                });
            }
        }
    } else if (type == QStringLiteral("freehand")) {
        double x = elem.value(QStringLiteral("x")).toDouble();
        double y = elem.value(QStringLiteral("y")).toDouble();
        QJsonArray pts = elem.value(QStringLiteral("points")).toArray();
        
        QVariantList pointsList;
        for (const QJsonValue &v : pts) {
            QJsonArray pt = v.toArray();
            if (pt.size() >= 2) {
                QPointF p(x + pt.at(0).toDouble(), y + pt.at(1).toDouble());
                pointsList.append(QVariant::fromValue(p));
            }
        }
        shape.insert(QStringLiteral("points"), pointsList);
    }

    return shape;
}
void attachBoundText(QList<QVariantMap> &shapes, const QString &defaultFontFamily, int defaultFontSize)
{
    QHash<QString, int> containerIndexById;
    for (int i = 0; i < shapes.size(); ++i) {
        const QString type = shapes[i].value(QStringLiteral("type")).toString();
        const QString id = shapes[i].value(QStringLiteral("id")).toString();
        if (ShapesModel::supportsAttachedText(type) && !id.isEmpty()) {
            containerIndexById.insert(id, i);
        }
    }

    QList<int> textIndicesToRemove;
    for (int i = 0; i < shapes.size(); ++i) {
        QVariantMap textShape = shapes[i];
        if (textShape.value(QStringLiteral("type")).toString() != QStringLiteral("text")) {
            continue;
        }

        QString containerId = textShape.value(QStringLiteral("containerId")).toString();
        if (containerId.isEmpty()) {
            containerId = textShape.value(QStringLiteral("customData")).toMap()
                .value(QStringLiteral("scribbleway")).toMap()
                .value(QStringLiteral("attachedTextFor")).toString();
        }
        if (!containerIndexById.contains(containerId)) {
            continue;
        }

        const int containerIndex = containerIndexById.value(containerId);
        QVariantMap container = shapes[containerIndex];
        QVariantMap binding;
        binding.insert(QStringLiteral("type"), QStringLiteral("attachedText"));
        binding.insert(QStringLiteral("id"), textShape.value(QStringLiteral("id")).toString());
        binding.insert(QStringLiteral("text"), textShape.value(QStringLiteral("text")).toString());
        binding.insert(QStringLiteral("fontFamily"), textShape.value(QStringLiteral("fontFamily"), container.value(QStringLiteral("fontFamily"), defaultFontFamily)).toString());
        binding.insert(QStringLiteral("fontSize"), textShape.value(QStringLiteral("fontSize"), container.value(QStringLiteral("fontSize"), defaultFontSize)).toInt());
        binding.insert(QStringLiteral("textAlign"), QStringLiteral("center"));
        binding.insert(QStringLiteral("verticalAlign"), QStringLiteral("middle"));

        QVariantList bindings = ShapesModel::withoutAttachedTextBinding(container);
        bindings.append(binding);
        container.insert(QStringLiteral("bindings"), bindings);
        container.insert(QStringLiteral("fontFamily"), binding.value(QStringLiteral("fontFamily")));
        container.insert(QStringLiteral("fontSize"), binding.value(QStringLiteral("fontSize")));
        shapes[containerIndex] = container;
        textIndicesToRemove.prepend(i);
    }

    for (int index : textIndicesToRemove) {
        shapes.removeAt(index);
    }
}

} // namespace Excalidraw
