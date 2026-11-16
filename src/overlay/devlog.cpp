#include "devlog.h"

#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QMutex>
#include <QScreen>

#include <cstdio>

// QtWarningMsg: info and debug lines are dropped until install() enables them.
Q_LOGGING_CATEGORY(lcDev, "scribbleway.dev", QtWarningMsg)

namespace {
QMutex s_mutex;
QtMessageHandler s_previous = nullptr;
QString s_dir;
QFile s_file;
QDate s_fileDate;

// The app runs for days from autostart, so the file is switched (and old ones pruned)
// whenever the date changes, not only at startup.
void openFor(const QDate &date)
{
    s_file.close();
    s_fileDate = date;
    s_file.setFileName(s_dir + QLatin1Char('/') + date.toString(Qt::ISODate) + QStringLiteral(".log"));
    // stderr, not qWarning(): this runs inside the message handler.
    if (!s_file.open(QIODevice::Append | QIODevice::Text)) {
        std::fprintf(stderr, "scribbleway: cannot write dev log %s\n", qPrintable(s_file.fileName()));
    }
    DevLog::prune(QDir(s_dir), date);
}

void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        // Nothing in here may log: the mutex is not recursive.
        QMutexLocker lock(&s_mutex);
        const QDateTime now = QDateTime::currentDateTime();
        if (now.date() != s_fileDate) openFor(now.date());
        if (s_file.isOpen()) {
            static const char *const kLevels[] = {"debug", "warning", "critical", "fatal", "info"};
            QByteArray line = now.toString(QStringLiteral("HH:mm:ss.zzz ")).toUtf8()
                + QByteArray(kLevels[type]).leftJustified(9);
            // Tag only foreign categories (layershellqt, qt.*); ours would be on every line.
            if (context.category && qstrcmp(context.category, "default") != 0
                && qstrcmp(context.category, "scribbleway.dev") != 0) {
                line += '[' + QByteArray(context.category) + "] ";
            }
            s_file.write(line + message.toUtf8() + '\n');
            // Flushed per line so the last lines before a crash make it to disk.
            s_file.flush();
        }
    }
    if (s_previous) s_previous(type, context, message);
}
}

void DevLog::install(const QString &dir)
{
    s_dir = dir;
    QDir().mkpath(dir);
    QLoggingCategory::setFilterRules(QStringLiteral("scribbleway.dev.info=true"));
    s_previous = qInstallMessageHandler(handler);
}

void DevLog::prune(QDir dir, const QDate &today, int keepDays)
{
    const QStringList files = dir.entryList({QStringLiteral("*.log")}, QDir::Files);
    for (const QString &name : files) {
        const QDate date = QDate::fromString(name.chopped(4), Qt::ISODate);
        if (date.isValid() && date.daysTo(today) > keepDays) dir.remove(name);
    }
}

void DevLog::logScreens(const char *why)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *primary = QGuiApplication::primaryScreen();
    qCInfo(lcDev).noquote() << why << "-" << screens.size() << "screen(s), primary"
                            << (primary ? primary->name() : QStringLiteral("(none)"));
    for (QScreen *s : screens) {
        qCInfo(lcDev).noquote() << "  screen" << s->name() << s->model() << s->geometry()
                                << "scale" << s->devicePixelRatio();
    }
}
