#pragma once

#include <QWidget>
#include <QVector>

// Small self-contained rolling line chart. MainWindow pushes one sample
// per poll tick (global rates, or a single torrent's rates depending on
// context); the widget keeps the last m_maxSamples points and repaints.
class SpeedGraphWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpeedGraphWidget(QWidget *parent = nullptr);

    void pushSample(qint64 downBytesPerSec, qint64 upBytesPerSec);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize minimumSizeHint() const override;

private:
    QVector<qint64> m_down;
    QVector<qint64> m_up;
    static constexpr int kMaxSamples = 120;
};
