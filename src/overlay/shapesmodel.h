#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QList>

class ShapesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ShapesModel(QObject *parent = nullptr);

    enum ShapeRoles {
        TypeRole = Qt::UserRole + 1,
        ColorRole,
        StrokeWidthRole,
        OpacityRole,
        SelectedRole,
        LockedRole,
        PointsRole,
        XRole,
        YRole,
        WidthRole,
        HeightRole,
        FromXRole,
        FromYRole,
        ToXRole,
        ToYRole,
        TextRole,
        FontFamilyRole,
        FontSizeRole,
        BorderRadiusRole,
        RoughnessRole,
        SeedRole,
        GlowRole,
        FillColorRole,
        FillOpacityRole,
        IdRole,
        StartBindingRole,
        EndBindingRole,
        BoundElementIdsRole
    };
    Q_ENUM(ShapeRoles)

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Helper methods for data modification
    void setShapes(const QList<QVariantMap> &shapes);
    QList<QVariantMap> shapes() const;
    QString shapeIdAt(int index) const;

    void addShape(const QVariantMap &shape);
    void updateShape(int index, const QVariantMap &properties);
    void removeShape(int index);
    bool moveShape(int fromIndex, int toIndex);
    void clear();
    void undo();
    void redo();
    void beginEdit();
    void endEdit();

private:
    void saveHistorySnapshot();

    QList<QVariantMap> m_shapes;
    QList<QList<QVariantMap>> m_history;
    QList<QList<QVariantMap>> m_redo;
    bool m_isApplyingUndo = false;
    int m_inEditTransaction = 0;
};
