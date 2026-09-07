#include "core/AppLogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

AppLogger::AppLogger(QObject *parent)
    : QObject(parent)
{
    m_logFilePath = logFilePath();
    QDir().mkpath(QFileInfo(m_logFilePath).absolutePath());
}

AppLogger &AppLogger::instance()
{
    static AppLogger logger;
    return logger;
}

QString AppLogger::levelText(int level)
{
    switch (level) {
    case LevelDebug: return QStringLiteral("DBG");
    case LevelInfo:  return QStringLiteral("INF");
    case LevelWarn:  return QStringLiteral("WRN");
    case LevelError: return QStringLiteral("ERR");
    default:         return QStringLiteral("???");
    }
}

QString AppLogger::logFilePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QStringLiteral(".");
    return base + QStringLiteral("/logs/app.log");
}

void AppLogger::log(int level, const QString &message)
{
    const QString time = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString line = QStringLiteral("[%1] [%2] %3")
                             .arg(time, levelText(level), message);
    emit messageLogged(level, message);
    writeFile(line);
}

void AppLogger::debug(const QString &message) { log(LevelDebug, message); }
void AppLogger::info(const QString &message)  { log(LevelInfo, message); }
void AppLogger::warn(const QString &message)  { log(LevelWarn, message); }
void AppLogger::error(const QString &message) { log(LevelError, message); }

void AppLogger::writeFile(const QString &line)
{
    QFile file(m_logFilePath);
    if (file.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&file);
        ts << line << QLatin1Char('\n');
    }
}
