#include "shapesmodel.h"

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
    return roles;
}

void ShapesModel::saveHistorySnapshot()
{
    m_history.append(m_shapes);
    if (m_history.size() > 50) {
        m_history.removeFirst();
    }
}

void ShapesModel::undo()
{
    if (m_history.isEmpty()) {
        return;
    }
    m_isApplyingUndo = true;
    QList<QVariantMap> previousState = m_history.takeLast();
    setShapes(previousState);
    m_isApplyingUndo = false;
}

void ShapesModel::beginEdit()
{
    if (!m_isApplyingUndo && !m_inEditTransaction) {
        saveHistorySnapshot();
        m_inEditTransaction = true;
    }
}

void ShapesModel::endEdit()
{
    m_inEditTransaction = false;
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
    beginInsertRows(QModelIndex(), m_shapes.size(), m_shapes.size());
    m_shapes.append(shape);
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

        if (hasRealChanges && !m_isApplyingUndo && !m_inEditTransaction) {
            saveHistorySnapshot();
        }

        auto &shape = m_shapes[index];
        QList<int> changedRoles;
        for (auto it = properties.begin(); it != properties.end(); ++it) {
            if (shape.value(it.key()) != it.value()) {
                shape[it.key()] = it.value();
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
