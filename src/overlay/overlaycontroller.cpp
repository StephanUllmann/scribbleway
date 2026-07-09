#include "overlaycontroller.h"
#include <QSettings>
#include <QStringList>

static const QStringList presetColors = {
    QStringLiteral("#e63946"),
    QStringLiteral("#f4a261"),
    QStringLiteral("#e9c46a"),
    QStringLiteral("#2a9d8f"),
    QStringLiteral("#457b9d"),
    QStringLiteral("#8338ec")
};
#include <QDebug>
#include <LayerShellQt/Window>
#include <QScreen>
#include <QGuiApplication>
#include <QUuid>
#include <KGlobalAccel>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QColor>
#include <QCursor>
#include <QPointF>
#include <QLineF>
#include <limits>

#include "../common/dbusutils.h"

OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
    , m_activeTool(QStringLiteral("freehand"))
    , m_defaultColor(QStringLiteral("#e63946"))
{
    m_defaultFontFamily = QStringLiteral("monospace");
    m_shapesModel.setParent(this);

    m_localShortcuts = {
        {QStringLiteral("tool_arrow"), QStringLiteral("Arrow"), QStringLiteral("A"), QStringLiteral("A")},
        {QStringLiteral("tool_rectangle"), QStringLiteral("Rectangle"), QStringLiteral("R"), QStringLiteral("R")},
        {QStringLiteral("tool_freehand"), QStringLiteral("Freehand"), QStringLiteral("F"), QStringLiteral("F")},
        {QStringLiteral("tool_ellipse"), QStringLiteral("Ellipse"), QStringLiteral("E"), QStringLiteral("E")},
        {QStringLiteral("tool_line"), QStringLiteral("Line"), QStringLiteral("L"), QStringLiteral("L")},
        {QStringLiteral("tool_text"), QStringLiteral("Text"), QStringLiteral("T"), QStringLiteral("T")},
        {QStringLiteral("color_cycle"), QStringLiteral("Cycle Color"), QStringLiteral("Q"), QStringLiteral("Q")},
        {QStringLiteral("color_1"), QStringLiteral("Red"), QStringLiteral("1"), QStringLiteral("1")},
        {QStringLiteral("color_2"), QStringLiteral("Orange"), QStringLiteral("2"), QStringLiteral("2")},
        {QStringLiteral("color_3"), QStringLiteral("Yellow"), QStringLiteral("3"), QStringLiteral("3")},
        {QStringLiteral("color_4"), QStringLiteral("Green"), QStringLiteral("4"), QStringLiteral("4")},
        {QStringLiteral("color_5"), QStringLiteral("Blue"), QStringLiteral("5"), QStringLiteral("5")},
        {QStringLiteral("color_6"), QStringLiteral("Violet"), QStringLiteral("6"), QStringLiteral("6")},
        {QStringLiteral("action_grow"), QStringLiteral("Grow"), QStringLiteral("+"), QStringLiteral("+")},
        {QStringLiteral("action_shrink"), QStringLiteral("Shrink"), QStringLiteral("-"), QStringLiteral("-")},
        {QStringLiteral("action_select"), QStringLiteral("Select Mode"), QStringLiteral("X"), QStringLiteral("X")},
        {QStringLiteral("action_cycle_roughness"), QStringLiteral("Cycle Roughness"), QStringLiteral("S"), QStringLiteral("S")},
        {QStringLiteral("action_undo"), QStringLiteral("Undo"), QStringLiteral("Ctrl+Z"), QStringLiteral("Ctrl+Z")},
        {QStringLiteral("action_clear"), QStringLiteral("Clear All"), QStringLiteral("Ctrl+Delete"), QStringLiteral("Ctrl+Delete")}
    };

    QSettings settings(QStringLiteral("scribbleway"), QStringLiteral("shortcuts"));
    for (auto &def : m_localShortcuts) {
        QString key = QStringLiteral("local/") + def.id;
        if (settings.contains(key)) {
            def.currentSequence = settings.value(key).toString();
        }
    }
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

int OverlayController::defaultBorderRadius() const
{
    return m_defaultBorderRadius;
}

void OverlayController::setDefaultBorderRadius(int radius)
{
    if (m_defaultBorderRadius != radius) {
        m_defaultBorderRadius = radius;
        Q_EMIT defaultBorderRadiusChanged();
    }
}

int OverlayController::defaultRoughness() const
{
    return m_defaultRoughness;
}

void OverlayController::setDefaultRoughness(int roughness)
{
    if (m_defaultRoughness != roughness) {
        m_defaultRoughness = roughness;
        Q_EMIT defaultRoughnessChanged();
    }
}

void OverlayController::addShape(const QVariantMap &shape)
{
    QVariantMap demarshalledShape = DBusUtils::demarshal(shape).toMap();
    qDebug() << "OverlayController::addShape - demarshalled shape:" << demarshalledShape;
    m_shapesModel.beginEdit();
    m_shapesModel.addShape(demarshalledShape);
    setSelectedIndex(m_shapesModel.rowCount() - 1);
    m_shapesModel.endEdit();
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
    QVariantMap demarshalledProps = DBusUtils::demarshal(properties).toMap();
    m_shapesModel.updateShape(index, demarshalledProps);
    notifyShapesChanged();
    
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
}

void OverlayController::setSelectedIndex(int index)
{
    if (m_selectedIndex != index || index == -1) {
        m_shapesModel.beginEdit();
        // Clear selection state in all shapes
        for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
            if (i != index && m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
                m_shapesModel.updateShape(i, {{QStringLiteral("selected"), false}});
            }
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
            if (shape.contains(QStringLiteral("borderRadius"))) {
                m_defaultBorderRadius = shape[QStringLiteral("borderRadius")].toInt();
            }
            if (shape.contains(QStringLiteral("roughness"))) {
                m_defaultRoughness = shape[QStringLiteral("roughness")].toInt();
            }
            Q_EMIT defaultColorChanged();
            Q_EMIT defaultStrokeWidthChanged();
            Q_EMIT defaultOpacityChanged();
            Q_EMIT defaultFontFamilyChanged();
            Q_EMIT defaultFontSizeChanged();
            Q_EMIT defaultBorderRadiusChanged();
            Q_EMIT defaultRoughnessChanged();

            ensureSelectMode();
        }
        m_shapesModel.endEdit();
        
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
        state[QStringLiteral("borderRadius")] = shape.value(QStringLiteral("borderRadius"), m_defaultBorderRadius).toInt();
        state[QStringLiteral("roughness")] = shape.value(QStringLiteral("roughness"), m_defaultRoughness).toInt();
        state[QStringLiteral("seed")] = shape.value(QStringLiteral("seed"), 123456).toInt();
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
        state[QStringLiteral("borderRadius")] = m_defaultBorderRadius;
        state[QStringLiteral("roughness")] = m_defaultRoughness;
        state[QStringLiteral("seed")] = 123456;
        state[QStringLiteral("locked")] = false;
        state[QStringLiteral("selectedIndex")] = -1;
    }
    return state;
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
    if (demarshalled.contains(QStringLiteral("borderRadius"))) {
        setDefaultBorderRadius(demarshalled[QStringLiteral("borderRadius")].toInt());
    }
    if (demarshalled.contains(QStringLiteral("roughness"))) {
        setDefaultRoughness(demarshalled[QStringLiteral("roughness")].toInt());
    }

    m_shapesModel.beginEdit();
    bool updatedAny = false;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            m_shapesModel.updateShape(i, demarshalled);
            updatedAny = true;
        }
    }

    if (!updatedAny && m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        m_shapesModel.updateShape(m_selectedIndex, demarshalled);
    }
    m_shapesModel.endEdit();

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
    m_dragStartShapes.clear();
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            m_dragStartShapes.insert(i, m_shapesModel.shapes()[i]);
        }
    }
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
    m_shapesModel.beginEdit();
    bool deletedAny = false;
    for (int i = m_shapesModel.rowCount() - 1; i >= 0; --i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            deleteShape(i);
            deletedAny = true;
        }
    }
    if (!deletedAny && m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        deleteShape(m_selectedIndex);
    }
    m_shapesModel.endEdit();
}

void OverlayController::toggleLock()
{
    m_shapesModel.beginEdit();
    bool lockedAny = false;
    bool hasUnlockedSelected = false;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            if (!m_shapesModel.shapes()[i].value(QStringLiteral("locked"), false).toBool()) {
                hasUnlockedSelected = true;
                break;
            }
        }
    }
    
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            setShapeLocked(i, hasUnlockedSelected);
            lockedAny = true;
        }
    }
    if (!lockedAny && m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        bool locked = m_shapesModel.shapes()[m_selectedIndex].value(QStringLiteral("locked"), false).toBool();
        setShapeLocked(m_selectedIndex, !locked);
    }
    m_shapesModel.endEdit();
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
        m_shapesModel.beginEdit();
        m_shapesModel.updateShape(index, {{QStringLiteral("locked"), locked}});
        // If locked, we deselect it
        if (locked && index == m_selectedIndex) {
            setSelectedIndex(-1);
        } else {
            notifyShapesChanged();
        }
        m_shapesModel.endEdit();
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

void OverlayController::selectShape(int index, bool shiftHeld)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        if (!shiftHeld) {
            setSelectedIndex(-1);
        }
        return;
    }

    bool wasSelected = m_shapesModel.shapes()[index].value(QStringLiteral("selected"), false).toBool();

    if (shiftHeld) {
        m_shapesModel.updateShape(index, {{QStringLiteral("selected"), !wasSelected}});
        
        if (!wasSelected) {
            m_selectedIndex = index;
            ensureSelectMode();
        } else {
            if (m_selectedIndex == index) {
                int newSelected = -1;
                for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
                    if (m_shapesModel.shapes()[i].value(QStringLiteral("selected"), false).toBool()) {
                        newSelected = i;
                        break;
                    }
                }
                m_selectedIndex = newSelected;
            }
        }
        notifyShapesChanged();
        notifySelectionChanged();
    } else {
        if (wasSelected) {
            if (m_selectedIndex != index) {
                m_selectedIndex = index;
                notifySelectionChanged();
            }
        } else {
            m_shapesModel.beginEdit();
            for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
                if (i != index && m_shapesModel.shapes()[i].value(QStringLiteral("selected"), false).toBool()) {
                    m_shapesModel.updateShape(i, {{QStringLiteral("selected"), false}});
                }
            }
            m_shapesModel.updateShape(index, {{QStringLiteral("selected"), true}});
            m_selectedIndex = index;
            m_shapesModel.endEdit();

            ensureSelectMode();
            notifyShapesChanged();
            notifySelectionChanged();
        }
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

void OverlayController::ensureSelectMode()
{
    if (m_currentMode != QStringLiteral("select")) {
        m_currentMode = QStringLiteral("select");
        Q_EMIT modeChanged(m_currentMode);
    }
    if (!m_activeTool.isEmpty()) {
        m_activeTool = QStringLiteral("");
        Q_EMIT activeToolChanged(m_activeTool);
    }
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
        map.insert(QStringLiteral("type"), QStringLiteral("global"));
        list.append(map);
    }

    for (const auto &ls : m_localShortcuts) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), ls.id);
        map.insert(QStringLiteral("name"), ls.displayName);
        map.insert(QStringLiteral("shortcut"), ls.currentSequence);
        map.insert(QStringLiteral("defaultShortcut"), ls.defaultSequence);
        map.insert(QStringLiteral("type"), QStringLiteral("local"));
        list.append(map);
    }
    return list;
}

void OverlayController::changeShortcut(const QString &actionId, const QString &shortcutString)
{
    QSettings settings(QStringLiteral("scribbleway"), QStringLiteral("shortcuts"));
    QKeySequence newSequence(shortcutString);
    bool isGlobal = false;
    for (const auto &sa : m_shortcutActions) {
        if (sa.id == actionId) {
            isGlobal = true;
            break;
        }
    }

    if (isGlobal) {
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

            // Cross-domain conflict: check if a local shortcut uses the new sequence
            bool localConflictFound = false;
            for (auto &ls : m_localShortcuts) {
                if (QKeySequence(ls.currentSequence) == newSequence) {
                    ls.currentSequence = QString();
                    settings.setValue(QStringLiteral("local/") + ls.id, ls.currentSequence);
                    localConflictFound = true;
                }
            }
            if (localConflictFound) {
                Q_EMIT localShortcutsChanged();
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
    } else {
        // Local shortcut
        if (!newSequence.isEmpty()) {
            // Check if another local shortcut uses the sequence
            for (auto &ls : m_localShortcuts) {
                if (ls.id != actionId && QKeySequence(ls.currentSequence) == newSequence) {
                    ls.currentSequence = QString();
                    settings.setValue(QStringLiteral("local/") + ls.id, ls.currentSequence);
                }
            }

            // Check if a global shortcut uses the sequence
            for (auto &sa : m_shortcutActions) {
                if (sa.action) {
                    QList<QKeySequence> existing = KGlobalAccel::self()->shortcut(sa.action);
                    if (!existing.isEmpty() && existing.first() == newSequence) {
                        KGlobalAccel::self()->setShortcut(sa.action, QList<QKeySequence>(), KGlobalAccel::NoAutoloading);
                    }
                }
            }
        }

        // Update the target local shortcut
        for (auto &ls : m_localShortcuts) {
            if (ls.id == actionId) {
                ls.currentSequence = shortcutString;
                settings.setValue(QStringLiteral("local/") + ls.id, ls.currentSequence);
                break;
            }
        }
        Q_EMIT localShortcutsChanged();
    }

    // Notify all listeners
    Q_EMIT shortcutsChanged(getShortcuts());
}

QVariantMap OverlayController::localShortcutSequences() const
{
    QVariantMap map;
    for (const auto &ls : m_localShortcuts) {
        map.insert(ls.id, ls.currentSequence);
    }
    return map;
}

void OverlayController::selectPresetColor(int index)
{
    if (index >= 0 && index < presetColors.size()) {
        updateProperties({{QStringLiteral("color"), presetColors[index]}});
    }
}

void OverlayController::toggleTool(const QString &tool)
{
    if (m_activeTool == tool) {
        enterSelectMode();
    } else {
        setActiveTool(tool);
    }
}

void OverlayController::cycleColor()
{
    QVariantMap state = getSelectionState();
    QString currentColor = state.value(QStringLiteral("color")).toString().toLower();
    int idx = presetColors.indexOf(currentColor);
    int nextIdx = (idx + 1) % presetColors.size();
    updateProperties({{QStringLiteral("color"), presetColors[nextIdx]}});
}

void OverlayController::cycleRoughness()
{
    if (!m_activeTool.isEmpty()) {
        // S alone should go from Draw into Select
        enterSelectMode();
        return;
    }
    
    // Otherwise, place the roughness cycling on S
    int currentRoughness = m_defaultRoughness;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            currentRoughness = m_shapesModel.shapes()[i].value(QStringLiteral("roughness"), 1).toInt();
            break;
        }
    }
    
    int nextRoughness = (currentRoughness + 1) % 3;
    updateProperties({{QStringLiteral("roughness"), nextRoughness}});
}

void OverlayController::growSelected()
{
    QVariantMap state = getSelectionState();
    if (!state.value(QStringLiteral("hasSelection")).toBool()) return;
    QString type = state.value(QStringLiteral("type")).toString();
    if (type.toLower() == QStringLiteral("text")) {
        int fontSize = state.value(QStringLiteral("fontSize"), 20).toInt();
        updateProperties({{QStringLiteral("fontSize"), fontSize + 2}});
    } else {
        int strokeWidth = state.value(QStringLiteral("strokeWidth"), 2).toInt();
        updateProperties({{QStringLiteral("strokeWidth"), qMin(strokeWidth + 1, 15)}});
    }
}

void OverlayController::shrinkSelected()
{
    QVariantMap state = getSelectionState();
    if (!state.value(QStringLiteral("hasSelection")).toBool()) return;
    QString type = state.value(QStringLiteral("type")).toString();
    if (type.toLower() == QStringLiteral("text")) {
        int fontSize = state.value(QStringLiteral("fontSize"), 20).toInt();
        updateProperties({{QStringLiteral("fontSize"), qMax(fontSize - 2, 10)}});
    } else {
        int strokeWidth = state.value(QStringLiteral("strokeWidth"), 2).toInt();
        updateProperties({{QStringLiteral("strokeWidth"), qMax(strokeWidth - 1, 1)}});
    }
}
void OverlayController::copySelected()
{
    QJsonArray elements;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            QVariantMap shape = m_shapesModel.shapes().at(i);
            QJsonObject elemObj = convertToExcalidraw(shape);
            if (!elemObj.isEmpty()) {
                elements.append(elemObj);
            }
        }
    }

    if (!elements.isEmpty()) {
        QJsonObject rootObj;
        rootObj.insert(QStringLiteral("type"), QStringLiteral("excalidraw/clipboard"));
        rootObj.insert(QStringLiteral("elements"), elements);
        QJsonDocument doc(rootObj);
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
    QVariantList list;
    // ponytail: only support excalidraw clipboard format, add fallback if other formats are needed
    if (jsonObj.value(QStringLiteral("type")).toString() == QStringLiteral("excalidraw/clipboard")) {
        QJsonArray elements = jsonObj.value(QStringLiteral("elements")).toArray();
        for (const QJsonValue &val : elements) {
            QJsonObject elemObj = val.toObject();
            QVariantMap shape = convertFromExcalidraw(elemObj);
            if (!shape.isEmpty()) {
                list.append(shape);
            }
        }
    }

    if (list.isEmpty()) return;

    QPointF localMousePos;
    if (localX >= 0.0 && localY >= 0.0) {
        localMousePos = QPointF(localX, localY);
    } else {
        QPoint globalMousePos = QCursor::pos();
        localMousePos = globalMousePos;
        if (m_window) {
            localMousePos = m_window->mapFromGlobal(globalMousePos);
        }
    }

    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (const QVariant &sv : list) {
        QVariantMap shape = sv.toMap();
        QString type = shape[QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
            double x = shape[QStringLiteral("x")].toDouble();
            double y = shape[QStringLiteral("y")].toDouble();
            double w = shape[QStringLiteral("width")].toDouble();
            double h = shape[QStringLiteral("height")].toDouble();
            minX = qMin(minX, x);
            maxX = qMax(maxX, x + w);
            minY = qMin(minY, y);
            maxY = qMax(maxY, y + h);
        } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            double fx = shape[QStringLiteral("fromX")].toDouble();
            double tx = shape[QStringLiteral("toX")].toDouble();
            double fy = shape[QStringLiteral("fromY")].toDouble();
            double ty = shape[QStringLiteral("toY")].toDouble();
            minX = qMin(minX, qMin(fx, tx));
            maxX = qMax(maxX, qMax(fx, tx));
            minY = qMin(minY, qMin(fy, ty));
            maxY = qMax(maxY, qMax(fy, ty));
        } else if (type == QStringLiteral("freehand")) {
            QVariantList points = shape[QStringLiteral("points")].toList();
            for (const QVariant &pv : points) {
                double px = 0, py = 0;
                if (pv.canConvert<QPointF>()) {
                    QPointF p = pv.toPointF();
                    px = p.x(); py = p.y();
                } else if (pv.typeId() == QMetaType::QVariantMap) {
                    QVariantMap pm = pv.toMap();
                    px = pm[QStringLiteral("x")].toDouble();
                    py = pm[QStringLiteral("y")].toDouble();
                } else {
                    continue;
                }
                minX = qMin(minX, px);
                maxX = qMax(maxX, px);
                minY = qMin(minY, py);
                maxY = qMax(maxY, py);
            }
        }
    }

    if (minX > maxX || minY > maxY) return;

    double cx = (minX + maxX) / 2.0;
    double cy = (minY + maxY) / 2.0;
    double dx = localMousePos.x() - cx;
    double dy = localMousePos.y() - cy;

    m_shapesModel.beginEdit();
    setSelectedIndex(-1);

    QList<int> pastedIndices;
    for (const QVariant &sv : list) {
        QVariantMap shape = sv.toMap();
        QString type = shape[QStringLiteral("type")].toString();
        shape.insert(QStringLiteral("selected"), true);
        shape.insert(QStringLiteral("locked"), false);

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

        m_shapesModel.addShape(shape);
        pastedIndices.append(m_shapesModel.rowCount() - 1);
    }

    if (!pastedIndices.isEmpty()) {
        m_selectedIndex = pastedIndices.last();
    }

    m_shapesModel.endEdit();
    notifyShapesChanged();
    notifySelectionChanged();
}

QJsonObject OverlayController::convertToExcalidraw(const QVariantMap &shape)
{
    QJsonObject elem;
    QString type = shape.value(QStringLiteral("type")).toString();
    
    QString excalType = (type == QStringLiteral("freehand")) ? QStringLiteral("freedraw") : type;
    if (excalType != QStringLiteral("rectangle") && excalType != QStringLiteral("ellipse") &&
        excalType != QStringLiteral("text") && excalType != QStringLiteral("line") &&
        excalType != QStringLiteral("arrow") && excalType != QStringLiteral("freedraw")) {
        return elem;
    }
    elem.insert(QStringLiteral("type"), excalType);

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).mid(0, 8);
    elem.insert(QStringLiteral("id"), id);
    
    elem.insert(QStringLiteral("strokeColor"), shape.value(QStringLiteral("color"), QStringLiteral("#000000")).toString());
    elem.insert(QStringLiteral("strokeWidth"), shape.value(QStringLiteral("strokeWidth"), 1.0).toDouble());
    
    double opacityFloat = shape.value(QStringLiteral("opacity"), 1.0).toDouble();
    elem.insert(QStringLiteral("opacity"), qRound(opacityFloat * 100.0));
    
    elem.insert(QStringLiteral("backgroundColor"), QStringLiteral("transparent"));
    elem.insert(QStringLiteral("fillStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("strokeStyle"), QStringLiteral("solid"));
    elem.insert(QStringLiteral("roughness"), shape.value(QStringLiteral("roughness"), 1).toInt());
    elem.insert(QStringLiteral("angle"), 0.0);
    elem.insert(QStringLiteral("isDeleted"), false);
    elem.insert(QStringLiteral("seed"), shape.value(QStringLiteral("seed"), 123456).toInt());
    elem.insert(QStringLiteral("version"), 1);
    elem.insert(QStringLiteral("versionNonce"), 123456789);
    elem.insert(QStringLiteral("updated"), 0);
    elem.insert(QStringLiteral("locked"), shape.value(QStringLiteral("locked"), false).toBool());

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        elem.insert(QStringLiteral("x"), shape.value(QStringLiteral("x")).toDouble());
        elem.insert(QStringLiteral("y"), shape.value(QStringLiteral("y")).toDouble());
        elem.insert(QStringLiteral("width"), shape.value(QStringLiteral("width")).toDouble());
        elem.insert(QStringLiteral("height"), shape.value(QStringLiteral("height")).toDouble());
        
        if (type == QStringLiteral("rectangle")) {
            int borderRadius = shape.value(QStringLiteral("borderRadius"), 0).toInt();
            if (borderRadius > 0) {
                QJsonObject roundnessObj;
                roundnessObj.insert(QStringLiteral("type"), 3);
                roundnessObj.insert(QStringLiteral("value"), borderRadius);
                elem.insert(QStringLiteral("roundness"), roundnessObj);
            } else {
                elem.insert(QStringLiteral("roundness"), QJsonValue::Null);
            }
        } else if (type == QStringLiteral("text")) {
            elem.insert(QStringLiteral("text"), shape.value(QStringLiteral("text")).toString());
            elem.insert(QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), 20).toInt());
            
            QString family = shape.value(QStringLiteral("fontFamily")).toString().toLower();
            int excalFont = 2;
            if (family.contains(QStringLiteral("code")) || family.contains(QStringLiteral("mono"))) {
                excalFont = 3;
            } else if (family.contains(QStringLiteral("hand")) || family.contains(QStringLiteral("script")) || family.contains(QStringLiteral("virgil"))) {
                excalFont = 1;
            }
            elem.insert(QStringLiteral("fontFamily"), excalFont);
        }
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        double fromX = shape.value(QStringLiteral("fromX")).toDouble();
        double fromY = shape.value(QStringLiteral("fromY")).toDouble();
        double toX = shape.value(QStringLiteral("toX")).toDouble();
        double toY = shape.value(QStringLiteral("toY")).toDouble();
        
        elem.insert(QStringLiteral("x"), fromX);
        elem.insert(QStringLiteral("y"), fromY);
        elem.insert(QStringLiteral("width"), qAbs(toX - fromX));
        elem.insert(QStringLiteral("height"), qAbs(toY - fromY));
        
        QJsonArray pts = { QJsonArray{0.0, 0.0}, QJsonArray{toX - fromX, toY - fromY} };
        elem.insert(QStringLiteral("points"), pts);
    } else if (type == QStringLiteral("freehand")) {
        QVariantList pointsList = shape.value(QStringLiteral("points")).toList();
        if (pointsList.isEmpty()) {
            return elem;
        }
        
        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();
        
        QList<QPointF> qpts;
        for (const QVariant &v : pointsList) {
            QPointF p;
            if (v.canConvert<QPointF>()) {
                p = v.toPointF();
            } else if (v.typeId() == QMetaType::QVariantMap) {
                QVariantMap m = v.toMap();
                p = QPointF(m.value(QStringLiteral("x")).toDouble(), m.value(QStringLiteral("y")).toDouble());
            } else {
                continue;
            }
            qpts.append(p);
            minX = qMin(minX, p.x());
            maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
        
        if (qpts.isEmpty()) {
            return elem;
        }
        
        elem.insert(QStringLiteral("x"), minX);
        elem.insert(QStringLiteral("y"), minY);
        elem.insert(QStringLiteral("width"), maxX - minX);
        elem.insert(QStringLiteral("height"), maxY - minY);
        
        QJsonArray pts;
        for (const QPointF &p : qpts) {
            QJsonArray ptVal;
            ptVal.append(p.x() - minX);
            ptVal.append(p.y() - minY);
            pts.append(ptVal);
        }
        elem.insert(QStringLiteral("points"), pts);
    }

    return elem;
}

QVariantMap OverlayController::convertFromExcalidraw(const QJsonObject &elem)
{
    QVariantMap shape;
    QString excalType = elem.value(QStringLiteral("type")).toString();
    
    QString type = (excalType == QStringLiteral("freedraw")) ? QStringLiteral("freehand") : excalType;
    if (type != QStringLiteral("rectangle") && type != QStringLiteral("ellipse") &&
        type != QStringLiteral("text") && type != QStringLiteral("line") &&
        type != QStringLiteral("arrow") && type != QStringLiteral("freehand")) {
        return shape;
    }
    shape.insert(QStringLiteral("type"), type);

    QString strokeColor = elem.value(QStringLiteral("strokeColor")).toString(QStringLiteral("#000000"));
    if (QColor(strokeColor) == Qt::white) {
        strokeColor = QStringLiteral("#e63946");
    }
    shape.insert(QStringLiteral("color"), strokeColor);
    shape.insert(QStringLiteral("strokeWidth"), elem.value(QStringLiteral("strokeWidth")).toDouble(1.0));
    
    double opacityInt = elem.value(QStringLiteral("opacity")).toDouble(100.0);
    shape.insert(QStringLiteral("opacity"), opacityInt / 100.0);
    
    shape.insert(QStringLiteral("selected"), true);
    shape.insert(QStringLiteral("locked"), false);
    shape.insert(QStringLiteral("roughness"), elem.value(QStringLiteral("roughness")).toInt(1));
    shape.insert(QStringLiteral("seed"), elem.value(QStringLiteral("seed")).toInt(123456));

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        shape.insert(QStringLiteral("x"), elem.value(QStringLiteral("x")).toDouble());
        shape.insert(QStringLiteral("y"), elem.value(QStringLiteral("y")).toDouble());
        shape.insert(QStringLiteral("width"), elem.value(QStringLiteral("width")).toDouble());
        shape.insert(QStringLiteral("height"), elem.value(QStringLiteral("height")).toDouble());
        
        if (type == QStringLiteral("rectangle")) {
            QJsonValue rv = elem.value(QStringLiteral("roundness"));
            int radius = rv.isObject() ? static_cast<int>(rv.toObject().value(QStringLiteral("value")).toDouble(8.0)) : (rv.isNull() ? 0 : 8);
            shape.insert(QStringLiteral("borderRadius"), radius);
        } else if (type == QStringLiteral("text")) {
            shape.insert(QStringLiteral("text"), elem.value(QStringLiteral("text")).toString());
            shape.insert(QStringLiteral("fontSize"), static_cast<int>(elem.value(QStringLiteral("fontSize")).toDouble(20.0)));
            
            int excalFont = static_cast<int>(elem.value(QStringLiteral("fontFamily")).toDouble(2.0));
            QString family;
            if (excalFont == 3) {
                family = QStringLiteral("Cascadia Code");
            } else if (excalFont == 1) {
                family = QStringLiteral("Virgil");
            } else {
                family = QStringLiteral("sans-serif");
            }
            shape.insert(QStringLiteral("fontFamily"), family);
        }
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        double x = elem.value(QStringLiteral("x")).toDouble();
        double y = elem.value(QStringLiteral("y")).toDouble();
        QJsonArray pts = elem.value(QStringLiteral("points")).toArray();
        
        double fromX = x;
        double fromY = y;
        double toX = x;
        double toY = y;
        
        if (pts.size() >= 2) {
            QJsonArray p1 = pts.first().toArray();
            QJsonArray p2 = pts.last().toArray();
            if (p1.size() >= 2 && p2.size() >= 2) {
                fromX = x + p1.at(0).toDouble();
                fromY = y + p1.at(1).toDouble();
                toX = x + p2.at(0).toDouble();
                toY = y + p2.at(1).toDouble();
            }
        }
        
        shape.insert(QStringLiteral("fromX"), fromX);
        shape.insert(QStringLiteral("fromY"), fromY);
        shape.insert(QStringLiteral("toX"), toX);
        shape.insert(QStringLiteral("toY"), toY);
    } else if (type == QStringLiteral("freehand")) {
        double x = elem.value(QStringLiteral("x")).toDouble();
        double y = elem.value(QStringLiteral("y")).toDouble();
        QJsonArray pts = elem.value(QStringLiteral("points")).toArray();
        
        QVariantList pointsList;
        for (const QJsonValue &v : pts) {
            QJsonArray pt = v.toArray();
            if (pt.size() >= 2) {
                QPointF p(x + pt.at(0).toDouble(), y + pt.at(1).toDouble());
                pointsList.append(QVariant::fromValue(p));
            }
        }
        shape.insert(QStringLiteral("points"), pointsList);
    }

    return shape;
}

bool OverlayController::hasMultiSelection() const
{
    int count = 0;
    const auto shapesList = m_shapesModel.shapes();
    for (const auto &shape : shapesList) {
        if (shape.value(QStringLiteral("selected"), false).toBool()) {
            count++;
            if (count > 1) {
                return true;
            }
        }
    }
    return false;
}

void OverlayController::beginDragSelection(bool shiftHeld)
{
    m_preDragSelection.clear();
    if (shiftHeld) {
        for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
            if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
                m_preDragSelection.insert(i);
            }
        }
    }
}

void OverlayController::selectShapesInRect(double rx, double ry, double rw, double rh, bool shiftHeld)
{
    Q_UNUSED(shiftHeld);
    QRectF selectRect(rx, ry, rw, rh);
    selectRect = selectRect.normalized();

    m_shapesModel.beginEdit();

    int newSelectedIndex = -1;

    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        QVariantMap shape = m_shapesModel.shapes()[i];
        QString type = shape[QStringLiteral("type")].toString();
        bool intersects = false;

        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
            double sx = shape[QStringLiteral("x")].toDouble();
            double sy = shape[QStringLiteral("y")].toDouble();
            double sw = shape[QStringLiteral("width")].toDouble();
            double sh = shape[QStringLiteral("height")].toDouble();
            QRectF shapeRect(sx, sy, sw, sh);
            intersects = selectRect.intersects(shapeRect) || selectRect.contains(shapeRect);
        } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            double fx = shape[QStringLiteral("fromX")].toDouble();
            double fy = shape[QStringLiteral("fromY")].toDouble();
            double tx = shape[QStringLiteral("toX")].toDouble();
            double ty = shape[QStringLiteral("toY")].toDouble();

            QLineF shapeLine(fx, fy, tx, ty);
            double srx = selectRect.x();
            double sry = selectRect.y();
            double srw = selectRect.width();
            double srh = selectRect.height();

            intersects = selectRect.contains(QPointF(fx, fy)) || selectRect.contains(QPointF(tx, ty)) ||
                         shapeLine.intersects(QLineF(srx, sry, srx, sry + srh)) == QLineF::BoundedIntersection ||
                         shapeLine.intersects(QLineF(srx + srw, sry, srx + srw, sry + srh)) == QLineF::BoundedIntersection ||
                         shapeLine.intersects(QLineF(srx, sry, srx + srw, sry)) == QLineF::BoundedIntersection ||
                         shapeLine.intersects(QLineF(srx, sry + srh, srx + srw, sry + srh)) == QLineF::BoundedIntersection;
        } else if (type == QStringLiteral("freehand")) {
            QVariantList points = shape[QStringLiteral("points")].toList();
            for (const QVariant &pv : points) {
                QPointF p;
                if (pv.canConvert<QPointF>()) {
                    p = pv.toPointF();
                } else if (pv.typeId() == QMetaType::QVariantMap) {
                    QVariantMap pm = pv.toMap();
                    p = QPointF(pm[QStringLiteral("x")].toDouble(), pm[QStringLiteral("y")].toDouble());
                } else {
                    continue;
                }
                if (selectRect.contains(p)) {
                    intersects = true;
                    break;
                }
            }
        }

        bool shouldSelect = intersects || m_preDragSelection.contains(i);

        m_shapesModel.updateShape(i, {{QStringLiteral("selected"), shouldSelect}});
        if (shouldSelect) {
            newSelectedIndex = i;
        }
    }

    m_shapesModel.endEdit();

    if (newSelectedIndex != -1) {
        m_selectedIndex = newSelectedIndex;
    } else {
        m_selectedIndex = -1;
    }

    notifyShapesChanged();
    notifySelectionChanged();
}

void OverlayController::dragSelected(double dx, double dy)
{
    if (m_dragStartShapes.isEmpty()) {
        return;
    }

    m_shapesModel.beginEdit();
    for (auto it = m_dragStartShapes.begin(); it != m_dragStartShapes.end(); ++it) {
        int index = it.key();
        QVariantMap shape = it.value();
        QString type = shape[QStringLiteral("type")].toString();

        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
            shape.insert(QStringLiteral("x"), shape[QStringLiteral("x")].toDouble() + dx);
            shape.insert(QStringLiteral("y"), shape[QStringLiteral("y")].toDouble() + dy);
        } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            shape.insert(QStringLiteral("fromX"), shape[QStringLiteral("fromX")].toDouble() + dx);
            shape.insert(QStringLiteral("toX"), shape[QStringLiteral("toX")].toDouble() + dx);
            shape.insert(QStringLiteral("fromY"), shape[QStringLiteral("fromY")].toDouble() + dy);
            shape.insert(QStringLiteral("toY"), shape[QStringLiteral("toY")].toDouble() + dy);
        } else if (type == QStringLiteral("freehand")) {
            QVariantList startPoints = shape[QStringLiteral("points")].toList();
            QVariantList newPoints;
            for (const QVariant &pv : startPoints) {
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

        m_shapesModel.updateShape(index, shape);
    }
    m_shapesModel.endEdit();
}


