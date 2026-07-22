#include "shapesmodel.h"
#include <QPointF>
#include <QUuid>

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
    if (!index.isValid() || index.row() < 0 || index.row() >= m_shapes.size())
        return QVariant();

    const auto &shape = m_shapes[index.row()];
    switch (role) {
        case TypeRole: return shape.value(QStringLiteral("type"));
        case ColorRole: return shape.value(QStringLiteral("color"));
        case StrokeWidthRole: return shape.value(QStringLiteral("strokeWidth"));
        case OpacityRole: return shape.value(QStringLiteral("opacity"));
        case SelectedRole: return shape.value(QStringLiteral("selected"));
        case LockedRole: return shape.value(QStringLiteral("locked"));
        case PointsRole: return shape.value(QStringLiteral("points"));
        case XRole: return shape.value(QStringLiteral("x"));
        case YRole: return shape.value(QStringLiteral("y"));
        case WidthRole: return shape.value(QStringLiteral("width"));
        case HeightRole: return shape.value(QStringLiteral("height"));
        case FromXRole: return shape.value(QStringLiteral("fromX"));
        case FromYRole: return shape.value(QStringLiteral("fromY"));
        case ToXRole: return shape.value(QStringLiteral("toX"));
        case ToYRole: return shape.value(QStringLiteral("toY"));
        case TextRole: return shape.value(QStringLiteral("text"));
        case FontFamilyRole: return shape.value(QStringLiteral("fontFamily"));
        case FontSizeRole: return shape.value(QStringLiteral("fontSize"));
        case BorderRadiusRole: return shape.value(QStringLiteral("borderRadius"));
        case RoughnessRole: return shape.value(QStringLiteral("roughness"));
        case SeedRole: return shape.value(QStringLiteral("seed"));
        case GlowRole: return shape.value(QStringLiteral("glow"));
        case IdRole: return shape.value(QStringLiteral("id"));
        case StartBindingRole: return shape.value(QStringLiteral("startBinding"));
        case EndBindingRole: return shape.value(QStringLiteral("endBinding"));
        case BoundElementIdsRole: return shape.value(QStringLiteral("boundElementIds"));
        default: return QVariant();
    }
}


QHash<int, QByteArray> ShapesModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TypeRole] = "type";
    roles[ColorRole] = "color";
    roles[StrokeWidthRole] = "strokeWidth";
    roles[OpacityRole] = "opacity";
    roles[SelectedRole] = "selected";
    roles[LockedRole] = "locked";
    roles[PointsRole] = "points";
    roles[XRole] = "x";
    roles[YRole] = "y";
    roles[WidthRole] = "width";
    roles[HeightRole] = "height";
    roles[FromXRole] = "fromX";
    roles[FromYRole] = "fromY";
    roles[ToXRole] = "toX";
    roles[ToYRole] = "toY";
    roles[TextRole] = "text";
    roles[FontFamilyRole] = "fontFamily";
    roles[FontSizeRole] = "fontSize";
    roles[BorderRadiusRole] = "borderRadius";
    roles[RoughnessRole] = "roughness";
    roles[SeedRole] = "seed";
    roles[GlowRole] = "glow";
    roles[IdRole] = "shapeId";
    roles[StartBindingRole] = "startBinding";
    roles[EndBindingRole] = "endBinding";
    roles[BoundElementIdsRole] = "boundElementIds";
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
                // Map key back to role for fine-grained change signal
                if (it.key() == QStringLiteral("type")) changedRoles << TypeRole;
                else if (it.key() == QStringLiteral("color")) changedRoles << ColorRole;
                else if (it.key() == QStringLiteral("strokeWidth")) changedRoles << StrokeWidthRole;
                else if (it.key() == QStringLiteral("opacity")) changedRoles << OpacityRole;
                else if (it.key() == QStringLiteral("selected")) changedRoles << SelectedRole;
                else if (it.key() == QStringLiteral("locked")) changedRoles << LockedRole;
                else if (it.key() == QStringLiteral("points")) changedRoles << PointsRole;
                else if (it.key() == QStringLiteral("x")) changedRoles << XRole;
                else if (it.key() == QStringLiteral("y")) changedRoles << YRole;
                else if (it.key() == QStringLiteral("width")) changedRoles << WidthRole;
                else if (it.key() == QStringLiteral("height")) changedRoles << HeightRole;
                else if (it.key() == QStringLiteral("fromX")) changedRoles << FromXRole;
                else if (it.key() == QStringLiteral("fromY")) changedRoles << FromYRole;
                else if (it.key() == QStringLiteral("toX")) changedRoles << ToXRole;
                else if (it.key() == QStringLiteral("toY")) changedRoles << ToYRole;
                else if (it.key() == QStringLiteral("text")) changedRoles << TextRole;
                else if (it.key() == QStringLiteral("fontFamily")) changedRoles << FontFamilyRole;
                else if (it.key() == QStringLiteral("fontSize")) changedRoles << FontSizeRole;
                else if (it.key() == QStringLiteral("borderRadius")) changedRoles << BorderRadiusRole;
                else if (it.key() == QStringLiteral("roughness")) changedRoles << RoughnessRole;
                else if (it.key() == QStringLiteral("seed")) changedRoles << SeedRole;
                else if (it.key() == QStringLiteral("glow")) changedRoles << GlowRole;
                else if (it.key() == QStringLiteral("id")) changedRoles << IdRole;
                else if (it.key() == QStringLiteral("startBinding")) changedRoles << StartBindingRole;
                else if (it.key() == QStringLiteral("endBinding")) changedRoles << EndBindingRole;
                else if (it.key() == QStringLiteral("boundElementIds")) changedRoles << BoundElementIdsRole;
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
