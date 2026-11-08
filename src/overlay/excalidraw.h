#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariantMap>

// Translation between Scribbleway shapes and Excalidraw's clipboard elements.
namespace Excalidraw {
QJsonObject elementFromShape(const QVariantMap &shape);
QJsonObject attachedTextElement(const QVariantMap &shape, const QVariantMap &attachedText, int defaultFontSize);
QVariantMap shapeFromElement(const QJsonObject &elem);
// Folds text elements that belong to a container in `shapes` into that container.
void attachBoundText(QList<QVariantMap> &shapes, const QString &defaultFontFamily, int defaultFontSize);
}
