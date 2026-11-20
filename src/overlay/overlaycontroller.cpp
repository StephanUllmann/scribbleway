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
#include <QScreen>
#include <QGuiApplication>
#include <QUuid>
#include "devlog.h"
#include "excalidraw.h"
#include "shapegeometry.h"
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

// What the dev log says about a shape: type, short id and bounds, or both endpoints for
// a line or arrow. Never its text or its points, which can hold whatever was on screen.
static QString describeShape(const QVariantMap &shape)
{
    const QString type = shape.value(QStringLiteral("type")).toString();
    const QString head = type + QLatin1Char(' ') + shape.value(QStringLiteral("id")).toString().left(8);
    auto n = [&shape](const char *key) { return QString::number(qRound(shape.value(QLatin1String(key)).toDouble())); };
    if (type == QStringLiteral("line") || type == QStringLiteral("arrow")) {
        return QStringLiteral("%1 (%2,%3)->(%4,%5)").arg(head, n("fromX"), n("fromY"), n("toX"), n("toY"));
    }
    const QRectF b = ShapeGeometry::bounds(shape);
    return QStringLiteral("%1 (%2,%3 %4x%5)")
        .arg(head).arg(qRound(b.x())).arg(qRound(b.y())).arg(qRound(b.width())).arg(qRound(b.height()));
}

// The one list of style keys, with each key's starting value, the Shapes that carry it and
// its range. Adding a style key is one row here. onlyFor: nullptr = every Shape, a type =
// that type only, "" = app-level, never stored on a Shape. A range applies when lo <= hi.
namespace {
struct StyleKey { const char *key; QVariant initial; const char *onlyFor = nullptr; double lo = 0, hi = -1; };
const StyleKey kStyleKeys[] = {
    {"color", QStringLiteral("#e63946")},
    {"strokeWidth", 2},
    {"opacity", 1.0},
    {"fontFamily", QStringLiteral("monospace"), "text"},
    {"fontSize", 20, "text"},
    {"borderRadius", 8, "rectangle"},
    {"roughness", 1, nullptr, 0, 2},
    {"glow", 10},
    {"fillColor", QStringLiteral("#e63946")},
    {"fillOpacity", 0.25, nullptr, 0, 1},
    // Shapes store the smoothed points, not the setting.
    {"freehandSmoothing", 2, "", 0, 3},
};

const StyleKey *findStyleKey(const QString &key)
{
    for (const StyleKey &k : kStyleKeys) {
        if (key == QLatin1String(k.key)) return &k;
    }
    return nullptr;
}

// The value clamped to the key's range and converted to the type of its starting value.
QVariant fitStyle(const StyleKey &k, QVariant value)
{
    if (k.lo <= k.hi) value = qBound(k.lo, value.toDouble(), k.hi);
    value.convert(k.initial.metaType());
    return value;
}

// The one table of every hotkey: global (KGlobalAccel in main.cpp) and overlay-local
// (QML Shortcut Instantiator in main.qml). Adding a hotkey is one row here; ids, defaults,
// overrides and handlers cannot drift out of step.
using Scope = HotkeySpec::Scope;
const HotkeySpec kHotkeys[] = {
    // Global (GlobalDev is registered only under --dev so a normal install claims one key)
    {.id = "action_activate_select_mode", .defaultSeq = "Meta+Shift+X",
     .run = [](auto &c, auto) { c.toggleMode(); },
     .scope = Scope::Global, .label = "Enter Selection Mode"},
    {.id = "action_dev_mark", .defaultSeq = "Meta+Shift+M",
     .run = [](auto &, auto) { DevLog::mark(); },
     .scope = Scope::GlobalDev, .label = "Mark the Dev Log"},

    // Cancel (app-wide so Escape works whether the overlay or the Tray Popup holds focus)
    {.id = "action_cancel", .defaultSeq = "Escape",
     .run = [](auto &c, auto) { c.enterPassthroughMode(); },
     .scope = Scope::Application, .needs = "always"},

    // Drawing tools
    {.id = "tool_arrow",     .defaultSeq = "A", .run = [](auto &c, auto) { c.toggleTool(QStringLiteral("arrow")); }},
    {.id = "tool_rectangle", .defaultSeq = "R", .run = [](auto &c, auto) { c.toggleTool(QStringLiteral("rectangle")); }},
    {.id = "tool_freehand",  .defaultSeq = "F", .run = [](auto &c, auto) { c.toggleTool(QStringLiteral("freehand")); }},
    {.id = "tool_ellipse",   .defaultSeq = "E", .run = [](auto &c, auto) { c.toggleTool(QStringLiteral("ellipse")); }},
    {.id = "tool_line",      .defaultSeq = "L", .run = [](auto &c, auto) { c.toggleTool(QStringLiteral("line")); }},
    {.id = "tool_text",      .defaultSeq = "T", .run = [](auto &c, auto) { c.toggleTool(QStringLiteral("text")); }},

    // Palette
    {.id = "color_cycle", .defaultSeq = "Q", .run = [](auto &c, auto) { c.cycleColor(); }},
    {.id = "color_1",     .defaultSeq = "1", .run = [](auto &c, auto) { c.selectPresetColor(0); }},
    {.id = "color_2",     .defaultSeq = "2", .run = [](auto &c, auto) { c.selectPresetColor(1); }},
    {.id = "color_3",     .defaultSeq = "3", .run = [](auto &c, auto) { c.selectPresetColor(2); }},
    {.id = "color_4",     .defaultSeq = "4", .run = [](auto &c, auto) { c.selectPresetColor(3); }},
    {.id = "color_5",     .defaultSeq = "5", .run = [](auto &c, auto) { c.selectPresetColor(4); }},
    {.id = "color_6",     .defaultSeq = "6", .run = [](auto &c, auto) { c.selectPresetColor(5); }},

    // Actions & editing
    {.id = "action_select",          .defaultSeq = "X",            .run = [](auto &c, auto) { c.enterSelectMode(); }},
    {.id = "action_grow",            .defaultSeq = "+",            .run = [](auto &c, auto) { c.growSelected(); }},
    {.id = "action_shrink",          .defaultSeq = "-",            .run = [](auto &c, auto) { c.shrinkSelected(); }},
    {.id = "action_cycle_roughness", .defaultSeq = "S",            .run = [](auto &c, auto) { c.cycleRoughness(); }},
    {.id = "action_undo",            .defaultSeq = "Ctrl+Z",       .run = [](auto &c, auto) { c.undo(); }},
    {.id = "action_redo",            .defaultSeq = "Ctrl+Shift+Z", .run = [](auto &c, auto) { c.redo(); }},
    {.id = "action_clear",           .defaultSeq = "Ctrl+Delete",  .run = [](auto &c, auto) { c.clear(); }},
    {.id = "action_toggle_shapes",   .defaultSeq = "Y",            .run = [](auto &c, auto) { c.toggleShapesHidden(); }},
    {.id = "action_increase_border_radius", .defaultSeq = "ü",     .run = [](auto &c, auto) { c.increaseBorderRadius(); }, .needs = "rectangle"},
    {.id = "action_decrease_border_radius", .defaultSeq = "ä",     .run = [](auto &c, auto) { c.decreaseBorderRadius(); }, .needs = "rectangle"},
    {.id = "action_delete",          .defaultSeq = "Delete",       .run = [](auto &c, auto) { c.deleteSelected(); }, .altSeq = "Backspace"},
    {.id = "action_copy",            .defaultSeq = nullptr,        .run = [](auto &c, auto) { c.copySelected(); }, .standardKey = QKeySequence::Copy},
    {.id = "action_paste",           .defaultSeq = nullptr,        .run = [](auto &c, QPointF pos) { c.pasteFromClipboard(pos.x(), pos.y()); }, .standardKey = QKeySequence::Paste},

    // Arrow-key nudge: move selected Shape(s) by 1px, or 10px with Shift.
    {.id = "action_nudge_left",       .defaultSeq = "Left",        .run = [](auto &c, auto) { c.nudgeSelected(-1, 0); },   .needs = "selection"},
    {.id = "action_nudge_right",      .defaultSeq = "Right",       .run = [](auto &c, auto) { c.nudgeSelected(1, 0); },    .needs = "selection"},
    {.id = "action_nudge_up",         .defaultSeq = "Up",          .run = [](auto &c, auto) { c.nudgeSelected(0, -1); },   .needs = "selection"},
    {.id = "action_nudge_down",       .defaultSeq = "Down",        .run = [](auto &c, auto) { c.nudgeSelected(0, 1); },    .needs = "selection"},
    {.id = "action_nudge_left_fast",  .defaultSeq = "Shift+Left",  .run = [](auto &c, auto) { c.nudgeSelected(-10, 0); },  .needs = "selection"},
    {.id = "action_nudge_right_fast", .defaultSeq = "Shift+Right", .run = [](auto &c, auto) { c.nudgeSelected(10, 0); },   .needs = "selection"},
    {.id = "action_nudge_up_fast",    .defaultSeq = "Shift+Up",    .run = [](auto &c, auto) { c.nudgeSelected(0, -10); },  .needs = "selection"},
    {.id = "action_nudge_down_fast",  .defaultSeq = "Shift+Down",  .run = [](auto &c, auto) { c.nudgeSelected(0, 10); },   .needs = "selection"},
};
} // namespace

OverlayController::OverlayController(QObject *parent)
    : QObject(parent)
    , m_activeTool(QStringLiteral("freehand"))
{
    for (const StyleKey &k : kStyleKeys) {
        m_style.insert(QLatin1String(k.key), k.initial);
    }
    m_shapesModel.setParent(this);
    m_shapesModel.setSelection(&m_selection);
    m_draftModel.setParent(this);
    m_draftModel.disableHistory();
    connect(&m_selection, &Selection::changed, this, &OverlayController::selectionChanged);
    // A new or restored Shape would otherwise vanish while shapes are hidden.
    connect(&m_shapesModel, &QAbstractItemModel::rowsInserted, this, [this] { setShapesHidden(false); });
    connect(&m_shapesModel, &QAbstractItemModel::modelReset, this, [this] {
        if (m_shapesModel.rowCount() > 0) setShapesHidden(false);
    });

    // Hotkeys are read here only; there is no UI for rebinding them, but hand-edited
    // entries in ~/.config/scribbleway/shortcuts.conf still apply.
    QSettings settings(QStringLiteral("scribbleway"), QStringLiteral("shortcuts"));
    for (const HotkeySpec &h : kHotkeys) {
        if (h.scope == Scope::Global || h.scope == Scope::GlobalDev) continue;

        const QString id = QString::fromLatin1(h.id);
        const QString settingsKey = QStringLiteral("local/") + id;
        QVariantList sequences;
        QString displaySeq;
        if (settings.contains(settingsKey)) {
            displaySeq = settings.value(settingsKey).toString();
            if (!displaySeq.isEmpty()) sequences.append(displaySeq);
        } else if (h.standardKey != QKeySequence::UnknownKey) {
            for (const QKeySequence &ks : QKeySequence::keyBindings(h.standardKey)) {
                const QString text = ks.toString(QKeySequence::PortableText);
                if (!text.isEmpty()) sequences.append(text);
            }
            if (!sequences.isEmpty()) displaySeq = sequences.first().toString();
        } else {
            displaySeq = QString::fromUtf8(h.defaultSeq);
            if (!displaySeq.isEmpty()) sequences.append(displaySeq);
            if (h.altSeq) sequences.append(QString::fromUtf8(h.altSeq));
        }

        m_localShortcutSequences.insert(id, displaySeq);
        m_localShortcuts.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("sequences"), sequences},
            {QStringLiteral("needs"), h.needs ? QString::fromLatin1(h.needs) : QString()},
            {QStringLiteral("appWide"), h.scope == Scope::Application},
        });
    }

    // The mode changes in several places; one listener logs them all.
    connect(this, &OverlayController::modeChanged, this,
            [](const QString &mode) {
                qCInfo(lcDev).noquote() << "mode:" << mode
                                        << (mode == QStringLiteral("passthrough") ? "(click-through)" : "");
            });
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


ShapesModel* OverlayController::shapesModel()
{
    return &m_shapesModel;
}

ShapesModel* OverlayController::draftModel()
{
    return &m_draftModel;
}

void OverlayController::setDraftShape(const QVariantMap &shape)
{
    if (shape.isEmpty()) {
        m_draftModel.clear();
    } else if (m_draftModel.rowCount() == 0) {
        m_draftModel.addShape(shape);
    } else {
        // Updated in place, so the delegate survives every mouse move.
        m_draftModel.updateShape(0, shape);
    }
}

int OverlayController::selectedIndex() const
{
    return indexForId(m_selection.primary());
}

bool OverlayController::isSelected(int index) const
{
    return m_selection.contains(m_shapesModel.shapeIdAt(index));
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
            setMode(QStringLiteral("draw"));
            m_selection.clear();
        } else if (m_currentMode == QStringLiteral("draw")) {
            setMode(QStringLiteral("passthrough"));
        }
    }
}

QString OverlayController::currentMode() const
{
    return m_currentMode;
}

QVariantMap OverlayController::style() const
{
    return m_style;
}

QVariantMap OverlayController::styleForNewShape(const QString &type) const
{
    QVariantMap out;
    for (const StyleKey &k : kStyleKeys) {
        if (!k.onlyFor || (*k.onlyFor && type == QLatin1String(k.onlyFor))) {
            out.insert(QLatin1String(k.key), m_style.value(QLatin1String(k.key)));
        }
    }
    return out;
}

bool OverlayController::writeStyle(const QString &key, const QVariant &value)
{
    const StyleKey *k = findStyleKey(key);
    if (!k) return false;
    const QVariant fitted = fitStyle(*k, value);
    if (m_style.value(key) == fitted) return false;
    m_style.insert(key, fitted);
    return true;
}

void OverlayController::addShape(const QVariantMap &shape)
{
    m_shapesModel.beginEdit();
    m_shapesModel.addShape(shape);
    qCInfo(lcDev).noquote() << "add" << describeShape(m_shapesModel.shapes().last());
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
        {QStringLiteral("fontFamily"), shape.value(QStringLiteral("fontFamily"), m_style.value(QStringLiteral("fontFamily")))},
        {QStringLiteral("fontSize"), shape.value(QStringLiteral("fontSize"), m_style.value(QStringLiteral("fontSize")))},
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

QRectF OverlayController::attachedTextRect(int index) const
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return QRectF();
    }
    return ShapeGeometry::attachedTextRect(m_shapesModel.shapes().at(index), m_style.value(QStringLiteral("fontSize")).toInt());
}

QVariantMap OverlayController::ensureAttachedTextForShape(int index)
{
    if (index < 0 || index >= m_shapesModel.rowCount()) {
        return QVariantMap();
    }

    QVariantMap shape = m_shapesModel.shapes().at(index);
    const QString type = shape.value(QStringLiteral("type")).toString();
    if (!ShapeGeometry::supportsAttachedText(type)) {
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
    if (index == selectedIndex()) {
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
    if (!ShapeGeometry::supportsAttachedText(shape.value(QStringLiteral("type")).toString())) {
        return;
    }

    QVariantList bindings = ShapesModel::withoutAttachedTextBinding(shape);
    QVariantMap binding = ShapesModel::firstAttachedTextBinding(shape);
    if (binding.isEmpty()) {
        binding = makeAttachedTextBinding(shape);
    }
    const QString fontFamily = shape.value(QStringLiteral("fontFamily"), m_style.value(QStringLiteral("fontFamily"))).toString();
    const int fontSize = shape.value(QStringLiteral("fontSize"), m_style.value(QStringLiteral("fontSize"))).toInt();
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
    if (index == selectedIndex()) {
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
    if (index == selectedIndex()) {
        notifySelectionChanged();
    }
}

void OverlayController::updateShape(int index, const QVariantMap &properties)
{
    m_shapesModel.updateShape(index, properties);

    if (index == selectedIndex()) {
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
    // Re-picking the primary leaves the rest of a multi-selection alone.
    if (index != -1 && index == selectedIndex()) return;
    const QString id = m_shapesModel.shapeIdAt(index);
    m_selection.set({}, id);
    if (id.isEmpty()) return;

    // The selected shape's style becomes the style for the next new shape.
    const QVariantMap shape = m_shapesModel.shapes()[index];
    bool changed = false;
    for (const StyleKey &k : kStyleKeys) {
        const QString key = QLatin1String(k.key);
        if (shape.contains(key)) changed |= writeStyle(key, shape.value(key));
    }
    if (changed) Q_EMIT styleChanged();

    ensureSelectMode();
}

QVariantMap OverlayController::getSelectionState() const
{
    QVariantMap state = m_style;
    const int index = selectedIndex();
    if (index >= 0) {
        const QVariantMap shape = m_shapesModel.shapes()[index];
        for (const StyleKey &k : kStyleKeys) {
            const QString key = QLatin1String(k.key);
            if (shape.contains(key)) state.insert(key, fitStyle(k, shape.value(key)));
        }
        state[QStringLiteral("hasSelection")] = true;
        state[QStringLiteral("type")] = shape[QStringLiteral("type")].toString();
        state[QStringLiteral("seed")] = shape.value(QStringLiteral("seed"), 123456).toInt();
        state[QStringLiteral("locked")] = shape.value(QStringLiteral("locked"), false).toBool();
        state[QStringLiteral("selectedIndex")] = index;
    } else {
        state[QStringLiteral("hasSelection")] = false;
        state[QStringLiteral("type")] = QStringLiteral("");
        state[QStringLiteral("locked")] = false;
        state[QStringLiteral("selectedIndex")] = -1;
    }
    return state;
}

bool OverlayController::hasSelection() const
{
    return selectedIndex() >= 0;
}


QStringList OverlayController::presetColors()
{
    return kPresetColors;
}

void OverlayController::updateProperties(const QVariantMap &properties)
{
    QVariantMap demarshalled = properties;

    // A style key both sets the style for new shapes and goes on to the selected shapes,
    // except an app-level key, which no shape stores.
    bool changed = false;
    for (const StyleKey &k : kStyleKeys) {
        const auto it = demarshalled.find(QLatin1String(k.key));
        if (it == demarshalled.end()) continue;
        *it = fitStyle(k, *it);
        changed |= writeStyle(it.key(), *it);
        if (k.onlyFor && !*k.onlyFor) demarshalled.erase(it);
    }

    // The fill is the stroke color at fill opacity: a new stroke color carries over to
    // any fill that is switched on, unless this same edit picks a fill color itself.
    const QVariant newColor = demarshalled.value(QStringLiteral("color"));
    const bool fillFollows = newColor.isValid() && !demarshalled.contains(QStringLiteral("fillColor"));
    auto hasFill = [](const QString &fill) {
        return !fill.isEmpty() && fill.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) != 0;
    };
    if (fillFollows && hasFill(m_style.value(QStringLiteral("fillColor")).toString())) {
        changed |= writeStyle(QStringLiteral("fillColor"), newColor);
    }
    if (changed) Q_EMIT styleChanged();

    const bool hasFontUpdate = demarshalled.contains(QStringLiteral("fontFamily")) || demarshalled.contains(QStringLiteral("fontSize"));
    // A container's attached text carries its own font, so a font change has to reach
    // the binding as well as the shape.
    auto applyTo = [&](int index) {
        QVariantMap shapeProps = demarshalled;
        const QVariantMap shape = m_shapesModel.shapes()[index];
        if (fillFollows && hasFill(shape.value(QStringLiteral("fillColor")).toString())) {
            shapeProps.insert(QStringLiteral("fillColor"), newColor);
        }
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
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (isSelected(i)) applyTo(i);
    }
    m_shapesModel.endEdit();

    notifySelectionChanged();
}

void OverlayController::undo()
{
    m_shapesModel.undo();
    qCInfo(lcDev) << "undo, shapes now" << m_shapesModel.rowCount();
    setSelectedIndex(-1);
    notifySelectionChanged();
}

void OverlayController::redo()
{
    m_shapesModel.redo();
    qCInfo(lcDev) << "redo, shapes now" << m_shapesModel.rowCount();
    setSelectedIndex(-1);
    notifySelectionChanged();
}

void OverlayController::clear()
{
    qCInfo(lcDev) << "clear" << m_shapesModel.rowCount() << "shape(s)";
    m_shapesModel.clear();
    m_selection.clear();
}

void OverlayController::beginEdit()
{
    m_shapesModel.beginEdit();
    m_editStartShapes = m_shapesModel.shapes();
    m_dragStartShapes.clear();
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (isSelected(i)) {
            m_dragStartShapes.insert(i, m_shapesModel.shapes()[i]);
        }
    }
}

void OverlayController::endEdit()
{
    // One line per changed shape at the end of a gesture, rather than one per mouse move.
    // Every shape is compared by id, so arrows pulled along by a bound shape show up too.
    if (lcDev().isInfoEnabled()) {
        QHash<QString, QString> before;
        for (const QVariantMap &shape : std::as_const(m_editStartShapes)) {
            before.insert(shape.value(QStringLiteral("id")).toString(), describeShape(shape));
        }
        for (const QVariantMap &shape : m_shapesModel.shapes()) {
            const QString was = before.value(shape.value(QStringLiteral("id")).toString());
            const QString now = describeShape(shape);
            if (!was.isEmpty() && was != now) qCInfo(lcDev).noquote() << "edit" << was << "=>" << now;
        }
    }
    m_editStartShapes.clear();
    m_shapesModel.endEdit();
}

void OverlayController::setMode(const QString &mode)
{
    if (m_currentMode == mode) return;
    // Reopening the overlay always starts with the Shapes visible.
    const bool reopening = m_currentMode == QStringLiteral("passthrough");
    m_currentMode = mode;
    if (reopening) setShapesHidden(false);
    Q_EMIT modeChanged(m_currentMode);
}

void OverlayController::enterSelectMode()
{
    Q_EMIT enterSelectModeRequested();
    setActiveTool(QStringLiteral(""));
    setMode(QStringLiteral("select"));
}

void OverlayController::enterPassthroughMode()
{
    Q_EMIT enterPassthroughModeRequested();
    setActiveTool(QStringLiteral(""));
    setMode(QStringLiteral("passthrough"));
}

void OverlayController::toggleMode()
{
    qInfo() << "toggle requested, current mode:" << m_currentMode;
    if (m_currentMode == QStringLiteral("passthrough")) {
        enterSelectMode();
    } else {
        enterPassthroughMode();
    }
}

void OverlayController::deleteSelected()
{
    m_shapesModel.beginEdit();
    for (int i = m_shapesModel.rowCount() - 1; i >= 0; --i) {
        if (isSelected(i)) deleteShape(i);
    }
    m_shapesModel.endEdit();
}

void OverlayController::raiseSelected()
{
    const int index = selectedIndex();
    if (index >= 0 && index < m_shapesModel.rowCount() - 1 && m_shapesModel.moveShape(index, index + 1)) {
        notifySelectionChanged();
    }
}

void OverlayController::lowerSelected()
{
    const int index = selectedIndex();
    if (index > 0 && m_shapesModel.moveShape(index, index - 1)) {
        notifySelectionChanged();
    }
}

void OverlayController::setShapeLocked(int index, bool locked)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        m_shapesModel.updateShape(index, {{QStringLiteral("locked"), locked}});
        // A locked shape must not stay in a selection, or group moves would still reach it.
        if (locked) deselect(index);
    }
}

void OverlayController::deleteShape(int index)
{
    if (index >= 0 && index < m_shapesModel.rowCount()) {
        // One undo step for the detach and the removal, or undo brings the shape back unattached.
        qCInfo(lcDev).noquote() << "delete" << describeShape(m_shapesModel.shapes().at(index));
        // The primary's row is re-read after the removal, which shifts every row below it.
        const bool touchesSelection = isSelected(index) || index < selectedIndex();
        deselect(index);
        m_shapesModel.beginEdit();
        cleanupBindingsForDelete(index);
        m_shapesModel.removeShape(index);
        m_shapesModel.endEdit();
        if (touchesSelection) notifySelectionChanged();
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

    const QString id = m_shapesModel.shapeIdAt(index);
    const bool wasSelected = m_selection.contains(id);

    if (shiftHeld && wasSelected) {
        deselect(index);
        return;
    }
    // Shift adds to the selection; a plain click on a selected shape only makes it the primary.
    m_selection.set(shiftHeld || wasSelected ? m_selection.ids() : QSet<QString>{}, id);
    if (!wasSelected) ensureSelectMode();
}

void OverlayController::deselect(int index)
{
    const QString id = m_shapesModel.shapeIdAt(index);
    if (!m_selection.contains(id)) return;
    QSet<QString> rest = m_selection.ids();
    rest.remove(id);
    QString primary = m_selection.primary();
    if (primary == id) {
        primary.clear();
        for (int i = 0; i < m_shapesModel.rowCount() && primary.isEmpty(); ++i) {
            if (rest.contains(m_shapesModel.shapeIdAt(i))) primary = m_shapesModel.shapeIdAt(i);
        }
    }
    m_selection.set(rest, primary);
}

void OverlayController::notifySelectionChanged()
{
    Q_EMIT selectionChanged();
}

void OverlayController::ensureSelectMode()
{
    setMode(QStringLiteral("select"));
    if (!m_activeTool.isEmpty()) {
        m_activeTool = QStringLiteral("");
        Q_EMIT activeToolChanged(m_activeTool);
    }
}

std::span<const HotkeySpec> OverlayController::hotkeys()
{
    return kHotkeys;
}

QVariantList OverlayController::localShortcuts() const
{
    return m_localShortcuts;
}

QVariantMap OverlayController::localShortcutSequences() const
{
    return m_localShortcutSequences;
}

bool OverlayController::shapesHidden() const
{
    return m_shapesHidden;
}

void OverlayController::setShapesHidden(bool hidden)
{
    if (m_shapesHidden == hidden) return;
    m_shapesHidden = hidden;
    qCInfo(lcDev).noquote() << "shapes" << (m_shapesHidden ? "hidden" : "shown");
    // Keep Delete, nudge and resize from acting on Shapes nobody can see.
    if (m_shapesHidden) setSelectedIndex(-1);
    Q_EMIT shapesHiddenChanged();
}

void OverlayController::toggleShapesHidden()
{
    setShapesHidden(!m_shapesHidden);
}

bool OverlayController::triggerShortcut(const QString &id, const QPointF &cursor)
{
    for (const HotkeySpec &h : kHotkeys) {
        if (id == QLatin1String(h.id)) {
            if (!h.run) return false;
            h.run(*this, cursor);
            return true;
        }
    }
    return false;
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
    const QVariant fallback = m_style.value(QStringLiteral("roughness"));
    int currentRoughness = fallback.toInt();
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        if (isSelected(i)) {
            currentRoughness = m_shapesModel.shapes()[i].value(QStringLiteral("roughness"), fallback).toInt();
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
        if (isSelected(i)) {
            QVariantMap shape = m_shapesModel.shapes().at(i);
            QJsonObject elemObj = Excalidraw::elementFromShape(shape);
            if (!elemObj.isEmpty()) {
                elements.append(elemObj);
                const QVariantMap attached = ShapesModel::firstAttachedTextBinding(shape);
                if (!attached.isEmpty()) {
                    const QJsonObject textObj = Excalidraw::attachedTextElement(shape, attached, m_style.value(QStringLiteral("fontSize")).toInt());
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
        qCInfo(lcDev) << "copy" << elements.size() << "element(s)";
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

    Excalidraw::attachBoundText(list, m_style.value(QStringLiteral("fontFamily")).toString(), m_style.value(QStringLiteral("fontSize")).toInt());

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
                binding[QStringLiteral("focus")] = ShapeGeometry::focusAt(target, endpoint);
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

    // Corners rather than QRectF::united, which drops a zero-size shape such as a lone point.
    // A freehand stroke without points has no position, so it does not pull the centre to 0,0.
    for (const QVariantMap &shape : list) {
        const QRectF r = ShapeGeometry::bounds(shape);
        if (shape.value(QStringLiteral("type")).toString() == QStringLiteral("freehand")
            && shape.value(QStringLiteral("points")).toList().isEmpty()) {
            continue;
        }
        minX = qMin(minX, r.left());
        maxX = qMax(maxX, r.right());
        minY = qMin(minY, r.top());
        maxY = qMax(maxY, r.bottom());
    }

    if (minX > maxX || minY > maxY) return;

    double cx = (minX + maxX) / 2.0;
    double cy = (minY + maxY) / 2.0;
    double dx = localMousePos.x() - cx;
    double dy = localMousePos.y() - cy;

    m_shapesModel.beginEdit();
    QSet<QString> pastedIds;
    for (QVariantMap shape : list) {
        shape.insert(QStringLiteral("locked"), false);
        ShapeGeometry::translate(shape, {dx, dy});
        m_shapesModel.addShape(shape);
        pastedIds.insert(shape.value(QStringLiteral("id")).toString());
    }
    qCInfo(lcDev) << "paste" << list.size() << "shape(s)";
    m_shapesModel.endEdit();

    // The paste replaces the selection, with the last pasted shape as the primary.
    m_selection.set(pastedIds, list.last().value(QStringLiteral("id")).toString());
}

bool OverlayController::hasMultiSelection() const
{
    return m_selection.count() > 1;
}

void OverlayController::beginDragSelection(bool shiftHeld)
{
    m_preDragSelection.clear();
    if (shiftHeld) {
        for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
            if (isSelected(i)) {
                m_preDragSelection.insert(i);
            }
        }
    }
}

void OverlayController::selectShapesInRect(double rx, double ry, double rw, double rh, bool /*shiftHeld*/)
{
    QRectF selectRect(rx, ry, rw, rh);
    selectRect = selectRect.normalized();

    QSet<QString> ids;
    QString primary;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        const QVariantMap shape = m_shapesModel.shapes()[i];
        const bool intersects = ShapeGeometry::intersects(shape, selectRect);

        // A locked shape never joins a selection, so group moves and deletes cannot reach it.
        bool shouldSelect = !shape.value(QStringLiteral("locked"), false).toBool()
            && (intersects || m_preDragSelection.contains(i));

        if (shouldSelect) {
            primary = shape.value(QStringLiteral("id")).toString();
            ids.insert(primary);
        }
    }
    m_selection.set(ids, primary);
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
    if (!m_dragStartShapes.isEmpty()) {
        moveShapes(m_dragStartShapes, dx, dy);
    }
}

void OverlayController::nudgeSelected(double dx, double dy)
{
    QHash<int, QVariantMap> starts;
    for (int i = 0; i < m_shapesModel.rowCount(); ++i) {
        const QVariantMap &shape = m_shapesModel.shapes()[i];
        if (isSelected(i) && !shape.value(QStringLiteral("locked"), false).toBool()) {
            starts.insert(i, shape);
        }
    }
    moveShapes(starts, dx, dy);
}

// Puts each shape at its start state moved by (dx, dy), lets go of shapes left behind,
// and drags along the arrow ends attached to moved boxes.
void OverlayController::moveShapes(const QHash<int, QVariantMap> &starts, double dx, double dy)
{
    m_shapesModel.beginEdit();
    const QSet<int> moved(starts.keyBegin(), starts.keyEnd());
    for (auto it = starts.begin(); it != starts.end(); ++it) {
        QVariantMap shape = it.value();
        detachFromUnmoved(it.key(), shape, moved);
        ShapeGeometry::translate(shape, {dx, dy});
        m_shapesModel.updateShape(it.key(), shape);
    }
    for (int idx : moved) {
        const QString type = m_shapesModel.shapes()[idx][QStringLiteral("type")].toString();
        if (type == QStringLiteral("rectangle") || type == QStringLiteral("ellipse")) {
            updateBoundEndpoints(idx, &moved);
        }
    }
    m_shapesModel.endEdit();
}

BindingHit OverlayController::findSnapTarget(double px, double py, int excludeIndex) const
{
    BindingHit best;
    double bestDist = kSnapThreshold + 1.0;

    const auto &shapes = m_shapesModel.shapes();
    for (int i = 0; i < shapes.size(); ++i) {
        if (i == excludeIndex) continue;

        const auto &shape = shapes[i];
        if (!ShapeGeometry::hasOutline(shape.value(QStringLiteral("type")).toString()))
            continue;
        if (shape.value(QStringLiteral("locked"), false).toBool())
            continue;

        const QPointF snapPt = ShapeGeometry::nearestOutlinePoint(shape, {px, py});
        double dx = snapPt.x() - px, dy = snapPt.y() - py;
        double dist = qSqrt(dx * dx + dy * dy);
        if (dist <= kSnapThreshold && dist < bestDist) {
            bestDist = dist;
            best.targetId = shape.value(QStringLiteral("id")).toString();
            best.focus = ShapeGeometry::focusAt(shape, snapPt);
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

    qCInfo(lcDev).noquote() << "bind" << lineId.left(8) << "start ->" << startHit.targetId.left(8)
                            << "end ->" << endHit.targetId.left(8);
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
            QPointF p = ShapeGeometry::pointAtFocus(targetShape, sb.value(QStringLiteral("focus")).toDouble());
            updates[QStringLiteral("fromX")] = p.x();
            updates[QStringLiteral("fromY")] = p.y();
        }

        QVariantMap eb = lineShape.value(QStringLiteral("endBinding")).toMap();
        if (eb.value(QStringLiteral("elementId")).toString() == targetId) {
            QPointF p = ShapeGeometry::pointAtFocus(targetShape, eb.value(QStringLiteral("focus")).toDouble());
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
    qCInfo(lcDev).noquote() << "unbind" << lineId.left(8) << (isStart ? "start" : "end")
                            << "from" << targetId.left(8);
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
    qCInfo(lcDev).noquote() << "resnap" << lineShape.value(QStringLiteral("id")).toString().left(8)
                            << (isStart ? "start" : "end") << "to" << hit.targetId.left(8);

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
