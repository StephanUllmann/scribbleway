#include "overlaycontroller.h"
#include <QDebug>
#include <QDBusArgument>
#include <QDBusVariant>
#include <LayerShellQt/Window>
#include <QScreen>
#include <QGuiApplication>
#include <KGlobalAccel>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCursor>
#include <QPointF>
#include <limits>

#include "../common/dbusutils.h"
#include <QFontDatabase>

OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
    , m_activeTool(QStringLiteral("freehand"))
    , m_defaultColor(QStringLiteral("#e63946"))
{
    const QStringList families = QFontDatabase::families();
    if (families.contains(QStringLiteral("Cascadia Code"), Qt::CaseInsensitive)) {
        m_defaultFontFamily = QStringLiteral("Cascadia Code");
    } else {
        m_defaultFontFamily = QStringLiteral("monospace");
    }
    m_shapesModel.setParent(this);
}

QQuickWindow* OverlayController::window() const
{
    return m_window;
}

void OverlayController::setWindow(QQuickWindow *window)
{
    if (m_window != window) {
        m_window = window;
        Q_EMIT windowChanged();
    }
}


QVariantList OverlayController::shapesMetadata() const
{
    QVariantList list;
    const auto shapesList = m_shapesModel.shapes();
    int index = 0;
    for (const auto &shape : shapesList) {
        QVariantMap metadata;
        QString type = shape.value(QStringLiteral("type")).toString();
        metadata.insert(QStringLiteral("type"), type);
        metadata.insert(QStringLiteral("locked"), shape.value(QStringLiteral("locked"), false).toBool());
        metadata.insert(QStringLiteral("selected"), shape.value(QStringLiteral("selected"), false).toBool());
        metadata.insert(QStringLiteral("color"), shape.value(QStringLiteral("color")).toString());

        // Generate the display name server-side
        QString typeCap = type;
        if (!typeCap.isEmpty()) {
            typeCap[0] = typeCap[0].toUpper();
        }
        QString displayName = QStringLiteral("%1 %2").arg(typeCap).arg(index + 1);
        metadata.insert(QStringLiteral("name"), displayName);

        list.append(metadata);
        index++;
    }
    return list;
}

ShapesModel* OverlayController::shapesModel()
{
    return &m_shapesModel;
}

int OverlayController::selectedIndex() const
{
    return m_selectedIndex;
}

QString OverlayController::activeTool() const
{
    return m_activeTool;
}

void OverlayController::setActiveTool(const QString &tool)
{
    if (m_activeTool != tool) {
        m_activeTool = tool;
        Q_EMIT activeToolChanged(m_activeTool);

        // Update mode based on tool state
        if (!tool.isEmpty()) {
            if (m_currentMode != QStringLiteral("draw")) {
                m_currentMode = QStringLiteral("draw");
                Q_EMIT modeChanged(m_currentMode);
            }
            if (m_selectedIndex != -1) {
                setSelectedIndex(-1);
            }
        } else {
            if (m_currentMode == QStringLiteral("draw")) {
                m_currentMode = QStringLiteral("passthrough");
                Q_EMIT modeChanged(m_currentMode);
            }
        }
    }
}

QString OverlayController::currentMode() const
{
    return m_currentMode;
}

QString OverlayController::defaultColor() const
{
    return m_defaultColor;
}

void OverlayController::setDefaultColor(const QString &color)
{
    if (m_defaultColor != color) {
        m_defaultColor = color;
        Q_EMIT defaultColorChanged();
    }
}

int OverlayController::defaultStrokeWidth() const
{
    return m_defaultStrokeWidth;
}

void OverlayController::setDefaultStrokeWidth(int width)
{
    if (m_defaultStrokeWidth != width) {
        m_defaultStrokeWidth = width;
        Q_EMIT defaultStrokeWidthChanged();
    }
}

double OverlayController::defaultOpacity() const
{
    return m_defaultOpacity;
}

void OverlayController::setDefaultOpacity(double opacity)
{
    if (m_defaultOpacity != opacity) {
        m_defaultOpacity = opacity;
        Q_EMIT defaultOpacityChanged();
    }
}

QString OverlayController::defaultFontFamily() const
{
    return m_defaultFontFamily;
}

void OverlayController::setDefaultFontFamily(const QString &family)
{
    if (m_defaultFontFamily != family) {
        m_defaultFontFamily = family;
        Q_EMIT defaultFontFamilyChanged();
    }
}

int OverlayController::defaultFontSize() const
{
    return m_defaultFontSize;
}

void OverlayController::setDefaultFontSize(int size)
{
    if (m_defaultFontSize != size) {
        m_defaultFontSize = size;
        Q_EMIT defaultFontSizeChanged();
    }
}

void OverlayController::addShape(const QVariantMap &shape)
{
    QVariantMap demarshalledShape;
    for (auto it = shape.begin(); it != shape.end(); ++it) {
        demarshalledShape.insert(it.key(), DBusUtils::demarshal(it.value()));
    }
    qDebug() << "OverlayController::addShape - demarshalled shape:" << demarshalledShape;
    m_shapesModel.addShape(demarshalledShape);
    setSelectedIndex(m_shapesModel.rowCount() - 1);
    notifyShapesChanged();
}

QVariantMap OverlayController::getShape(int index) const
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        return m_shapesModel.shapes().at(index);
    }
    return QVariantMap();
}

void OverlayController::updateShape(int index, const QVariantMap &properties)
{
    QVariantMap demarshalledProps;
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        demarshalledProps.insert(it.key(), DBusUtils::demarshal(it.value()));
    }
    m_shapesModel.updateShape(index, demarshalledProps);
    notifyShapesChanged();
    
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
}

void OverlayController::setSelectedIndex(int index)
{
    if (m_selectedIndex != index) {
        // Clear selection state in previous shape
        if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
            m_shapesModel.updateShape(m_selectedIndex, {{QStringLiteral("selected"), false}});
        }
        
        m_selectedIndex = index;
        
        // Set selection state in new shape
        if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
            m_shapesModel.updateShape(m_selectedIndex, {{QStringLiteral("selected"), true}});
            
            // Set properties from selected shape
            QVariantMap shape = m_shapesModel.shapes()[m_selectedIndex];
            m_defaultColor = shape[QStringLiteral("color")].toString();
            m_defaultStrokeWidth = shape[QStringLiteral("strokeWidth")].toInt();
            m_defaultOpacity = shape[QStringLiteral("opacity")].toDouble();
            if (shape.contains(QStringLiteral("fontFamily"))) {
                m_defaultFontFamily = shape[QStringLiteral("fontFamily")].toString();
            }
            if (shape.contains(QStringLiteral("fontSize"))) {
                m_defaultFontSize = shape[QStringLiteral("fontSize")].toInt();
            }
            Q_EMIT defaultColorChanged();
            Q_EMIT defaultStrokeWidthChanged();
            Q_EMIT defaultOpacityChanged();
            Q_EMIT defaultFontFamilyChanged();
            Q_EMIT defaultFontSizeChanged();

            if (m_currentMode != QStringLiteral("select")) {
                m_currentMode = QStringLiteral("select");
                Q_EMIT modeChanged(m_currentMode);
            }
            if (!m_activeTool.isEmpty()) {
                m_activeTool = QStringLiteral("");
                Q_EMIT activeToolChanged(m_activeTool);
            }
        }
        
        notifyShapesChanged();
        notifySelectionChanged();
    }
}

void OverlayController::updateInputMask(const QVariantList &rects)
{
    if (!m_window) return;
    
    QRegion region;
    for (const QVariant &v : rects) {
        QRect r = v.toRectF().toRect();
        if (r.isValid()) {
            region += r;
        }
    }
    
    // Safety fallback: if region is empty, set to a 1x1 pixel so the overlay is fully click-through.
    // We cannot use off-screen coordinates or a truly empty region because Qt Wayland 
    // will treat it as "no mask" and block the entire screen.
    if (region.isEmpty()) {
        region += QRect(0, 0, 1, 1);
    }
    
    if (m_lastInputMask == region) {
        return;
    }
    m_lastInputMask = region;
    m_window->setMask(region);
}

// DBus Slots
QVariantList OverlayController::getShapesMetadata()
{
    return shapesMetadata();
}

QVariantMap OverlayController::getSelectionState()
{
    QVariantMap state;
    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        QVariantMap shape = m_shapesModel.shapes()[m_selectedIndex];
        state[QStringLiteral("hasSelection")] = true;
        state[QStringLiteral("type")] = shape[QStringLiteral("type")].toString();
        state[QStringLiteral("color")] = shape[QStringLiteral("color")].toString();
        state[QStringLiteral("strokeWidth")] = shape[QStringLiteral("strokeWidth")].toInt();
        state[QStringLiteral("opacity")] = shape[QStringLiteral("opacity")].toDouble();
        state[QStringLiteral("fontFamily")] = shape.value(QStringLiteral("fontFamily"), m_defaultFontFamily).toString();
        state[QStringLiteral("fontSize")] = shape.value(QStringLiteral("fontSize"), m_defaultFontSize).toInt();
        state[QStringLiteral("locked")] = shape.value(QStringLiteral("locked"), false).toBool();
        state[QStringLiteral("selectedIndex")] = m_selectedIndex;
    } else {
        state[QStringLiteral("hasSelection")] = false;
        state[QStringLiteral("type")] = QStringLiteral("");
        state[QStringLiteral("color")] = m_defaultColor;
        state[QStringLiteral("strokeWidth")] = m_defaultStrokeWidth;
        state[QStringLiteral("opacity")] = m_defaultOpacity;
        state[QStringLiteral("fontFamily")] = m_defaultFontFamily;
        state[QStringLiteral("fontSize")] = m_defaultFontSize;
        state[QStringLiteral("locked")] = false;
        state[QStringLiteral("selectedIndex")] = -1;
    }
    return state;
}

void OverlayController::setTool(const QString &tool)
{
    setActiveTool(tool);
}

QString OverlayController::getActiveTool()
{
    return m_activeTool;
}

void OverlayController::updateProperties(const QVariantMap &properties)
{
    QVariantMap demarshalled = DBusUtils::demarshal(properties).toMap();
    if (demarshalled.contains(QStringLiteral("color"))) {
        setDefaultColor(demarshalled[QStringLiteral("color")].toString());
    }
    if (demarshalled.contains(QStringLiteral("strokeWidth"))) {
        setDefaultStrokeWidth(demarshalled[QStringLiteral("strokeWidth")].toInt());
    }
    if (demarshalled.contains(QStringLiteral("opacity"))) {
        setDefaultOpacity(demarshalled[QStringLiteral("opacity")].toDouble());
    }
    if (demarshalled.contains(QStringLiteral("fontFamily"))) {
        setDefaultFontFamily(demarshalled[QStringLiteral("fontFamily")].toString());
    }
    if (demarshalled.contains(QStringLiteral("fontSize"))) {
        setDefaultFontSize(demarshalled[QStringLiteral("fontSize")].toInt());
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        m_shapesModel.updateShape(m_selectedIndex, demarshalled);
    }

    notifyShapesChanged();
    notifySelectionChanged();
}

void OverlayController::undo()
{
    m_shapesModel.undo();
    m_selectedIndex = -1;
    notifyShapesChanged();
    notifySelectionChanged();
}

void OverlayController::clear()
{
    m_shapesModel.clear();
    m_selectedIndex = -1;
    notifyShapesChanged();
    notifySelectionChanged();
}

void OverlayController::beginEdit()
{
    m_shapesModel.beginEdit();
}

void OverlayController::endEdit()
{
    m_shapesModel.endEdit();
}

void OverlayController::setKeyboardInteractivity(bool interactive)
{
    if (!m_window) return;
    if (auto *layerWindow = LayerShellQt::Window::get(m_window)) {
        layerWindow->setKeyboardInteractivity(interactive 
            ? LayerShellQt::Window::KeyboardInteractivityOnDemand
            : LayerShellQt::Window::KeyboardInteractivityNone);
    }
}

void OverlayController::setTargetScreen(const QString &screenName)
{
    if (!m_window || screenName.isEmpty()) return;
    
    QScreen *target = nullptr;
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == screenName) {
            target = screen;
            break;
        }
    }
    
    if (target && m_window->screen() != target) {
        m_window->hide();
        m_window->setScreen(target);
        m_window->show();
    }
}

void OverlayController::enterSelectMode()
{
    Q_EMIT enterSelectModeRequested();
    setActiveTool(QStringLiteral(""));
    if (m_currentMode != QStringLiteral("select")) {
        m_currentMode = QStringLiteral("select");
        Q_EMIT modeChanged(m_currentMode);
    }
}

void OverlayController::enterPassthroughMode()
{
    Q_EMIT enterPassthroughModeRequested();
    setActiveTool(QStringLiteral(""));
    if (m_currentMode != QStringLiteral("passthrough")) {
        m_currentMode = QStringLiteral("passthrough");
        Q_EMIT modeChanged(m_currentMode);
    }
}

void OverlayController::deleteSelected()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        deleteShape(m_selectedIndex);
    }
}

void OverlayController::toggleLock()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        QVariantMap shape = m_shapesModel.shapes()[m_selectedIndex];
        bool locked = shape.value(QStringLiteral("locked"), false).toBool();
        setShapeLocked(m_selectedIndex, !locked);
    }
}

void OverlayController::raiseSelected()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount() - 1) {
        if (m_shapesModel.moveShape(m_selectedIndex, m_selectedIndex + 1)) {
            m_selectedIndex++;
            notifyShapesChanged();
            notifySelectionChanged();
        }
    }
}

void OverlayController::lowerSelected()
{
    if (m_selectedIndex > 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        if (m_shapesModel.moveShape(m_selectedIndex, m_selectedIndex - 1)) {
            m_selectedIndex--;
            notifyShapesChanged();
            notifySelectionChanged();
        }
    }
}

void OverlayController::setShapeLocked(int index, bool locked)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        m_shapesModel.updateShape(index, {{QStringLiteral("locked"), locked}});
        // If locked, we deselect it
        if (locked && index == m_selectedIndex) {
            setSelectedIndex(-1);
        } else {
            notifyShapesChanged();
        }
    }
}

void OverlayController::deleteShape(int index)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        m_shapesModel.removeShape(index);
        if (m_selectedIndex == index) {
            m_selectedIndex = -1;
            notifySelectionChanged();
        } else if (m_selectedIndex > index) {
            m_selectedIndex--;
            notifySelectionChanged();
        }
        notifyShapesChanged();
    }
}

void OverlayController::selectShape(int index)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        setSelectedIndex(index);
    } else {
        setSelectedIndex(-1);
    }
}

void OverlayController::notifySelectionChanged()
{
    Q_EMIT selectionChanged(getSelectionState());
}

void OverlayController::notifyShapesChanged()
{
    Q_EMIT shapesMetadataChanged(shapesMetadata());
}

void OverlayController::registerAction(QAction *action, const QString &actionId, const QString &displayName)
{
    m_shortcutActions.append({action, actionId, displayName});
}

QVariantList OverlayController::getShortcuts()
{
    QVariantList list;
    for (const auto &sa : m_shortcutActions) {
        if (!sa.action) continue;
        QVariantMap map;
        map.insert(QStringLiteral("id"), sa.id);
        map.insert(QStringLiteral("name"), sa.displayName);
        
        QList<QKeySequence> current = KGlobalAccel::self()->shortcut(sa.action);
        QString shortcutStr = current.isEmpty() ? QString() : current.first().toString(QKeySequence::PortableText);
        
        QList<QKeySequence> def = KGlobalAccel::self()->defaultShortcut(sa.action);
        QString defaultStr = def.isEmpty() ? QString() : def.first().toString(QKeySequence::PortableText);
        
        map.insert(QStringLiteral("shortcut"), shortcutStr);
        map.insert(QStringLiteral("defaultShortcut"), defaultStr);
        list.append(map);
    }
    return list;
}

void OverlayController::changeShortcut(const QString &actionId, const QString &shortcutString)
{
    QKeySequence newSequence(shortcutString);
    
    // Auto-reassignment conflict resolution:
    // If the sequence is not empty, check if another action uses this shortcut.
    if (!newSequence.isEmpty()) {
        for (auto &sa : m_shortcutActions) {
            if (sa.id != actionId && sa.action) {
                QList<QKeySequence> existing = KGlobalAccel::self()->shortcut(sa.action);
                if (!existing.isEmpty() && existing.first() == newSequence) {
                    // Clear it from the existing conflicting action
                    KGlobalAccel::self()->setShortcut(sa.action, QList<QKeySequence>(), KGlobalAccel::NoAutoloading);
                }
            }
        }
    }
    
    // Assign to the target action
    for (auto &sa : m_shortcutActions) {
        if (sa.id == actionId && sa.action) {
            QList<QKeySequence> shortcuts;
            if (!newSequence.isEmpty()) {
                shortcuts.append(newSequence);
            }
            KGlobalAccel::self()->setShortcut(sa.action, shortcuts, KGlobalAccel::NoAutoloading);
            break;
        }
    }
    
    // Notify all listeners
    Q_EMIT shortcutsChanged(getShortcuts());
}

void OverlayController::copySelected()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        QVariantMap shape = m_shapesModel.shapes().at(m_selectedIndex);
        
        // Remove 'selected' state before serializing so it doesn't propagate
        shape.insert(QStringLiteral("selected"), false);
        
        QJsonObject jsonObj = QJsonObject::fromVariantMap(shape);
        QJsonDocument doc(jsonObj);
        QGuiApplication::clipboard()->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
}

void OverlayController::pasteFromClipboard(double localX, double localY)
{
    QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty()) return;

    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject jsonObj = doc.object();
    if (!jsonObj.contains(QStringLiteral("type"))) return;

    QVariantMap shape = jsonObj.toVariantMap();
    QString type = shape[QStringLiteral("type")].toString();

    // Ensure it's not marked selected or locked initially
    shape.insert(QStringLiteral("selected"), false);
    shape.insert(QStringLiteral("locked"), false);

    // Calculate center of the original shape
    double cx = 0;
    double cy = 0;

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
        cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        cx = (shape[QStringLiteral("fromX")].toDouble() + shape[QStringLiteral("toX")].toDouble()) / 2.0;
        cy = (shape[QStringLiteral("fromY")].toDouble() + shape[QStringLiteral("toY")].toDouble()) / 2.0;
    } else if (type == QStringLiteral("freehand")) {
        QVariantList points = shape[QStringLiteral("points")].toList();
        if (points.isEmpty()) return;

        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();

        for (const QVariant &pv : points) {
            double px = 0;
            double py = 0;
            if (pv.canConvert<QPointF>()) {
                QPointF p = pv.toPointF();
                px = p.x();
                py = p.y();
            } else if (pv.typeId() == QMetaType::QVariantMap) {
                QVariantMap pm = pv.toMap();
                px = pm[QStringLiteral("x")].toDouble();
                py = pm[QStringLiteral("y")].toDouble();
            } else {
                continue;
            }
            if (px < minX) minX = px;
            if (px > maxX) maxX = px;
            if (py < minY) minY = py;
            if (py > maxY) maxY = py;
        }

        if (minX <= maxX && minY <= maxY) {
            cx = (minX + maxX) / 2.0;
            cy = (minY + maxY) / 2.0;
        }
    } else {
        // Unknown shape type
        return;
    }

    // Determine target local mouse position
    QPointF localMousePos;
    if (localX >= 0.0 && localY >= 0.0) {
        localMousePos = QPointF(localX, localY);
    } else {
        // Get current global mouse position and map it to window
        QPoint globalMousePos = QCursor::pos();
        localMousePos = globalMousePos;
        if (m_window) {
            localMousePos = m_window->mapFromGlobal(globalMousePos);
        }
    }

    // Calculate delta to translate center to mouse position
    double dx = localMousePos.x() - cx;
    double dy = localMousePos.y() - cy;

    // Apply delta to coordinates
    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        shape.insert(QStringLiteral("x"), shape[QStringLiteral("x")].toDouble() + dx);
        shape.insert(QStringLiteral("y"), shape[QStringLiteral("y")].toDouble() + dy);
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        shape.insert(QStringLiteral("fromX"), shape[QStringLiteral("fromX")].toDouble() + dx);
        shape.insert(QStringLiteral("toX"), shape[QStringLiteral("toX")].toDouble() + dx);
        shape.insert(QStringLiteral("fromY"), shape[QStringLiteral("fromY")].toDouble() + dy);
        shape.insert(QStringLiteral("toY"), shape[QStringLiteral("toY")].toDouble() + dy);
    } else if (type == QStringLiteral("freehand")) {
        QVariantList points = shape[QStringLiteral("points")].toList();
        QVariantList newPoints;
        for (const QVariant &pv : points) {
            if (pv.canConvert<QPointF>()) {
                QPointF p = pv.toPointF();
                newPoints.append(QPointF(p.x() + dx, p.y() + dy));
            } else if (pv.typeId() == QMetaType::QVariantMap) {
                QVariantMap pm = pv.toMap();
                pm.insert(QStringLiteral("x"), pm[QStringLiteral("x")].toDouble() + dx);
                pm.insert(QStringLiteral("y"), pm[QStringLiteral("y")].toDouble() + dy);
                newPoints.append(pm);
            }
        }
        shape.insert(QStringLiteral("points"), newPoints);
    }

    addShape(shape);
}


