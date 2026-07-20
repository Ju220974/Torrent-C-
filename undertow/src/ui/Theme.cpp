#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QColor>

namespace Theme {

void apply(QApplication &app, Mode mode)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette pal;
    const QColor highlight(53, 132, 228);

    if (mode == Mode::Dark) {
        const QColor window(30, 32, 37);
        const QColor base(22, 24, 28);
        const QColor alternate(37, 40, 46);
        const QColor text(224, 226, 230);
        const QColor disabled(110, 112, 118);

        pal.setColor(QPalette::Window, window);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alternate);
        pal.setColor(QPalette::ToolTipBase, alternate);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::Button, alternate);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::BrightText, QColor(255, 90, 90));
        pal.setColor(QPalette::Link, highlight);
        pal.setColor(QPalette::Highlight, highlight);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::PlaceholderText, disabled);
        pal.setColor(QPalette::Mid, QColor(90, 94, 102));
        pal.setColor(QPalette::Disabled, QPalette::Text, disabled);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    } else {
        const QColor window(245, 246, 248);
        const QColor base(255, 255, 255);
        const QColor alternate(237, 239, 242);
        const QColor text(32, 34, 38);
        const QColor disabled(160, 162, 166);

        pal.setColor(QPalette::Window, window);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alternate);
        pal.setColor(QPalette::ToolTipBase, base);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::Button, alternate);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::BrightText, QColor(200, 30, 30));
        pal.setColor(QPalette::Link, highlight);
        pal.setColor(QPalette::Highlight, highlight);
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::PlaceholderText, disabled);
        pal.setColor(QPalette::Mid, QColor(200, 202, 206));
        pal.setColor(QPalette::Disabled, QPalette::Text, disabled);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    }

    app.setPalette(pal);
}

} // namespace Theme
