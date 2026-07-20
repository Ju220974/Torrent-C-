#include "ProgressDelegate.h"

#include <QPainter>
#include <QApplication>

ProgressDelegate::ProgressDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void ProgressDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    const float progress = index.data(Qt::UserRole).toFloat();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    }

    QRect barRect = option.rect.adjusted(6, option.rect.height() / 2 - 8, -6, -(option.rect.height() / 2 - 8));

    QColor trackColor = option.palette.color(QPalette::Mid);
    trackColor.setAlpha(90);
    painter->setPen(Qt::NoPen);
    painter->setBrush(trackColor);
    painter->drawRoundedRect(barRect, 6, 6);

    QRect fillRect = barRect;
    fillRect.setWidth(int(barRect.width() * qBound(0.0f, progress, 1.0f)));
    if (fillRect.width() > 0) {
        QColor fillColor = option.palette.color(QPalette::Highlight);
        painter->setBrush(fillColor);
        painter->drawRoundedRect(fillRect, 6, 6);
    }

    painter->setPen(option.state & QStyle::State_Selected
        ? option.palette.color(QPalette::HighlightedText)
        : option.palette.color(QPalette::Text));
    const QString label = QStringLiteral("%1%").arg(progress * 100.0, 0, 'f', 1);
    painter->drawText(option.rect, Qt::AlignCenter, label);

    painter->restore();
}

QSize ProgressDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QSize(option.rect.width(), 26);
}
