#include "ui/LogPanel.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QVBoxLayout>

#include "core/AppLogger.h"

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *toolbar = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("运行日志"), this);
    auto *clearBtn = new QPushButton(QStringLiteral("清空"), this);
    clearBtn->setAutoDefault(false);
    toolbar->addWidget(title);
    toolbar->addStretch(1);
    toolbar->addWidget(clearBtn);
    layout->addLayout(toolbar);

    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    // 防止长时间运行内存无限增长，只保留最近 5000 行
    m_view->document()->setMaximumBlockCount(5000);

    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::TypeWriter);
    m_view->setFont(mono);
    layout->addWidget(m_view, 1);

    connect(clearBtn, &QPushButton::clicked, m_view, &QPlainTextEdit::clear);
}

void LogPanel::clear()
{
    m_view->clear();
}

void LogPanel::appendLog(int level, const QString &message)
{
    QString color;
    QString tag;
    switch (level) {
    case AppLogger::LevelDebug: color = QStringLiteral("#8a93a6"); tag = QStringLiteral("DBG"); break;
    case AppLogger::LevelInfo:  color = QStringLiteral("#c7d2e0"); tag = QStringLiteral("INF"); break;
    case AppLogger::LevelWarn:  color = QStringLiteral("#e6b85c"); tag = QStringLiteral("WRN"); break;
    default:                    color = QStringLiteral("#ff7b72"); tag = QStringLiteral("ERR"); break;
    }

    m_view->appendHtml(QStringLiteral("<span style='color:%1'><b>[%2]</b> %3</span>")
                           .arg(color, tag, message.toHtmlEscaped()));
}
