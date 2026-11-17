#pragma once

#include <QObject>
#include <QSet>
#include <QString>

// Which Shapes are selected, by id. Selection is view state, so it lives here and not
// in the Shape maps: it never reaches the undo history or the clipboard, and ids,
// unlike rows, survive a reorder or a delete of another shape.
class Selection : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    bool contains(const QString &id) const { return m_ids.contains(id); }
    int count() const { return int(m_ids.size()); }
    const QSet<QString> &ids() const { return m_ids; }
    // The one shape the Tray Popup shows and raise/lower move. Empty only when nothing is selected.
    QString primary() const { return m_primary; }

    // Replaces the selection. A primary that is not in `ids` joins it.
    void set(QSet<QString> ids, const QString &primary);
    void clear() { set({}, {}); }

Q_SIGNALS:
    void changed();

private:
    QSet<QString> m_ids;
    QString m_primary;
};
