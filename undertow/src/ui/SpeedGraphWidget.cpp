#include "SpeedGraphWidget.h"
#include "../core/Utils.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>

SpeedGraphWidget::SpeedGraphWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(140);
}

void SpeedGraphWidget::pushSample(qint64 downBytesPerSec, qint64 upBytesPerSec)
{
    m_down.push_back(downBytesPerSec);
    m_up.push_back(upBytesPerSec);
    while (m_down.size() > kMaxSamples) m_down.pop_front();
    while (m_up.size() > kMaxSamples) m_up.pop_front();
    update();
}

void SpeedGraphWidget::clear()
{
    m_down.clear();
    m_up.clear();
    update();
}

QSize SpeedGraphWidget::minimumSizeHint() const
{
    return QSize(240, 140);
}

void SpeedGraphWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF full = rect();
    const QColor downColor(53, 132, 228);
    const QColor upColor(230, 126, 34);
    const QColor gridColor = palette().color(QPalette::Mid);

    const qreal marginLeft = 58;
    const qreal marginBottom = 4;
    const qreal marginTop = 22;
    const qreal marginRight = 10;
    QRectF plot(full.left() + marginLeft, full.top() + marginTop,
                full.width() - marginLeft - marginRight,
                full.height() - marginTop - marginBottom);

    qint64 maxVal = 1024;
    for (qint64 v : m_down) maxVal = std::max(maxVal, v);
    for (qint64 v : m_up) maxVal = std::max(maxVal, v);
    maxVal = qint64(maxVal * 1.15);

    QPen gridPen(gridColor);
    gridPen.setStyle(Qt::DotLine);
    p.setPen(gridPen);
    for (int i = 0; i <= 3; ++i) {
        qreal y = plot.top() + plot.height() * i / 3.0;
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        qint64 value = maxVal - (maxVal * i / 3);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(QRectF(full.left(), y - 8, marginLeft - 8, 16),
                   Qt::AlignRight | Qt::AlignVCenter, Utils::formatSpeed(value));
        p.setPen(gridPen);
    }

    auto drawSeries = [&](const QVector<qint64> &series, const QColor &color) {
        if (series.size() < 2) return;
        QPainterPath path;
        QPainterPath fillPath;
        const int n = series.size();
        for (int i = 0; i < n; ++i) {
            qreal x = plot.left() + plot.width() * i / qreal(kMaxSamples - 1);
            qreal ratio = maxVal > 0 ? double(series[i]) / double(maxVal) : 0.0;
            qreal y = plot.bottom() - plot.height() * ratio;
            if (i == 0) { path.moveTo(x, y); fillPath.moveTo(x, plot.bottom()); fillPath.lineTo(x, y); }
            else { path.lineTo(x, y); fillPath.lineTo(x, y); }
        }
        qreal lastX = plot.left() + plot.width() * (n - 1) / qreal(kMaxSamples - 1);
        fillPath.lineTo(lastX, plot.bottom());
        fillPath.closeSubpath();

        QColor fill = color;
        fill.setAlpha(35);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(fillPath);

        QPen linePen(color);
        linePen.setWidthF(2.0);
        p.setPen(linePen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    };

    drawSeries(m_down, downColor);
    drawSeries(m_up, upColor);

    p.setPen(palette().color(QPalette::Text));
    QFont legendFont = p.font();
    legendFont.setPointSizeF(legendFont.pointSizeF() * 0.9);
    p.setFont(legendFont);

    const qint64 curDown = m_down.isEmpty() ? 0 : m_down.last();
    const qint64 curUp = m_up.isEmpty() ? 0 : m_up.last();

    p.setBrush(downColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(full.left() + marginLeft, full.top() + 8), 4, 4);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(QRectF(full.left() + marginLeft + 10, full.top(), 160, 18), Qt::AlignVCenter,
               tr("Download: %1").arg(Utils::formatSpeed(curDown)));

    p.setBrush(upColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(full.left() + marginLeft + 190, full.top() + 8), 4, 4);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(QRectF(full.left() + marginLeft + 200, full.top(), 160, 18), Qt::AlignVCenter,
               tr("Upload: %1").arg(Utils::formatSpeed(curUp)));
}
