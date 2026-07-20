#pragma once

#include <QStyledItemDelegate>

// Paints the "Progresso" column as an inline progress bar with a
// percentage label instead of the raw float Qt would otherwise show.
class ProgressDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ProgressDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};
