#include <QApplication>
#include <QString>
#include <QUrl>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Undertow"));
    QApplication::setQuitOnLastWindowClosed(false);

    MainWindow window;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith(QStringLiteral("magnet:"), Qt::CaseInsensitive)) {
            window.openPathOrUri(arg);
        } else if (arg.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
            window.openPathOrUri(QUrl(arg).toLocalFile());
        } else if (arg.endsWith(QStringLiteral(".torrent"), Qt::CaseInsensitive)) {
            window.openPathOrUri(arg);
        }
    }

    window.show();
    return app.exec();
}
