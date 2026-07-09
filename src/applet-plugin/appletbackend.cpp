#include "appletbackend.h"

#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QDBusReply>
#include <QKeySequence>
#include <QGuiApplication>
#include <QScreen>
#include <QProcess>

#include "../common/dbusutils.h"

AppletBackend::AppletBackend(QObject *parent)
    : QObject(parent)
    , m_selectedColor(QStringLiteral("#ffffff"))
{
    if (auto *primary = QGuiApplication::primaryScreen()) {
        m_targetScreen = primary->name();
    }
    
    m_selectedFontFamily = QStringLiteral("monospace");
    
    // Watch for the overlay D-Bus service
    auto *watcher = new QDBusServiceWatcher(
        QStringLiteral("org.kde.scribbleway"),
        QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForOwnerChange,
        this
    );
    
    connect(watcher, &QDBusServiceWatcher::serviceRegistered,
            this, &AppletBackend::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &AppletBackend::onServiceUnregistered);
            
    // Initial connection attempt
    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.kde.scribbleway"))) {
        connectToOverlay();
    }
}


bool AppletBackend::overlayConnected() const
{
    return m_overlayConnected;
}

bool AppletBackend::hasSelection() const
{
    return m_hasSelection;
}

QString AppletBackend::selectedType() const
{
    return m_selectedType;
}

QString AppletBackend::selectedColor() const
{
    return m_selectedColor;
}

int AppletBackend::selectedStrokeWidth() const
{
    return m_selectedStrokeWidth;
}

double AppletBackend::selectedOpacity() const
{
    return m_selectedOpacity;
}

QString AppletBackend::selectedFontFamily() const
{
    return m_selectedFontFamily;
}

int AppletBackend::selectedFontSize() const
{
    return m_selectedFontSize;
}

bool AppletBackend::selectedLocked() const
{
    return m_selectedLocked;
}


QVariantList AppletBackend::shapesList() const
{
    return m_shapesList;
}

int AppletBackend::selectedShapeIndex() const
{
    return m_selectedShapeIndex;
}

int AppletBackend::selectedBorderRadius() const
{
    return m_selectedBorderRadius;
}

QString AppletBackend::currentMode() const
{
    return m_currentMode;
}


QString AppletBackend::activeTool() const
{
    return m_activeTool;
}

void AppletBackend::setTool(const QString &tool)
{
    sendDBus(QStringLiteral("setActiveTool"), {tool});
}

void AppletBackend::setColor(const QString &color)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("color"), color}} });
}

void AppletBackend::setStrokeWidth(int width)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("strokeWidth"), width}} });
}

void AppletBackend::setOpacity(double opacity)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("opacity"), opacity}} });
}

void AppletBackend::setFontFamily(const QString &family)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("fontFamily"), family}} });
}

void AppletBackend::setFontSize(int size)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("fontSize"), size}} });
}

void AppletBackend::setBorderRadius(int radius)
{
    sendDBus(QStringLiteral("updateProperties"), { QVariantMap{{QStringLiteral("borderRadius"), radius}} });
}

void AppletBackend::undo()
{
    sendDBus(QStringLiteral("undo"));
}

void AppletBackend::clear()
{
    sendDBus(QStringLiteral("clear"));
}

void AppletBackend::deleteSelected()
{
    sendDBus(QStringLiteral("deleteSelected"));
}

void AppletBackend::toggleLock()
{
    sendDBus(QStringLiteral("toggleLock"));
}

void AppletBackend::raiseSelected()
{
    sendDBus(QStringLiteral("raiseSelected"));
}

void AppletBackend::lowerSelected()
{
    sendDBus(QStringLiteral("lowerSelected"));
}

void AppletBackend::setShapeLocked(int index, bool locked)
{
    sendDBus(QStringLiteral("setShapeLocked"), {index, locked});
}

void AppletBackend::deleteShape(int index)
{
    sendDBus(QStringLiteral("deleteShape"), {index});
}

void AppletBackend::selectShape(int index)
{
    sendDBus(QStringLiteral("selectShape"), {index});
}

void AppletBackend::enterSelectMode()
{
    sendDBus(QStringLiteral("enterSelectMode"));
}

void AppletBackend::enterPassthroughMode()
{
    sendDBus(QStringLiteral("enterPassthroughMode"));
}

void AppletBackend::onServiceRegistered(const QString &serviceName)
{
    Q_UNUSED(serviceName)
    connectToOverlay();
}

void AppletBackend::onServiceUnregistered(const QString &serviceName)
{
    Q_UNUSED(serviceName)
    m_overlayConnected = false;
    m_hasSelection = false;
    m_selectedShapeIndex = -1;
    m_activeTool = QString();
    m_currentMode = QStringLiteral("passthrough");
    m_shapesList.clear();
    m_shortcuts.clear();
    if (m_dbusInterface) {
        delete m_dbusInterface;
        m_dbusInterface = nullptr;
    }
    Q_EMIT overlayConnectedChanged();
    Q_EMIT selectionChanged();
    Q_EMIT shapesListChanged();
    Q_EMIT modeChanged();
    Q_EMIT activeToolChanged();
    Q_EMIT shortcutsChanged();
}

void AppletBackend::connectToOverlay()
{
    if (m_dbusInterface) {
        delete m_dbusInterface;
    }
    
    m_dbusInterface = new QDBusInterface(
        QStringLiteral("org.kde.scribbleway"),
        QStringLiteral("/Overlay"),
        QStringLiteral("org.kde.scribbleway.OverlayController"),
        QDBusConnection::sessionBus(),
        this
    );
    
    if (m_dbusInterface->isValid()) {
        m_overlayConnected = true;
        
        // Sync current target screen to overlay
        if (!m_targetScreen.isEmpty()) {
            m_dbusInterface->asyncCall(QStringLiteral("setTargetScreen"), m_targetScreen);
        }
        
        // Connect to signals from the overlay app
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.scribbleway"),
            QStringLiteral("/Overlay"),
            QStringLiteral("org.kde.scribbleway.OverlayController"),
            QStringLiteral("selectionChanged"),
            this,
            SLOT(onSelectionChanged(QVariantMap))
        );
        
        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.scribbleway"),
            QStringLiteral("/Overlay"),
            QStringLiteral("org.kde.scribbleway.OverlayController"),
            QStringLiteral("shapesMetadataChanged"),
            this,
            SLOT(onShapesMetadataChanged(QVariantList))
        );

        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.scribbleway"),
            QStringLiteral("/Overlay"),
            QStringLiteral("org.kde.scribbleway.OverlayController"),
            QStringLiteral("modeChanged"),
            this,
            SLOT(onModeChanged(QString))
        );

        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.scribbleway"),
            QStringLiteral("/Overlay"),
            QStringLiteral("org.kde.scribbleway.OverlayController"),
            QStringLiteral("activeToolChanged"),
            this,
            SLOT(onActiveToolChanged(QString))
        );

        QDBusConnection::sessionBus().connect(
            QStringLiteral("org.kde.scribbleway"),
            QStringLiteral("/Overlay"),
            QStringLiteral("org.kde.scribbleway.OverlayController"),
            QStringLiteral("shortcutsChanged"),
            this,
            SLOT(onShortcutsChanged(QVariantList))
        );
        
        // Request initial state asynchronously or synchronously
        QDBusReply<QVariantList> shapesReply = m_dbusInterface->call(QStringLiteral("shapesMetadata"));
        if (shapesReply.isValid()) {
            m_shapesList = DBusUtils::demarshal(shapesReply.value()).toList();
            Q_EMIT shapesListChanged();
        }
        
        QDBusReply<QVariantMap> selReply = m_dbusInterface->call(QStringLiteral("getSelectionState"));
        if (selReply.isValid()) {
            onSelectionChanged(selReply.value());
        }

        QDBusReply<QString> toolReply = m_dbusInterface->call(QStringLiteral("activeTool"));
        if (toolReply.isValid()) {
            m_activeTool = toolReply.value();
            Q_EMIT activeToolChanged();
        }

        QDBusReply<QVariantList> shortcutsReply = m_dbusInterface->call(QStringLiteral("getShortcuts"));
        if (shortcutsReply.isValid()) {
            m_shortcuts = DBusUtils::demarshal(shortcutsReply.value()).toList();
            Q_EMIT shortcutsChanged();
        }
    } else {
        m_overlayConnected = false;
        delete m_dbusInterface;
        m_dbusInterface = nullptr;
    }
    
    Q_EMIT overlayConnectedChanged();
}

void AppletBackend::sendDBus(const QString &method, const QVariantList &args)
{
    if (!m_overlayConnected || !m_dbusInterface || !m_dbusInterface->isValid()) {
        return;
    }
    m_dbusInterface->asyncCallWithArgumentList(method, args);
}

void AppletBackend::onSelectionChanged(const QVariantMap &state)
{
    QVariantMap demarshalled = DBusUtils::demarshal(state).toMap();
    m_hasSelection = demarshalled[QStringLiteral("hasSelection")].toBool();
    m_selectedType = demarshalled[QStringLiteral("type")].toString();
    m_selectedColor = demarshalled[QStringLiteral("color")].toString();
    m_selectedStrokeWidth = demarshalled[QStringLiteral("strokeWidth")].toInt();
    m_selectedOpacity = demarshalled[QStringLiteral("opacity")].toDouble();
    m_selectedFontFamily = demarshalled[QStringLiteral("fontFamily")].toString();
    m_selectedFontSize = demarshalled[QStringLiteral("fontSize")].toInt();
    m_selectedLocked = demarshalled[QStringLiteral("locked")].toBool();
    m_selectedShapeIndex = demarshalled[QStringLiteral("selectedIndex")].toInt();
    m_selectedBorderRadius = demarshalled.value(QStringLiteral("borderRadius"), 8).toInt();
    Q_EMIT selectionChanged();
}

void AppletBackend::onShapesMetadataChanged(const QVariantList &metadata)
{
    m_shapesList = DBusUtils::demarshal(metadata).toList();
    Q_EMIT shapesListChanged();
}

void AppletBackend::onModeChanged(const QString &mode)
{
    if (m_currentMode != mode) {
        m_currentMode = mode;
        Q_EMIT modeChanged();
    }
}

void AppletBackend::onActiveToolChanged(const QString &tool)
{
    if (m_activeTool != tool) {
        m_activeTool = tool;
        Q_EMIT activeToolChanged();
    }
}

QStringList AppletBackend::screenNames() const
{
    QStringList names;
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        names << screen->name();
    }
    return names;
}

QString AppletBackend::targetScreen() const
{
    return m_targetScreen;
}

void AppletBackend::setTargetScreen(const QString &screenName)
{
    if (m_targetScreen != screenName) {
        m_targetScreen = screenName;
        Q_EMIT targetScreenChanged();
        sendDBus(QStringLiteral("setTargetScreen"), {screenName});
    }
}

QVariantList AppletBackend::shortcuts() const
{
    return m_shortcuts;
}

void AppletBackend::changeShortcut(const QString &actionId, const QString &shortcutString)
{
    sendDBus(QStringLiteral("changeShortcut"), {actionId, shortcutString});
}

QString AppletBackend::formatKeySequence(int key, int modifiers) const
{
    return QKeySequence(key | modifiers).toString(QKeySequence::PortableText);
}

void AppletBackend::onShortcutsChanged(const QVariantList &shortcuts)
{
    m_shortcuts = DBusUtils::demarshal(shortcuts).toList();
    Q_EMIT shortcutsChanged();
}

void AppletBackend::startDaemon()
{
    QProcess::startDetached(QStringLiteral("scribbleway-overlay"));
}
