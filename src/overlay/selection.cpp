#include "selection.h"

void Selection::set(QSet<QString> ids, const QString &primary)
{
    if (!primary.isEmpty()) ids.insert(primary);
    Q_ASSERT(primary.isEmpty() == ids.isEmpty());
    if (ids == m_ids && primary == m_primary) return;
    m_ids = std::move(ids);
    m_primary = primary;
    Q_EMIT changed();
}
