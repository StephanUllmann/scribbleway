#pragma once

#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QString>

namespace DBusUtils {


inline QVariant demarshal(const QVariant &var) {
    if (var.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = var.value<QDBusArgument>();
        if (arg.currentType() == QDBusArgument::ArrayType) {
            QVariantList list;
            arg.beginArray();
            while (!arg.atEnd()) {
                if (arg.currentType() == QDBusArgument::MapType) {
                    QVariantMap elem;
                    arg >> elem;
                    list.append(demarshal(elem));
                } else if (arg.currentType() == QDBusArgument::ArrayType) {
                    QVariantList elem;
                    arg >> elem;
                    list.append(demarshal(elem));
                } else if (arg.currentType() == QDBusArgument::VariantType) {
                    QDBusVariant elem;
                    arg >> elem;
                    list.append(demarshal(elem.variant()));
                } else {
                    QVariant elem;
                    arg >> elem;
                    list.append(elem);
                }
            }
            arg.endArray();
            return list;
        } else if (arg.currentType() == QDBusArgument::MapType) {
            QVariantMap map;
            arg.beginMap();
            while (!arg.atEnd()) {
                QString key;
                arg.beginMapEntry();
                arg >> key;
                QVariant val;
                if (arg.currentType() == QDBusArgument::MapType) {
                    QVariantMap elem;
                    arg >> elem;
                    val = demarshal(elem);
                } else if (arg.currentType() == QDBusArgument::ArrayType) {
                    QVariantList elem;
                    arg >> elem;
                    val = demarshal(elem);
                } else if (arg.currentType() == QDBusArgument::VariantType) {
                    QDBusVariant elem;
                    arg >> elem;
                    val = demarshal(elem.variant());
                } else {
                    arg >> val;
                }
                arg.endMapEntry();
                map.insert(key, val);
            }
            arg.endMap();
            return map;
        } else if (arg.currentType() == QDBusArgument::VariantType) {
            QDBusVariant val;
            arg >> val;
            return demarshal(val.variant());
        }
    } else if (var.userType() == QMetaType::QVariantMap) {
        QVariantMap map = var.toMap();
        QVariantMap result;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            result.insert(it.key(), demarshal(it.value()));
        }
        return result;
    } else if (var.userType() == QMetaType::QVariantList) {
        QVariantList list = var.toList();
        for (int i = 0; i < list.size(); ++i) {
            list[i] = demarshal(list[i]);
        }
        return list;
    }
    return var;
}

} // namespace DBusUtils
