#pragma once

#include <QDate>
#include <QDir>
#include <QLoggingCategory>

// Dev-mode lines (qCInfo(lcDev)). Off unless the app runs with --dev, so normal runs
// keep the journal as quiet as before.
Q_DECLARE_LOGGING_CATEGORY(lcDev)

namespace DevLog {

// Turns lcDev on and copies every Qt and QML message into one file per day in `dir`.
void install(const QString &dir);

// Deletes the day files in `dir` dated more than `keepDays` before `today`.
void prune(QDir dir, const QDate &today, int keepDays = 7);

// Logs the screen count, the primary screen, and each screen's geometry and scale.
void logScreens(const char *why);

// Writes a numbered marker line so a bug report can say "after mark 3".
void mark();

}
