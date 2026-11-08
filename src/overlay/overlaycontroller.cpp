#include "overlaycontroller.h"
#include <QtMath>
#include <cmath>
#include <QDebug>
#include <QSettings>
#include <QStringList>

static const QStringList kPresetColors = {
    QStringLiteral("#e63946"),
    QStringLiteral("#f4a261"),
    QStringLiteral("#e9c46a"),
    QStringLiteral("#2a9d8f"),
    QStringLiteral("#457b9d"),
    QStringLiteral("#8338ec")
};
#include <LayerShellQt/Window>
#include <QScreen>
#include <QGuiApplication>
#include <QUuid>
#include "excalidraw.h"
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QColor>
#include <QCursor>
#include <QPointF>
#include <QLineF>
#include <limits>
#include <numbers>
#include <utility>

OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
    , m_activeTool(QStringLiteral("freehand"))
    , m_defaultColor(QStringLiteral("#e63946"))
    , m_defaultFillColor(QStringLiteral("#e63946"))
{
    m_defaultFontFamily = QStringLiteral("monospace");
    m_shapesModel.setParent(this);

    // Hotkeys are read here only; there is no UI for rebinding them, but hand-edited
    // entries in ~/.config/scribbleway/shortcuts.conf still apply.
    static constexpr std::pair<const char *, const char *> kLocalShortcuts[] = {
        {"tool_arrow", "A"}, {"tool_rectangle", "R"}, {"tool_freehand", "F"},
        {"tool_ellipse", "E"}, {"tool_line", "L"}, {"tool_text", "T"},
        {"color_cycle", "Q"}, {"color_1", "1"}, {"color_2", "2"}, {"color_3", "3"},
        {"color_4", "4"}, {"color_5", "5"}, {"color_6", "6"},
        {"action_grow", "+"}, {"action_shrink", "-"}, {"action_select", "X"},
        {"action_cycle_roughness", "S"}, {"action_undo", "Ctrl+Z"},
        {"action_redo", "Ctrl+Shift+Z"}, {"action_clear", "Ctrl+Delete"},
        {"action_increase_border_radius", "ü"}, {"action_decrease_border_radius", "ä"},
    };
    QSettings settings(QStringLiteral("scribbleway"), QStringLiteral("shortcuts"));
    for (const auto &[id, sequence] : kLocalShortcuts) {
        m_localShortcutSequences.insert(QLatin1String(id),
            settings.value(QStringLiteral("local/") + QLatin1String(id), QString::fromUtf8(sequence)).toString());
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
        // Remembered so remapWindow() can return here: Qt's primary screen changes when
        // screens come and go, so it can't be relied on to mean the same one afterwards.
        if (m_window && m_targetScreen.isEmpty()) {
            m_targetScreen = m_window->screen()->name();
            Q_EMIT targetScreenChanged();
        }
    }
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
        // The one place that proves a local hotkey actually reached QML — the tool
        // shortcuts are the first thing to go silent when focus is lost.
        qInfo() << "active tool:" << (tool.isEmpty() ? QStringLiteral("(none)") : tool);
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
    roughness = qBound(0, roughness, 2);
    if (m_defaultRoughness != roughness) {
        m_defaultRoughness = roughness;
        Q_EMIT defaultRoughnessChanged();
    }
}

int OverlayController::defaultGlow() const
{
    return m_defaultGlow;
}

void OverlayController::setDefaultGlow(int glow)
{
    if (m_defaultGlow != glow) {
        m_defaultGlow = glow;
        Q_EMIT defaultGlowChanged();
    }
}

QString OverlayController::defaultFillColor() const
{
    return m_defaultFillColor;
}

void OverlayController::setDefaultFillColor(const QString &color)
{
    if (m_defaultFillColor != color) {
        m_defaultFillColor = color;
        Q_EMIT defaultFillColorChanged();
    }
}

double OverlayController::defaultFillOpacity() const
{
    return m_defaultFillOpacity;
}

void OverlayController::setDefaultFillOpacity(double opacity)
{
    const double clamped = qBound(0.0, opacity, 1.0);
    if (!qFuzzyCompare(m_defaultFillOpacity, clamped)) {
        m_defaultFillOpacity = clamped;
        Q_EMIT defaultFillOpacityChanged();
    }
}

int OverlayController::defaultFreehandSmoothing() const
{
    return m_defaultFreehandSmoothing;
}

void OverlayController::setDefaultFreehandSmoothing(int level)
{
    int clamped = qBound(0, level, 3);
    if (m_defaultFreehandSmoothing != clamped) {
        m_defaultFreehandSmoothing = clamped;
        Q_EMIT defaultFreehandSmoothingChanged();
    }
}

void OverlayController::addShape(const QVariantMap &shape)
{
    m_shapesModel.beginEdit();
    m_shapesModel.addShape(shape);
    setSelectedIndex(m_shapesModel.rowCount() - 1);
    m_shapesModel.endEdit();
}

int OverlayController::indexForId(const QString &id) const
{
    if (id.isEmpty()) return -1;
    const auto &shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (shapes[i].value(QStringLiteral("id")).toString() == id) {
            return i;
        }
    }
    return -1;
}

QVariantMap OverlayController::getShape(int index) const
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        return m_shapesModel.shapes().at(index);
    }
    return QVariantMap();
}


QVariantMap OverlayController::makeAttachedTextBinding(const QVariantMap &shape) const
{
    return {
        {QStringLiteral("type"), QStringLiteral("attachedText")},
        {QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("text"), QString()},
        {QStringLiteral("fontFamily"), shape.value(QStringLiteral("fontFamily"), m_defaultFontFamily)},
        {QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), m_defaultFontSize)},
        {QStringLiteral("textAlign"), QStringLiteral("center")},
        {QStringLiteral("verticalAlign"), QStringLiteral("middle")}
    };
}

QVariantMap OverlayController::attachedTextForShape(int index) const
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return QVariantMap();
    }
    return ShapesModel::firstAttachedTextBinding(m_shapesModel.shapes().at(index));
}

QVariantMap OverlayController::ensureAttachedTextForShape(int index)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return QVariantMap();
    }

    QVariantMap shape = m_shapesModel.shapes().at(index);
    const QString type = shape.value(QStringLiteral("type")).toString();
    if (!ShapesModel::supportsAttachedText(type)) {
        return QVariantMap();
    }

    QVariantMap existing = ShapesModel::firstAttachedTextBinding(shape);
    if (!existing.isEmpty()) {
        return existing;
    }

    QVariantMap binding = makeAttachedTextBinding(shape);
    QVariantList bindings = shape.value(QStringLiteral("bindings")).toList();
    bindings.append(binding);
    
    QVariantMap updates;
    updates.insert(QStringLiteral("bindings"), bindings);
    if (!shape.contains(QStringLiteral("fontFamily"))) {
        updates.insert(QStringLiteral("fontFamily"), binding.value(QStringLiteral("fontFamily")));
    }
    if (!shape.contains(QStringLiteral("fontSize"))) {
        updates.insert(QStringLiteral("fontSize"), binding.value(QStringLiteral("fontSize")));
    }
    
    m_shapesModel.updateShape(index, updates);
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
    return binding;
}

void OverlayController::setAttachedText(int index, const QString &text)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return;
    }

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        removeAttachedText(index);
        return;
    }

    QVariantMap shape = m_shapesModel.shapes().at(index);
    if (!ShapesModel::supportsAttachedText(shape.value(QStringLiteral("type")).toString())) {
        return;
    }

    QVariantList bindings = ShapesModel::withoutAttachedTextBinding(shape);
    QVariantMap binding = ShapesModel::firstAttachedTextBinding(shape);
    if (binding.isEmpty()) {
        binding = makeAttachedTextBinding(shape);
    }
    const QString fontFamily = shape.value(QStringLiteral("fontFamily"), m_defaultFontFamily).toString();
    const int fontSize = shape.value(QStringLiteral("fontSize"), m_defaultFontSize).toInt();
    binding.insert(QStringLiteral("text"), trimmed);
    binding.insert(QStringLiteral("fontFamily"), fontFamily);
    binding.insert(QStringLiteral("fontSize"), fontSize);
    bindings.append(binding);

    QVariantMap updates;
    updates.insert(QStringLiteral("bindings"), bindings);
    if (!shape.contains(QStringLiteral("fontFamily"))) {
        updates.insert(QStringLiteral("fontFamily"), fontFamily);
    }
    if (!shape.contains(QStringLiteral("fontSize"))) {
        updates.insert(QStringLiteral("fontSize"), fontSize);
    }

    m_shapesModel.updateShape(index, updates);
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
}

void OverlayController::removeAttachedText(int index)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return;
    }

    const QVariantMap shape = m_shapesModel.shapes().at(index);
    QVariantList bindings = ShapesModel::withoutAttachedTextBinding(shape);
    if (bindings == shape.value(QStringLiteral("bindings")).toList()) {
        return;
    }

    m_shapesModel.updateShape(index, {{QStringLiteral("bindings"), bindings}});
    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }
}

void OverlayController::updateShape(int index, const QVariantMap &properties)
{
    m_shapesModel.updateShape(index, properties);

    if (index == m_selectedIndex) {
        notifySelectionChanged();
    }

    bool geometryChanged = properties.contains(QStringLiteral("x")) ||
                           properties.contains(QStringLiteral("y")) ||
                           properties.contains(QStringLiteral("width")) ||
                           properties.contains(QStringLiteral("height"));
    if (geometryChanged && index >= 0 && index < m_shapesModel.rowCount()) {
        const QVariantMap shape = m_shapesModel.shapes().at(index);
        if (!shape.value(QStringLiteral("boundElementIds")).toList().isEmpty()) {
            updateBoundEndpoints(index);
        }
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
            if (shape.contains(QStringLiteral("glow"))) {
                m_defaultGlow = shape[QStringLiteral("glow")].toInt();
            }
            if (shape.contains(QStringLiteral("fillColor"))) {
                m_defaultFillColor = shape[QStringLiteral("fillColor")].toString();
            }
            if (shape.contains(QStringLiteral("fillOpacity"))) {
                m_defaultFillOpacity = shape[QStringLiteral("fillOpacity")].toDouble();
            }
            Q_EMIT defaultColorChanged();
            Q_EMIT defaultStrokeWidthChanged();
            Q_EMIT defaultOpacityChanged();
            Q_EMIT defaultFontFamilyChanged();
            Q_EMIT defaultFontSizeChanged();
            Q_EMIT defaultBorderRadiusChanged();
            Q_EMIT defaultRoughnessChanged();
            Q_EMIT defaultGlowChanged();
            Q_EMIT defaultFillColorChanged();
            Q_EMIT defaultFillOpacityChanged();

            ensureSelectMode();
        }
        m_shapesModel.endEdit();
        
        notifySelectionChanged();
    }
}

void OverlayController::updateInputMask(const QVariantList &rects)
{
    QRegion region;
    for (const QVariant &v : rects) {
        QRect r = v.toRectF().toRect();
        if (r.isValid()) {
            region += r;
        }
    }
    m_baseInputMask = region;
    applyInputMask();
}

void OverlayController::setPopupExclusion(const QRect &rect)
{
    if (m_popupExclusion == rect) return;
    m_popupExclusion = rect;
    applyInputMask();
}

void OverlayController::applyInputMask()
{
    if (!m_window) return;

    QRegion region = m_baseInputMask;
    if (m_popupExclusion.isValid()) {
        region -= m_popupExclusion;
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

QVariantMap OverlayController::getSelectionState() const
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
        state[QStringLiteral("glow")] = shape.value(QStringLiteral("glow"), m_defaultGlow).toInt();
        state[QStringLiteral("fillColor")] = shape.value(QStringLiteral("fillColor"), m_defaultFillColor).toString();
        state[QStringLiteral("fillOpacity")] = shape.value(QStringLiteral("fillOpacity"), m_defaultFillOpacity).toDouble();
        state[QStringLiteral("freehandSmoothing")] = m_defaultFreehandSmoothing;
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
        state[QStringLiteral("glow")] = m_defaultGlow;
        state[QStringLiteral("fillColor")] = m_defaultFillColor;
        state[QStringLiteral("fillOpacity")] = m_defaultFillOpacity;
        state[QStringLiteral("freehandSmoothing")] = m_defaultFreehandSmoothing;
        state[QStringLiteral("locked")] = false;
        state[QStringLiteral("selectedIndex")] = -1;
    }
    return state;
}

bool OverlayController::hasSelection() const
{
    return m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount();
}


QStringList OverlayController::presetColors()
{
    return kPresetColors;
}

QStringList OverlayController::screenNames() const
{
    QStringList names;
    for (QScreen *screen : QGuiApplication::screens()) {
        names << screen->name();
    }
    return names;
}

QString OverlayController::targetScreen() const
{
    return m_targetScreen;
}



void OverlayController::updateProperties(const QVariantMap &properties)
{
    QVariantMap demarshalled = properties;

    if (demarshalled.contains(QStringLiteral("roughness"))) {
        demarshalled[QStringLiteral("roughness")] = qBound(0, demarshalled[QStringLiteral("roughness")].toInt(), 2);
    }
    // App-level default only: shapes store the smoothed points, not the setting.
    const QVariant smoothing = demarshalled.take(QStringLiteral("freehandSmoothing"));
    if (smoothing.isValid()) {
        setDefaultFreehandSmoothing(smoothing.toInt());
    }

    // Every shape key has a matching `default<Key>` Q_PROPERTY, so route through the
    // metaobject instead of writing one near-identical branch per property -- the branch
    // someone forgets to add is exactly how defaults silently stop sticking. Keys with no
    // such property (bindings, text, geometry) simply don't match and fall through.
    // testDefaultsReachableFromQml() pins that these properties stay writable.
    for (auto it = demarshalled.cbegin(); it != demarshalled.cend(); ++it) {
        const QString name = QStringLiteral("default") + it.key().at(0).toUpper() + it.key().mid(1);
        setProperty(name.toUtf8().constData(), it.value());
    }

    const bool hasFontUpdate = demarshalled.contains(QStringLiteral("fontFamily")) || demarshalled.contains(QStringLiteral("fontSize"));
    // A container's attached text carries its own font, so a font change has to reach
    // the binding as well as the shape.
    auto applyTo = [this, &demarshalled, hasFontUpdate](int index) {
        QVariantMap shapeProps = demarshalled;
        const QVariantMap shape = m_shapesModel.shapes()[index];
        const QVariantMap attached = ShapesModel::firstAttachedTextBinding(shape);
        if (!attached.isEmpty() && hasFontUpdate) {
            QVariantList bindings = ShapesModel::withoutAttachedTextBinding(shape);
            QVariantMap updatedAttached = attached;
            for (const QString &key : {QStringLiteral("fontFamily"), QStringLiteral("fontSize")}) {
                if (demarshalled.contains(key)) {
                    updatedAttached.insert(key, demarshalled[key]);
                }
            }
            bindings.append(updatedAttached);
            shapeProps.insert(QStringLiteral("bindings"), bindings);
        }
        m_shapesModel.updateShape(index, shapeProps);
    };

    m_shapesModel.beginEdit();
    bool updatedAny = false;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            applyTo(i);
            updatedAny = true;
        }
    }
    if (!updatedAny && m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        applyTo(m_selectedIndex);
    }
    m_shapesModel.endEdit();

    notifySelectionChanged();
}

void OverlayController::undo()
{
    m_shapesModel.undo();
    setSelectedIndex(-1);
    notifySelectionChanged();
}

void OverlayController::redo()
{
    m_shapesModel.redo();
    setSelectedIndex(-1);
    notifySelectionChanged();
}

void OverlayController::clear()
{
    m_shapesModel.clear();
    m_selectedIndex = -1;
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
    m_wantsKeyboard = interactive;
    applyKeyboardInteractivity();
}

bool OverlayController::trayPopupOpen() const
{
    return m_trayPopupOpen;
}

void OverlayController::setTrayPopupOpen(bool open)
{
    if (m_trayPopupOpen == open) return;
    m_trayPopupOpen = open;
    if (!open) setPopupWantsKeyboard(false);
    Q_EMIT trayPopupOpenChanged();
}

bool OverlayController::popupWantsKeyboard() const
{
    return m_popupWantsKeyboard;
}

// Only the popup's text fields take the keyboard away from the overlay, and only while
// they are focused. Yielding it for the whole time the popup is open killed every tool
// hotkey the moment the popup was touched — the shortcuts live on the overlay window,
// so an overlay with no keyboard is an overlay with no hotkeys.
void OverlayController::setPopupWantsKeyboard(bool wants)
{
    if (m_popupWantsKeyboard == wants) return;
    m_popupWantsKeyboard = wants;
    applyKeyboardInteractivity();
    Q_EMIT popupWantsKeyboardChanged();
}

void OverlayController::closeTrayPopup()
{
    Q_EMIT trayPopupCloseRequested();
}

void OverlayController::applyKeyboardInteractivity()
{
    if (!m_window) return;
    auto *layerWindow = LayerShellQt::Window::get(m_window);
    if (!layerWindow) return;

    const bool interactive = m_wantsKeyboard && !m_popupWantsKeyboard;

    // Exclusive, not OnDemand: OnDemand means "focus me when the user clicks me",
    // and a layer surface has no way to ask for focus itself — QWindow::requestActivate()
    // is a no-op here. KWin happened to hand focus over anyway; Hyprland does not, so
    // every QML Shortcut in main.qml was dead there. Exclusive is what an annotation
    // overlay wants while a tool is active, and it works on both.
    const auto wanted = interactive
        ? LayerShellQt::Window::KeyboardInteractivityExclusive
        : LayerShellQt::Window::KeyboardInteractivityNone;

    // The QML side calls this from requestInputRegionUpdate(), which runs on every
    // draw event — several times a second while drawing. Only act on real
    // transitions, so this neither spams the journal nor re-commits the layer
    // surface for a value it already has.
    if (layerWindow->keyboardInteractivity() == wanted) return;

    // Logged because "my hotkeys don't work" is otherwise undebuggable from outside:
    // neither hyprctl nor kwin exposes layer-shell keyboard focus.
    qInfo() << "keyboard interactivity:" << (interactive ? "exclusive" : "none");
    layerWindow->setKeyboardInteractivity(wanted);
}

static QScreen *screenNamed(const QString &name)
{
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == name) return screen;
    }
    return nullptr;
}

// The compositor closes a layer surface when its output goes away, and nothing maps it
// again by itself, so the hotkeys would keep driving a window nobody can see. A fresh
// surface also puts the overlay back on its own screen once that screen returns.
void OverlayController::remapWindow()
{
    Q_EMIT screensChanged();
    if (!m_window) return;

    QScreen *target = screenNamed(m_targetScreen);
    m_window->hide();
    // Without destroy() the native window survives and keeps its old output.
    m_window->destroy();
    m_window->setScreen(target ? target : QGuiApplication::primaryScreen());
    m_window->show();
    qInfo() << "overlay remapped to" << m_window->screen()->name();
}

void OverlayController::setTargetScreen(const QString &screenName)
{
    if (!m_window || screenName.isEmpty()) return;

    QScreen *target = screenNamed(screenName);

    if (target && m_window->screen() != target) {
        m_window->hide();
        m_window->setScreen(target);
        m_window->show();
    }

    if (m_targetScreen != screenName) {
        m_targetScreen = screenName;
        Q_EMIT targetScreenChanged();
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

void OverlayController::raiseSelected()
{
    if (m_selectedIndex >= 0 && m_selectedIndex < m_shapesModel.rowCount() - 1) {
        if (m_shapesModel.moveShape(m_selectedIndex, m_selectedIndex + 1)) {
            m_selectedIndex++;
            notifySelectionChanged();
        }
    }
}

void OverlayController::lowerSelected()
{
    if (m_selectedIndex > 0 && m_selectedIndex < m_shapesModel.rowCount()) {
        if (m_shapesModel.moveShape(m_selectedIndex, m_selectedIndex - 1)) {
            m_selectedIndex--;
            notifySelectionChanged();
        }
    }
}

void OverlayController::setShapeLocked(int index, bool locked)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        m_shapesModel.beginEdit();
        m_shapesModel.updateShape(index, {{QStringLiteral("locked"), locked}});
        // A locked shape must not stay in a selection, or group moves would still reach it.
        if (locked && index == m_selectedIndex) {
            setSelectedIndex(-1);
        } else if (locked) {
            m_shapesModel.updateShape(index, {{QStringLiteral("selected"), false}});
        }
        m_shapesModel.endEdit();
    }
}

void OverlayController::deleteShape(int index)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        cleanupBindingsForDelete(index);
        m_shapesModel.removeShape(index);
        if (m_selectedIndex == index) {
            m_selectedIndex = -1;
            notifySelectionChanged();
        } else if (m_selectedIndex > index) {
            m_selectedIndex--;
            notifySelectionChanged();
        }
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

    if (m_shapesModel.shapes()[index].value(QStringLiteral("locked"), false).toBool()) {
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
            notifySelectionChanged();
        }
    }
}

void OverlayController::notifySelectionChanged()
{
    Q_EMIT selectionChanged();
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

QVariantMap OverlayController::localShortcutSequences() const
{
    return m_localShortcutSequences;
}

void OverlayController::selectPresetColor(int index)
{
    if (index >= 0 && index < kPresetColors.size()) {
        updateProperties({{QStringLiteral("color"), kPresetColors[index]}});
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
    int idx = kPresetColors.indexOf(currentColor);
    int nextIdx = (idx + 1) % kPresetColors.size();
    updateProperties({{QStringLiteral("color"), kPresetColors[nextIdx]}});
}

void OverlayController::cycleRoughness()
{
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

bool OverlayController::selectedTypeIs(const QString &type) const
{
    return getSelectionState().value(QStringLiteral("type")).toString().compare(type, Qt::CaseInsensitive) == 0;
}

void OverlayController::bumpSelected(const QString &key, int delta, int lo, int hi)
{
    const QVariantMap state = getSelectionState();
    if (!state.value(QStringLiteral("hasSelection")).toBool()) return;
    updateProperties({{key, qBound(lo, state.value(key).toInt() + delta, hi)}});
}

void OverlayController::growSelected()
{
    if (selectedTypeIs(QStringLiteral("text"))) {
        bumpSelected(QStringLiteral("fontSize"), 2, 10);
    } else {
        bumpSelected(QStringLiteral("strokeWidth"), 1, 1, 15);
    }
}

void OverlayController::shrinkSelected()
{
    if (selectedTypeIs(QStringLiteral("text"))) {
        bumpSelected(QStringLiteral("fontSize"), -2, 10);
    } else {
        bumpSelected(QStringLiteral("strokeWidth"), -1, 1);
    }
}

void OverlayController::increaseBorderRadius()
{
    if (selectedTypeIs(QStringLiteral("rectangle"))) {
        bumpSelected(QStringLiteral("borderRadius"), 2, 0);
    }
}

void OverlayController::decreaseBorderRadius()
{
    if (selectedTypeIs(QStringLiteral("rectangle"))) {
        bumpSelected(QStringLiteral("borderRadius"), -2, 0);
    }
}

void OverlayController::copySelected()
{
    QJsonArray elements;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (m_shapesModel.shapes()[i].value(QStringLiteral("selected")).toBool()) {
            QVariantMap shape = m_shapesModel.shapes().at(i);
            QJsonObject elemObj = Excalidraw::elementFromShape(shape);
            if (!elemObj.isEmpty()) {
                elements.append(elemObj);
                const QVariantMap attached = ShapesModel::firstAttachedTextBinding(shape);
                if (!attached.isEmpty()) {
                    const QJsonObject textObj = Excalidraw::attachedTextElement(shape, attached, m_defaultFontSize);
                    if (!textObj.isEmpty()) {
                        elements.append(textObj);
                    }
                }
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
    // ponytail: only support excalidraw clipboard format, add fallback if other formats are needed
    if (jsonObj.value(QStringLiteral("type")).toString() != QStringLiteral("excalidraw/clipboard")) return;

    QList<QVariantMap> list;
    const QJsonArray elements = jsonObj.value(QStringLiteral("elements")).toArray();
    for (const QJsonValue &val : elements) {
        const QVariantMap shape = Excalidraw::shapeFromElement(val.toObject());
        if (!shape.isEmpty()) {
            list.append(shape);
        }
    }
    if (list.isEmpty()) return;

    Excalidraw::attachBoundText(list, m_defaultFontFamily, m_defaultFontSize);

    // Pre-pass: generate new unique IDs for pasted elements and map them
    QHash<QString, QString> idMap;
    for (QVariantMap &shape : list) {
        const QString oldId = shape.value(QStringLiteral("id")).toString();
        const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!oldId.isEmpty()) {
            idMap.insert(oldId, newId);
        }
        shape.insert(QStringLiteral("id"), newId);
    }

    // Excalidraw's focus is a -1..1 offset, not our fraction of the outline, so it is
    // recomputed from where the endpoint actually sits.
    auto remapBinding = [this, &idMap, &list](QVariantMap &shape, const QString &key, const QPointF &endpoint) {
        QVariantMap binding = shape.value(key).toMap();
        if (binding.isEmpty()) return;
        const QString newId = idMap.value(binding.value(QStringLiteral("elementId")).toString());
        if (newId.isEmpty()) {
            shape.remove(key);
            return;
        }
        binding[QStringLiteral("elementId")] = newId;
        for (const QVariantMap &target : list) {
            if (target.value(QStringLiteral("id")).toString() == newId) {
                binding[QStringLiteral("focus")] = focusForPoint(target, endpoint.x(), endpoint.y());
                break;
            }
        }
        shape[key] = binding;
    };

    // Second pass: update bindings within the pasted set
    for (QVariantMap &shape : list) {
        const QString type = shape.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
            remapBinding(shape, QStringLiteral("startBinding"),
                         QPointF(shape.value(QStringLiteral("fromX")).toDouble(), shape.value(QStringLiteral("fromY")).toDouble()));
            remapBinding(shape, QStringLiteral("endBinding"),
                         QPointF(shape.value(QStringLiteral("toX")).toDouble(), shape.value(QStringLiteral("toY")).toDouble()));
        } else if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            QVariantList newBoundIds;
            const QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
            for (const QVariant &bidVar : boundIds) {
                if (idMap.contains(bidVar.toString())) {
                    newBoundIds.append(idMap.value(bidVar.toString()));
                }
            }
            if (!newBoundIds.isEmpty()) {
                shape[QStringLiteral("boundElementIds")] = newBoundIds;
            } else {
                shape.remove(QStringLiteral("boundElementIds"));
            }
        }
    }

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

    for (const QVariantMap &shape : list) {
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
            const QVariantList points = shape[QStringLiteral("points")].toList();
            for (const QVariant &pv : points) {
                const double px = pv.toPointF().x();
                const double py = pv.toPointF().y();
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
    for (QVariantMap shape : list) {
        shape.insert(QStringLiteral("selected"), true);
        shape.insert(QStringLiteral("locked"), false);
        translateShape(shape, dx, dy);

        m_shapesModel.addShape(shape);
        pastedIndices.append(m_shapesModel.rowCount() - 1);
    }

    if (!pastedIndices.isEmpty()) {
        m_selectedIndex = pastedIndices.last();
    }

    m_shapesModel.endEdit();
    notifySelectionChanged();
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

void OverlayController::selectShapesInRect(double rx, double ry, double rw, double rh, bool /*shiftHeld*/)
{
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
                const QPointF p = pv.toPointF();
                if (selectRect.contains(p)) {
                    intersects = true;
                    break;
                }
            }
        }

        // A locked shape never joins a selection, so group moves and deletes cannot reach it.
        bool shouldSelect = !shape.value(QStringLiteral("locked"), false).toBool()
            && (intersects || m_preDragSelection.contains(i));

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

    notifySelectionChanged();
}

void OverlayController::translateShape(QVariantMap &shape, double dx, double dy)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    auto shift = [&shape, dx, dy](QLatin1String xKey, QLatin1String yKey) {
        shape[xKey] = shape.value(xKey).toDouble() + dx;
        shape[yKey] = shape.value(yKey).toDouble() + dy;
    };

    if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse") || type == QStringLiteral("text")) {
        shift(QLatin1String("x"), QLatin1String("y"));
    } else if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        shift(QLatin1String("fromX"), QLatin1String("fromY"));
        shift(QLatin1String("toX"), QLatin1String("toY"));
    } else if (type == QStringLiteral("freehand")) {
        QVariantList newPoints;
        const QVariantList points = shape.value(QStringLiteral("points")).toList();
        for (const QVariant &pv : points) {
            newPoints.append(pv.toPointF() + QPointF(dx, dy));
        }
        shape.insert(QStringLiteral("points"), newPoints);
    }
}

void OverlayController::detachFromUnmoved(int index, QVariantMap &shape, const QSet<int> &moved)
{
    for (bool isStart : {true, false}) {
        const QString key = isStart ? QStringLiteral("startBinding") : QStringLiteral("endBinding");
        const QString target = shape.value(key).toMap().value(QStringLiteral("elementId")).toString();
        if (!target.isEmpty() && !moved.contains(indexForId(target))) {
            breakEndpointBinding(index, isStart);
            shape.remove(key);
        }
    }
}

void OverlayController::dragSelected(double dx, double dy)
{
    if (m_dragStartShapes.isEmpty()) {
        return;
    }

    m_shapesModel.beginEdit();
    const QSet<int> draggedIndices(m_dragStartShapes.keyBegin(), m_dragStartShapes.keyEnd());
    for (auto it = m_dragStartShapes.begin(); it != m_dragStartShapes.end(); ++it) {
        detachFromUnmoved(it.key(), it.value(), draggedIndices);
        QVariantMap shape = it.value();
        translateShape(shape, dx, dy);
        m_shapesModel.updateShape(it.key(), shape);
    }

    for (auto it = m_dragStartShapes.begin(); it != m_dragStartShapes.end(); ++it) {
        QString type = m_shapesModel.shapes()[it.key()][QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            updateBoundEndpoints(it.key(), &draggedIndices);
        }
    }

    m_shapesModel.endEdit();
}

void OverlayController::nudgeSelected(double dx, double dy)
{
    QSet<int> nudgedIndices;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        const QVariantMap &shape = m_shapesModel.shapes()[i];
        if (shape.value(QStringLiteral("selected"), false).toBool()
            && !shape.value(QStringLiteral("locked"), false).toBool()) {
            nudgedIndices.insert(i);
        }
    }

    m_shapesModel.beginEdit();
    for (int idx : nudgedIndices) {
        QVariantMap shape = m_shapesModel.shapes()[idx];
        detachFromUnmoved(idx, shape, nudgedIndices);
        translateShape(shape, dx, dy);
        m_shapesModel.updateShape(idx, shape);
    }
    for (int idx : nudgedIndices) {
        QString type = m_shapesModel.shapes()[idx][QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            updateBoundEndpoints(idx, &nudgedIndices);
        }
    }
    m_shapesModel.endEdit();
}



QPointF OverlayController::nearestPointOnRect(double rx, double ry, double rw, double rh, double px, double py) const
{
    // Nearest point on each edge, then keep the closest of the four.
    QPointF top(qBound(rx, px, rx + rw), ry);
    QPointF bottom(qBound(rx, px, rx + rw), ry + rh);
    QPointF left(rx, qBound(ry, py, ry + rh));
    QPointF right(rx + rw, qBound(ry, py, ry + rh));

    auto dist2 = [&](const QPointF &a) {
        double dx = a.x() - px, dy = a.y() - py;
        return dx * dx + dy * dy;
    };

    QPointF candidates[] = { top, bottom, left, right };
    double minDist = dist2(top);
    QPointF best = top;

    for (int i = 1; i < 4; ++i) {
        double d = dist2(candidates[i]);
        if (d < minDist) {
            minDist = d;
            best = candidates[i];
        }
    }
    return best;
}

QPointF OverlayController::nearestPointOnEllipse(double cx, double cy, double a, double b, double px, double py) const
{
    double lx = px - cx;
    double ly = py - cy;

    if (qFuzzyIsNull(lx) && qFuzzyIsNull(ly)) {
        return QPointF(cx, cy - b);
    }

    double theta = qAtan2(ly / b, lx / a);
    for (int i = 0; i < 4; ++i) {
        double ct = qCos(theta), st = qSin(theta);
        double ex = a * ct, ey = b * st;
        double dx = lx - ex, dy = ly - ey;
        double dtx = -a * st, dty = b * ct;
        double denom = dtx * dtx + dty * dty;
        if (qFuzzyIsNull(denom)) break;
        theta += (dx * dtx + dy * dty) / denom;
    }

    return QPointF(cx + a * qCos(theta), cy + b * qSin(theta));
}

double OverlayController::focusForPoint(const QVariantMap &shape, double px, double py) const
{
    QString type = shape.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("rectangle")) {
        double rx = shape[QStringLiteral("x")].toDouble();
        double ry = shape[QStringLiteral("y")].toDouble();
        double rw = shape[QStringLiteral("width")].toDouble();
        double rh = shape[QStringLiteral("height")].toDouble();
        QPointF sp = nearestPointOnRect(rx, ry, rw, rh, px, py);

        double perim = 2.0 * (rw + rh);
        if (perim < 1e-6) return 0.0;

        double topCenter = rx + rw / 2.0;
        double d = 0.0;

        if (qFuzzyCompare(sp.y(), ry)) {
            d = sp.x() - topCenter;
            if (d < 0) d += perim;
        } else if (qFuzzyCompare(sp.x(), rx + rw)) {
            d = rw / 2.0 + (sp.y() - ry);
        } else if (qFuzzyCompare(sp.y(), ry + rh)) {
            d = rw / 2.0 + rh + (rx + rw - sp.x());
        } else {
            d = rw / 2.0 + rh + rw + (ry + rh - sp.y());
        }

        return d / perim;
    } else if (type == QStringLiteral("ellipse")) {
        double cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
        double cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;

        const double a = shape[QStringLiteral("width")].toDouble() / 2.0;
        const double b = shape[QStringLiteral("height")].toDouble() / 2.0;
        if (a <= 0.0 || b <= 0.0) return 0.0;

        // The parametric angle, the same one pointFromBinding() turns back into a point.
        double angle = qAtan2((py - cy) / b, (px - cx) / a);
        if (angle < 0) angle += 2.0 * std::numbers::pi;
        return angle / (2.0 * std::numbers::pi);
    }

    return 0.0;
}

QPointF OverlayController::pointFromBinding(const QVariantMap &shape, double focus) const
{
    QString type = shape.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("rectangle")) {
        double rx = shape[QStringLiteral("x")].toDouble();
        double ry = shape[QStringLiteral("y")].toDouble();
        double rw = shape[QStringLiteral("width")].toDouble();
        double rh = shape[QStringLiteral("height")].toDouble();
        double perim = 2.0 * (rw + rh);
        double d = focus * perim;
        double topCenter = rx + rw / 2.0;

        if (d <= rw / 2.0) {
            return QPointF(topCenter + d, ry);
        }
        d -= rw / 2.0;
        if (d <= rh) {
            return QPointF(rx + rw, ry + d);
        }
        d -= rh;
        if (d <= rw) {
            return QPointF(rx + rw - d, ry + rh);
        }
        d -= rw;
        if (d <= rh) {
            return QPointF(rx, ry + rh - d);
        }
        d -= rh;
        return QPointF(rx + d, ry);
    } else if (type == QStringLiteral("ellipse")) {
        double cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
        double cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;
        double a = shape[QStringLiteral("width")].toDouble() / 2.0;
        double b = shape[QStringLiteral("height")].toDouble() / 2.0;

        double angle = focus * 2.0 * std::numbers::pi;
        return QPointF(cx + a * qCos(angle), cy + b * qSin(angle));
    }

    return QPointF();
}

BindingHit OverlayController::findSnapTarget(double px, double py, int excludeIndex) const
{
    BindingHit best;
    double bestDist = kSnapThreshold + 1.0;

    const auto &shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (i == excludeIndex) continue;

        const auto &shape = shapes[i];
        QString type = shape.value(QStringLiteral("type")).toString();
        if (type != QStringLiteral("rectangle") && type != QStringLiteral("ellipse"))
            continue;
        if (shape.value(QStringLiteral("locked"), false).toBool())
            continue;

        QPointF snapPt;
        if (type == QStringLiteral("rectangle")) {
            double rx = shape[QStringLiteral("x")].toDouble();
            double ry = shape[QStringLiteral("y")].toDouble();
            double rw = shape[QStringLiteral("width")].toDouble();
            double rh = shape[QStringLiteral("height")].toDouble();
            snapPt = nearestPointOnRect(rx, ry, rw, rh, px, py);
        } else {
            double cx = shape[QStringLiteral("x")].toDouble() + shape[QStringLiteral("width")].toDouble() / 2.0;
            double cy = shape[QStringLiteral("y")].toDouble() + shape[QStringLiteral("height")].toDouble() / 2.0;
            double a = shape[QStringLiteral("width")].toDouble() / 2.0;
            double b = shape[QStringLiteral("height")].toDouble() / 2.0;
            snapPt = nearestPointOnEllipse(cx, cy, a, b, px, py);
        }

        double dx = snapPt.x() - px, dy = snapPt.y() - py;
        double dist = qSqrt(dx * dx + dy * dy);
        if (dist <= kSnapThreshold && dist < bestDist) {
            bestDist = dist;
            best.targetId = shape.value(QStringLiteral("id")).toString();
            best.focus = focusForPoint(shape, snapPt.x(), snapPt.y());
            best.snapPoint = snapPt;
            best.valid = true;
        }
    }

    return best;
}

QVariantMap OverlayController::findSnapInfo(double px, double py, int excludeIndex) const
{
    QVariantMap result;
    BindingHit hit = findSnapTarget(px, py, excludeIndex);
    result[QStringLiteral("valid")] = hit.valid;
    if (hit.valid) {
        result[QStringLiteral("targetIndex")] = indexForId(hit.targetId);
        result[QStringLiteral("snapX")] = hit.snapPoint.x();
        result[QStringLiteral("snapY")] = hit.snapPoint.y();
    } else {
        result[QStringLiteral("targetIndex")] = -1;
        result[QStringLiteral("snapX")] = 0.0;
        result[QStringLiteral("snapY")] = 0.0;
    }
    return result;
}


void OverlayController::addBackReference(const QString &targetId, const QString &lineId)
{
    int targetIdx = indexForId(targetId);
    if (targetIdx < 0) return;
    QVariantMap shape = m_shapesModel.shapes()[targetIdx];
    QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
    if (!boundIds.contains(lineId)) {
        boundIds.append(lineId);
        m_shapesModel.updateShape(targetIdx, {{QStringLiteral("boundElementIds"), boundIds}});
    }
}

void OverlayController::removeBackReference(const QString &targetId, const QString &lineId)
{
    int targetIdx = indexForId(targetId);
    if (targetIdx < 0) return;
    QVariantMap shape = m_shapesModel.shapes()[targetIdx];
    QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
    boundIds.removeAll(lineId);
    m_shapesModel.updateShape(targetIdx, {{QStringLiteral("boundElementIds"), boundIds}});
}

void OverlayController::createBindingsForShape(int lineIndex)
{
    if (lineIndex < 0 || lineIndex >= m_shapesModel.rowCount()) return;
    const auto &shapes = m_shapesModel.shapes();
    const auto &lineShape = shapes[lineIndex];
    QString type = lineShape.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("line") && type != QStringLiteral("arrow")) return;

    QString lineId = lineShape.value(QStringLiteral("id")).toString();
    double fromX = lineShape[QStringLiteral("fromX")].toDouble();
    double fromY = lineShape[QStringLiteral("fromY")].toDouble();
    double toX = lineShape[QStringLiteral("toX")].toDouble();
    double toY = lineShape[QStringLiteral("toY")].toDouble();

    QVariantMap updates;

    // Check start endpoint
    BindingHit startHit = findSnapTarget(fromX, fromY, lineIndex);
    if (startHit.valid) {
        QVariantMap sb;
        sb[QStringLiteral("elementId")] = startHit.targetId;
        sb[QStringLiteral("focus")] = startHit.focus;
        sb[QStringLiteral("gap")] = 0.0;
        updates[QStringLiteral("startBinding")] = sb;
        updates[QStringLiteral("fromX")] = startHit.snapPoint.x();
        updates[QStringLiteral("fromY")] = startHit.snapPoint.y();
        addBackReference(startHit.targetId, lineId);
    }

    // Check end endpoint
    BindingHit endHit = findSnapTarget(toX, toY, lineIndex);
    if (endHit.valid) {
        QVariantMap eb;
        eb[QStringLiteral("elementId")] = endHit.targetId;
        eb[QStringLiteral("focus")] = endHit.focus;
        eb[QStringLiteral("gap")] = 0.0;
        updates[QStringLiteral("endBinding")] = eb;
        updates[QStringLiteral("toX")] = endHit.snapPoint.x();
        updates[QStringLiteral("toY")] = endHit.snapPoint.y();
        addBackReference(endHit.targetId, lineId);
    }

    if (!updates.isEmpty()) {
        m_shapesModel.updateShape(lineIndex, updates);
    }
}

void OverlayController::updateBoundEndpoints(int shapeIndex, const QSet<int> *skipIndices)
{
    if (shapeIndex < 0 || shapeIndex >= m_shapesModel.rowCount()) return;
    const QVariantMap targetShape = m_shapesModel.shapes().at(shapeIndex);
    QString targetId = targetShape.value(QStringLiteral("id")).toString();
    QVariantList boundIds = targetShape.value(QStringLiteral("boundElementIds")).toList();

    for (const QVariant &bidVar : boundIds) {
        QString lineId = bidVar.toString();
        int lineIdx = indexForId(lineId);
        if (lineIdx < 0) continue;
        if (skipIndices && skipIndices->contains(lineIdx)) continue;

        const QVariantMap lineShape = m_shapesModel.shapes().at(lineIdx);
        QVariantMap updates;

        QVariantMap sb = lineShape.value(QStringLiteral("startBinding")).toMap();
        if (sb.value(QStringLiteral("elementId")).toString() == targetId) {
            QPointF p = pointFromBinding(targetShape, sb.value(QStringLiteral("focus")).toDouble());
            updates[QStringLiteral("fromX")] = p.x();
            updates[QStringLiteral("fromY")] = p.y();
        }

        QVariantMap eb = lineShape.value(QStringLiteral("endBinding")).toMap();
        if (eb.value(QStringLiteral("elementId")).toString() == targetId) {
            QPointF p = pointFromBinding(targetShape, eb.value(QStringLiteral("focus")).toDouble());
            updates[QStringLiteral("toX")] = p.x();
            updates[QStringLiteral("toY")] = p.y();
        }

        if (!updates.isEmpty()) {
            m_shapesModel.updateShape(lineIdx, updates);
        }
    }
}

void OverlayController::breakEndpointBinding(int lineIndex, bool isStart)
{
    if (lineIndex < 0 || lineIndex >= m_shapesModel.rowCount()) return;
    const QVariantMap lineShape = m_shapesModel.shapes().at(lineIndex);
    QString lineId = lineShape.value(QStringLiteral("id")).toString();
    QString bindingKey = isStart ? QStringLiteral("startBinding") : QStringLiteral("endBinding");

    QVariantMap binding = lineShape.value(bindingKey).toMap();
    if (binding.isEmpty()) return;

    QString targetId = binding.value(QStringLiteral("elementId")).toString();
    removeBackReference(targetId, lineId);

    m_shapesModel.updateShape(lineIndex, {{bindingKey, QVariantMap()}});
}

void OverlayController::cleanupBindingsForDelete(int deletedIndex)
{
    if (deletedIndex < 0 || deletedIndex >= m_shapesModel.rowCount()) return;
    const QVariantMap shape = m_shapesModel.shapes().at(deletedIndex);
    QString deletedId = shape.value(QStringLiteral("id")).toString();
    QString type = shape.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        QVariantMap sb = shape.value(QStringLiteral("startBinding")).toMap();
        if (!sb.isEmpty()) {
            removeBackReference(sb.value(QStringLiteral("elementId")).toString(), deletedId);
        }
        QVariantMap eb = shape.value(QStringLiteral("endBinding")).toMap();
        if (!eb.isEmpty()) {
            removeBackReference(eb.value(QStringLiteral("elementId")).toString(), deletedId);
        }
    } else if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
        QVariantList boundIds = shape.value(QStringLiteral("boundElementIds")).toList();
        for (const QVariant &bidVar : boundIds) {
            int lineIdx = indexForId(bidVar.toString());
            if (lineIdx < 0) continue;
            const QVariantMap lineShape = m_shapesModel.shapes().at(lineIdx);
            QVariantMap updates;

            QVariantMap sb = lineShape.value(QStringLiteral("startBinding")).toMap();
            if (sb.value(QStringLiteral("elementId")).toString() == deletedId) {
                updates[QStringLiteral("startBinding")] = QVariantMap();
            }
            QVariantMap eb = lineShape.value(QStringLiteral("endBinding")).toMap();
            if (eb.value(QStringLiteral("elementId")).toString() == deletedId) {
                updates[QStringLiteral("endBinding")] = QVariantMap();
            }
            if (!updates.isEmpty()) {
                m_shapesModel.updateShape(lineIdx, updates);
            }
        }
    }
}

void OverlayController::resnapEndpoint(int lineIndex, bool isStart)
{
    if (lineIndex < 0 || lineIndex >= m_shapesModel.rowCount()) return;
    const QVariantMap lineShape = m_shapesModel.shapes().at(lineIndex);
    QString type = lineShape.value(QStringLiteral("type")).toString();
    if (type != QStringLiteral("line") && type != QStringLiteral("arrow")) return;

    double px = isStart ? lineShape[QStringLiteral("fromX")].toDouble()
                        : lineShape[QStringLiteral("toX")].toDouble();
    double py = isStart ? lineShape[QStringLiteral("fromY")].toDouble()
                        : lineShape[QStringLiteral("toY")].toDouble();

    BindingHit hit = findSnapTarget(px, py, lineIndex);
    if (!hit.valid) return;

    QString lineId = lineShape.value(QStringLiteral("id")).toString();
    QString bindingKey = isStart ? QStringLiteral("startBinding") : QStringLiteral("endBinding");
    QString xKey = isStart ? QStringLiteral("fromX") : QStringLiteral("toX");
    QString yKey = isStart ? QStringLiteral("fromY") : QStringLiteral("toY");

    QVariantMap binding;
    binding[QStringLiteral("elementId")] = hit.targetId;
    binding[QStringLiteral("focus")] = hit.focus;
    binding[QStringLiteral("gap")] = 0.0;

    m_shapesModel.updateShape(lineIndex, {
        {bindingKey, binding},
        {xKey, hit.snapPoint.x()},
        {yKey, hit.snapPoint.y()}
    });
    addBackReference(hit.targetId, lineId);
}
