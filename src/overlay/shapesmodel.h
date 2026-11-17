#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QList>
#include <QRectF>

class Selection;

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
        BoundElementIdsRole,
        BindingsRole,
        AttachedTextRole,
        BoundsRole,
        AttachedTextRectRole
    };
    Q_ENUM(ShapeRoles)

    // QAbstractItemModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Helper methods for data modification
    void setShapes(const QList<QVariantMap> &shapes);
    const QList<QVariantMap> &shapes() const;
    QString shapeIdAt(int index) const;
    static QVariantMap firstAttachedTextBinding(const QVariantMap &shape);
    static QVariantList withoutAttachedTextBinding(const QVariantMap &shape);
    // The selected role reads from `selection`, which must outlive the model.
    void setSelection(const Selection *selection);

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
    void recordHistory();

    QList<QVariantMap> m_shapes;
    const Selection *m_selection = nullptr;
    QList<QList<QVariantMap>> m_history;
    QList<QList<QVariantMap>> m_redo;
    bool m_isApplyingUndo = false;
    int m_inEditTransaction = 0;
    bool m_editSnapshotTaken = false;
};
