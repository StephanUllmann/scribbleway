#include "overlaywindow.h"

#include <QGuiApplication>

#include "devlog.h"

OverlayWindow::OverlayWindow(LayerSurface *surface, QObject *parent)
    : QObject(parent)
    , m_surface(surface)
{
    // A display switch fires a burst of these, briefly with no screens at all, so wait
    // for it to settle before giving the overlay a new surface.
    m_remapTimer.setSingleShot(true);
    m_remapTimer.setInterval(500);
    connect(&m_remapTimer, &QTimer::timeout, this, &OverlayWindow::remap);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *s) {
        qCInfo(lcDev) << "screen added:" << s->name();
        m_remapTimer.start();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *s) {
        qCInfo(lcDev) << "screen removed:" << s->name();
        m_remapTimer.start();
    });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen *s) {
        qCInfo(lcDev) << "primary screen now:" << (s ? s->name() : QString());
        m_remapTimer.start();
    });
}

void OverlayWindow::attach(QQuickWindow *window)
{
    m_window = window;
    // Geometry, not setScreen(): see remap().
    m_window->setGeometry(QGuiApplication::primaryScreen()->geometry());
    // Remembered so remap() can return here: Qt's primary screen changes when screens
    // come and go, so it can't be relied on to mean the same one afterwards.
    if (m_targetScreen.isEmpty()) {
        m_targetScreen = m_window->screen()->name();
        Q_EMIT targetScreenChanged();
    }
    qCInfo(lcDev) << "overlay placed on" << m_window->screen()->name() << m_window->geometry();
    // A remap keeps this QWindow but changes its screen and size, and the hole has to
    // follow, or it leaves a dead zone where it used to be.
    connect(m_window, &QWindow::screenChanged, this, &OverlayWindow::applyInputMask);
    connect(m_window, &QWindow::widthChanged, this, &OverlayWindow::applyInputMask);
    connect(m_window, &QWindow::heightChanged, this, &OverlayWindow::applyInputMask);

    applyLayerConfig();
    applyInputMask();
    m_window->show();
}

void OverlayWindow::setShapeRegion(const QVariantList &rects)
{
    QRegion region;
    for (const QVariant &v : rects) {
        const QRect r = v.toRectF().toRect();
        if (r.isValid()) region += r;
    }
    m_shapeRegion = region;
    applyInputMask();
}

void OverlayWindow::setHole(QScreen *screen, const LayerConfig &placement, QSize size)
{
    m_holeScreen = screen;
    m_holePlacement = placement;
    m_holeSize = size;
    applyInputMask();
}

// On each axis, a surface anchored to one edge keeps its margin to that edge; one
// anchored to both edges or to neither is centred.
static int placeOnAxis(int area, int size, bool toStart, bool toEnd, int startMargin, int endMargin)
{
    if (toStart == toEnd) return (area - size) / 2;
    return toStart ? startMargin : area - size - endMargin;
}

QRect OverlayWindow::holeRect() const
{
    if (!m_window || !m_holeScreen || m_holeScreen != m_window->screen()) return {};
    const Qt::Edges a = m_holePlacement.anchors;
    const QMargins &m = m_holePlacement.margins;
    return QRect(placeOnAxis(m_window->width(), m_holeSize.width(), a.testFlag(Qt::LeftEdge),
                             a.testFlag(Qt::RightEdge), m.left(), m.right()),
                 placeOnAxis(m_window->height(), m_holeSize.height(), a.testFlag(Qt::TopEdge),
                             a.testFlag(Qt::BottomEdge), m.top(), m.bottom()),
                 m_holeSize.width(), m_holeSize.height());
}

void OverlayWindow::applyInputMask()
{
    if (!m_window) return;

    QRegion region = m_shapeRegion;
    const QRect hole = holeRect();
    if (hole != m_lastHole) {
        m_lastHole = hole;
        qCInfo(lcDev) << "popup exclusion:" << hole;
    }
    if (hole.isValid()) region -= hole;

    // Qt-Wayland reads an empty region as "no mask", which blocks the whole screen, so
    // click-through keeps one pixel in the corner.
    if (region.isEmpty()) region = QRegion(0, 0, 1, 1);

    if (m_lastInputMask == region) return;
    m_lastInputMask = region;
    qCInfo(lcDev) << "input mask:" << region.rectCount() << "rect(s), bounds" << region.boundingRect();
    m_window->setMask(region);
}

void OverlayWindow::setToolWantsKeyboard(bool wants)
{
    m_toolWantsKeyboard = wants;
    applyKeyboard();
}

void OverlayWindow::setKeyboardYielded(bool yielded)
{
    m_keyboardYielded = yielded;
    applyKeyboard();
}

void OverlayWindow::applyKeyboard()
{
    // Exclusive, not OnDemand: OnDemand means "focus me when the user clicks me",
    // and a layer surface has no way to ask for focus itself — QWindow::requestActivate()
    // is a no-op here. KWin happened to hand focus over anyway; Hyprland does not, so
    // every QML Shortcut in main.qml was dead there. Exclusive is what an annotation
    // overlay wants while a tool is active, and it works on both.
    const bool interactive = m_toolWantsKeyboard && !m_keyboardYielded;
    const auto wanted = interactive ? LayerConfig::Keyboard::Exclusive : LayerConfig::Keyboard::None;

    // QML calls setToolWantsKeyboard() on every draw event, several times a second while
    // drawing. Only act on real transitions, so this neither spams the journal nor
    // re-commits the layer surface for a value it already has.
    if (m_config.keyboard == wanted) return;
    m_config.keyboard = wanted;

    // Logged because "my hotkeys don't work" is otherwise undebuggable from outside:
    // neither hyprctl nor kwin exposes layer-shell keyboard focus.
    qInfo() << "keyboard interactivity:" << (interactive ? "exclusive" : "none");
    applyLayerConfig();
}

void OverlayWindow::applyLayerConfig()
{
    if (m_window) m_surface->apply(m_window, m_config);
}

QStringList OverlayWindow::screenNames() const
{
    QStringList names;
    for (QScreen *screen : QGuiApplication::screens()) {
        names << screen->name();
    }
    return names;
}

QStringList OverlayWindow::screenLabels() const
{
    QStringList labels;
    for (QScreen *screen : QGuiApplication::screens()) {
        const QString name = screen->name();
        // Built-in panels are wired as eDP (or LVDS/DSI on older or ARM machines).
        const bool builtIn = name.startsWith(QLatin1String("eDP")) || name.startsWith(QLatin1String("LVDS"))
            || name.startsWith(QLatin1String("DSI"));
        const QString model = screen->model().trimmed();
        if (builtIn) {
            labels << QStringLiteral("Laptop screen (%1)").arg(name);
        } else if (!model.isEmpty()) {
            labels << QStringLiteral("External screen: %1 (%2)").arg(model, name);
        } else {
            labels << QStringLiteral("External screen (%1)").arg(name);
        }
    }
    return labels;
}

QString OverlayWindow::targetScreen() const
{
    return m_targetScreen;
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
void OverlayWindow::remap()
{
    Q_EMIT screensChanged();
    if (!m_window) return;

    QScreen *target = screenNamed(m_targetScreen);
    m_window->hide();
    // Without destroy() the native window survives and keeps its old output.
    m_window->destroy();
    // setGeometry(), not setScreen(): show() re-picks the screen from the window's
    // position, and a stale (0,0) lands on whichever screen sits at the top left.
    m_window->setGeometry((target ? target : QGuiApplication::primaryScreen())->geometry());
    // The new surface gets the whole config, not just whatever the old one carried over.
    applyLayerConfig();
    m_window->show();
    qInfo() << "overlay remapped to" << m_window->screen()->name() << "wanted" << m_targetScreen
            << m_window->geometry();
    DevLog::logScreens("after remap");
}

void OverlayWindow::setTargetScreen(const QString &screenName)
{
    if (!m_window || screenName.isEmpty()) return;

    QScreen *target = screenNamed(screenName);
    qCInfo(lcDev) << "target screen chosen:" << screenName << (target ? "" : "(not found)");

    if (m_targetScreen != screenName) {
        m_targetScreen = screenName;
        Q_EMIT targetScreenChanged();
    }

    // remap() destroys the native window first; a plain setScreen() leaves it on its old
    // output under Wayland.
    if (target && m_window->screen() != target) remap();
}
