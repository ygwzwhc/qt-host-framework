#include <QApplication>
#include <QMetaType>

#include "core/FrameCodec.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 高分屏适配（Qt6 默认开启，无需处理）
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("QtHost"));
    QCoreApplication::setApplicationName(QStringLiteral("QtHostFramework"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.8.0"));

    qRegisterMetaType<Frame>("Frame");

    MainWindow w;
    w.show();
    return app.exec();
}
