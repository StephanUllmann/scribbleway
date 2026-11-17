#include "shapesmodel.h"
#include "shapegeometry.h"
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
    {nullptr, "bounds"},
    {nullptr, "attachedTextRect"},
};
constexpr int kRoleCount = int(sizeof(kRoles) / sizeof(kRoles[0]));
static_assert(kRoleCount == ShapesModel::AttachedTextRectRole - ShapesModel::TypeRole + 1,
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

QVariantList ShapesModel::withoutAttachedTextBinding(const QVariantMap &shape)
{
    QVariantList filtered;
    const QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    for (const QVariant &bindingValue : bindings) {
        const QVariantMap binding = bindingValue.toMap();
        if (binding.value(QStringLiteral("type")).toString() != QStringLiteral("attachedText")) {
            filtered.append(binding);
        }
    }
    return filtered;
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
    if (const char *key = kRoles[slot].key) {
        return shape.value(QLatin1String(key));
    }
    switch (role) {
    case AttachedTextRole:
        return firstAttachedTextBinding(shape);
    case BoundsRole:
        return ShapeGeometry::bounds(shape);
    default:
        if (!ShapeGeometry::supportsAttachedText(shape.value(QStringLiteral("type")).toString())) {
            return QVariant();
        }
        // Every attached text stores its own fontSize, so the fallback only covers hand-made maps.
        return ShapeGeometry::attachedTextRect(shape, 20);
    }
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

// Undo and redo each keep at most this many states.
static void pushCapped(QList<QList<QVariantMap>> &stack, const QList<QVariantMap> &state)
{
    stack.append(state);
    if (stack.size() > 50) {
        stack.removeFirst();
    }
}

void ShapesModel::saveHistorySnapshot()
{
    pushCapped(m_history, m_shapes);
    // New user mutation invalidates the redo branch.
    m_redo.clear();
}

// Every mutator calls this before a real change. An edit group stores one snapshot, at
// its first real change, so a press or a selection change alone costs no undo step.
void ShapesModel::recordHistory()
{
    if (m_isApplyingUndo) {
        return;
    }
    if (m_inEditTransaction > 0) {
        if (m_editSnapshotTaken) {
            return;
        }
        m_editSnapshotTaken = true;
    }
    saveHistorySnapshot();
}

void ShapesModel::undo()
{
    if (m_history.isEmpty()) {
        return;
    }
    // The model reset destroys the delegate whose release would have closed an open group.
    m_inEditTransaction = 0;
    m_isApplyingUndo = true;
    // Preserve the state we are leaving so redo can restore it.
    pushCapped(m_redo, m_shapes);
    QList<QVariantMap> previousState = m_history.takeLast();
    setShapes(previousState);
    m_isApplyingUndo = false;
}

void ShapesModel::redo()
{
    if (m_redo.isEmpty()) {
        return;
    }
    m_inEditTransaction = 0;
    m_isApplyingUndo = true;
    // Current state becomes undoable again.
    pushCapped(m_history, m_shapes);
    QList<QVariantMap> nextState = m_redo.takeLast();
    setShapes(nextState);
    m_isApplyingUndo = false;
}

void ShapesModel::beginEdit()
{
    if (!m_isApplyingUndo) {
        if (m_inEditTransaction == 0) {
            m_editSnapshotTaken = false;
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
    recordHistory();
    beginResetModel();
    m_shapes = shapes;
    endResetModel();
}

const QList<QVariantMap> &ShapesModel::shapes() const
{
    return m_shapes;
}

void ShapesModel::addShape(const QVariantMap &shape)
{
    recordHistory();
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
            // A text shape's size just follows its rendered text, so it is not an edit either.
            const bool derived = it.key() == QStringLiteral("selected")
                || (currentShape.value(QStringLiteral("type")).toString() == QStringLiteral("text")
                    && (it.key() == QStringLiteral("width") || it.key() == QStringLiteral("height")));
            if (!derived && currentShape.value(it.key()) != it.value()) {
                hasRealChanges = true;
                break;
            }
        }

        if (hasRealChanges) {
            recordHistory();
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
                // The computed roles change with the keys they are derived from.
                if (role == BindingsRole) {
                    changedRoles << AttachedTextRole << AttachedTextRectRole;
                } else if (role == FontSizeRole) {
                    changedRoles << AttachedTextRectRole;
                } else if (role == PointsRole || (role >= XRole && role <= ToYRole)) {
                    changedRoles << BoundsRole << AttachedTextRectRole;
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
        recordHistory();
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
    recordHistory();
    int destination = (fromIndex < toIndex) ? (toIndex + 1) : toIndex;
    beginMoveRows(QModelIndex(), fromIndex, fromIndex, QModelIndex(), destination);
    m_shapes.move(fromIndex, toIndex);
    endMoveRows();
    return true;
}

void ShapesModel::clear()
{
    if (!m_shapes.isEmpty()) {
        recordHistory();
        m_inEditTransaction = 0;
        beginResetModel();
        m_shapes.clear();
        endResetModel();
    }
}
