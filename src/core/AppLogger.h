#pragma once

#include <QObject>
#include <QString>

/**
 * 应用级日志器（全局单例）。
 *
 * 职责：
 *  1. 把带时间戳、级别的日志通过信号 messageLogged() 广播给 UI（日志面板）；
 *  2. 同时以追加方式落盘到日志文件（AppData 目录下 logs/app.log）。
 *
 * UI/业务代码不要直接依赖 QFile/QTextStream，统一走本类，
 * 这样将来换成 log4qt / spdlog 只改一处。
 */
class AppLogger : public QObject
{
    Q_OBJECT
public:
    enum Level {
        LevelDebug = 0,
        LevelInfo = 1,
        LevelWarn = 2,
        LevelError = 3
    };

    static AppLogger &instance();
    static QString levelText(int level);

    void log(int level, const QString &message);
    void debug(const QString &message);
    void info(const QString &message);
    void warn(const QString &message);
    void error(const QString &message);

signals:
    // level 取 Level 枚举值
    void messageLogged(int level, const QString &message);

private:
    explicit AppLogger(QObject *parent = nullptr);
    Q_DISABLE_COPY(AppLogger)

    QString logFilePath() const;
    void writeFile(const QString &line);

    QString m_logFilePath;
};
