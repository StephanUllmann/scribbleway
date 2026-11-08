#include "shapesmodel.h"
#include <QPointF>
#include <QUuid>

// One table behind data(), roleNames() and updateShape()'s key -> role lookup. Row order
// must match the ShapeRoles enum: row 0 is TypeRole. A null `key` means the role is
// computed rather than read straight out of the shape map.
namespace {
struct RoleEntry { const char *key; const char *qmlName; };

constexpr RoleEntry kRoles[] = {
    {"type", "type"},
    {"color", "color"},
    {"strokeWidth", "strokeWidth"},
    {"opacity", "opacity"},
    {"selected", "selected"},
    {"locked", "locked"},
    {"points", "points"},
    {"x", "x"},
    {"y", "y"},
    {"width", "width"},
    {"height", "height"},
    {"fromX", "fromX"},
    {"fromY", "fromY"},
    {"toX", "toX"},
    {"toY", "toY"},
    {"text", "text"},
    {"fontFamily", "fontFamily"},
    {"fontSize", "fontSize"},
    {"borderRadius", "borderRadius"},
    {"roughness", "roughness"},
    {"seed", "seed"},
    {"glow", "glow"},
    {"fillColor", "fillColor"},
    {"fillOpacity", "fillOpacity"},
    {"id", "shapeId"},
    {"startBinding", "startBinding"},
    {"endBinding", "endBinding"},
    {"boundElementIds", "boundElementIds"},
    {"bindings", "bindings"},
    {nullptr, "attachedText"},
};
constexpr int kRoleCount = int(sizeof(kRoles) / sizeof(kRoles[0]));
static_assert(kRoleCount == ShapesModel::AttachedTextRole - ShapesModel::TypeRole + 1,
              "kRoles must have one row per ShapeRoles value, in enum order");

int roleForKey(const QString &key)
{
    static const QHash<QString, int> byKey = [] {
        QHash<QString, int> h;
        for (int i = 0; i < kRoleCount; ++i) {
            if (kRoles[i].key) {
                h.insert(QLatin1String(kRoles[i].key), ShapesModel::TypeRole + i);
            }
        }
        return h;
    }();
    return byKey.value(key, -1);
}
} // namespace

static QVariantList normalizePoints(const QVariant &var)
{
    QVariantList result;
    const QVariantList list = var.toList();
    result.reserve(list.size());
    for (const QVariant &v : list) {
        if (v.canConvert<QPointF>()) {
            result.append(QVariant::fromValue(v.toPointF()));
        } else if (v.typeId() == QMetaType::QVariantMap) {
            const QVariantMap map = v.toMap();
            result.append(QVariant::fromValue(QPointF(
                map.value(QStringLiteral("x")).toDouble(),
                map.value(QStringLiteral("y")).toDouble()
            )));
        } else {
            result.append(v);
        }
    }
    return result;
}

QVariantMap ShapesModel::firstAttachedTextBinding(const QVariantMap &shape)
{
    const QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    for (const QVariant &bindingValue : bindings) {
        const QVariantMap binding = bindingValue.toMap();
        if (binding.value(QStringLiteral("type")).toString() == QStringLiteral("attachedText")) {
            return binding;
        }
    }
    return QVariantMap();
}

ShapesModel::ShapesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ShapesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_shapes.size();
}

QVariant ShapesModel::data(const QModelIndex &index, int role) const
{
    const int slot = role - TypeRole;
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shapes.size()
        || slot < 0 || slot >= kRoleCount) {
        return QVariant();
    }

    const auto &shape = m_shapes[index.row()];
    const char *key = kRoles[slot].key;
    return key ? shape.value(QLatin1String(key)) : firstAttachedTextBinding(shape);
}


QHash<int, QByteArray> ShapesModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    for (int i = 0; i < kRoleCount; ++i) {
        roles[TypeRole + i] = kRoles[i].qmlName;
    }
    return roles;
}

QString ShapesModel::shapeIdAt(int index) const
{
    if (index >= 0 && index < m_shapes.size()) {
        return m_shapes[index].value(QStringLiteral("id")).toString();
    }
    return QString();
}

void ShapesModel::saveHistorySnapshot()
{
    m_history.append(m_shapes);
    if (m_history.size() > 50) {
        m_history.removeFirst();
    }
    // New user mutation invalidates the redo branch.
    m_redo.clear();
}

void ShapesModel::undo()
{
    if (m_history.isEmpty()) {
        return;
    }
    m_isApplyingUndo = true;
    // Preserve the state we are leaving so redo can restore it.
    m_redo.append(m_shapes);
    if (m_redo.size() > 50) {
        m_redo.removeFirst();
    }
    QList<QVariantMap> previousState = m_history.takeLast();
    setShapes(previousState);
    m_isApplyingUndo = false;
}

void ShapesModel::redo()
{
    if (m_redo.isEmpty()) {
        return;
    }
    m_isApplyingUndo = true;
    // Current state becomes undoable again.
    m_history.append(m_shapes);
    if (m_history.size() > 50) {
        m_history.removeFirst();
    }
    QList<QVariantMap> nextState = m_redo.takeLast();
    setShapes(nextState);
    m_isApplyingUndo = false;
}

void ShapesModel::beginEdit()
{
    if (!m_isApplyingUndo) {
        if (m_inEditTransaction == 0) {
            saveHistorySnapshot();
        }
        m_inEditTransaction++;
    }
}

void ShapesModel::endEdit()
{
    if (!m_isApplyingUndo && m_inEditTransaction > 0) {
        m_inEditTransaction--;
    }
}

void ShapesModel::setShapes(const QList<QVariantMap> &shapes)
{
    if (!m_isApplyingUndo) {
        saveHistorySnapshot();
    }
    beginResetModel();
    m_shapes = shapes;
    endResetModel();
}

QList<QVariantMap> ShapesModel::shapes() const
{
    return m_shapes;
}

void ShapesModel::addShape(const QVariantMap &shape)
{
    if (!m_isApplyingUndo) {
        saveHistorySnapshot();
    }
    QVariantMap normalizedShape = shape;
    if (!normalizedShape.contains(QStringLiteral("id")) || normalizedShape.value(QStringLiteral("id")).toString().isEmpty()) {
        normalizedShape.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    if (normalizedShape.contains(QStringLiteral("points"))) {
        normalizedShape.insert(QStringLiteral("points"), normalizePoints(normalizedShape.value(QStringLiteral("points"))));
    }
    beginInsertRows(QModelIndex(), m_shapes.size(), m_shapes.size());
    m_shapes.append(normalizedShape);
    endInsertRows();
}

void ShapesModel::updateShape(int index, const QVariantMap &properties)
{
    if (index >= 0 && index < m_shapes.size()) {
        // Check if anything other than 'selected' is changing, and if it's actually changing
        bool hasRealChanges = false;
        const auto &currentShape = m_shapes[index];
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            if (it.key() != QStringLiteral("selected") && currentShape.value(it.key()) != it.value()) {
                hasRealChanges = true;
                break;
            }
        }

        if (hasRealChanges && !m_isApplyingUndo && m_inEditTransaction == 0) {
            saveHistorySnapshot();
        }

        auto &shape = m_shapes[index];
        QList<int> changedRoles;
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            QVariant val = it.value();
            if (it.key() == QStringLiteral("points")) {
                val = normalizePoints(val);
            }
            if (shape.value(it.key()) != val) {
                shape[it.key()] = val;
                const int role = roleForKey(it.key());
                if (role >= 0) {
                    changedRoles << role;
                }
                // attachedText is derived from bindings, so it changes with it.
                if (role == BindingsRole) {
                    changedRoles << AttachedTextRole;
                }
            }
        }
        if (!changedRoles.isEmpty()) {
            QModelIndex idx = createIndex(index, 0);
            Q_EMIT dataChanged(idx, idx, changedRoles);
        }
    }
}


void ShapesModel::removeShape(int index)
{
    if (index >= 0 && index < m_shapes.size()) {
        if (!m_isApplyingUndo) {
            saveHistorySnapshot();
        }
        beginRemoveRows(QModelIndex(), index, index);
        m_shapes.removeAt(index);
        endRemoveRows();
    }
}

bool ShapesModel::moveShape(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_shapes.size() || toIndex < 0 || toIndex >= m_shapes.size()) {
        return false;
    }
    if (fromIndex == toIndex) {
        return true;
    }
    if (!m_isApplyingUndo) {
        saveHistorySnapshot();
    }
    int destination = (fromIndex < toIndex) ? (toIndex + 1) : toIndex;
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destination);
    m_shapes.move(fromIndex, toIndex);
    endMoveRows();
    return true;
}

void ShapesModel::clear()
{
    if (!m_shapes.isEmpty()) {
        if (!m_isApplyingUndo) {
            saveHistorySnapshot();
        }
        beginResetModel();
        m_shapes.clear();
        endResetModel();
    }
}
