#pragma once

#include <QWidget>

class QPlainTextEdit;

/**
 * LogPanel —— 统一日志面板。
 *
 * 订阅 AppLogger::messageLogged，按级别着色显示，
 * 方便追踪串口事件、协议收发帧与业务日志。
 */
class LogPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LogPanel(QWidget *parent = nullptr);

    // 供菜单/工具栏清空日志
    void clear();

public slots:
    void appendLog(int level, const QString &message);

private:
    QPlainTextEdit *m_view = nullptr;
};
