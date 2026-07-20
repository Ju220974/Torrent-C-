#include "Utils.h"

#include <QDateTime>
#include <QLocale>
#include <cmath>

namespace Utils {

QString formatSize(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("--");
    constexpr qint64 KB = 1024;
    constexpr qint64 MB = KB * 1024;
    constexpr qint64 GB = MB * 1024;
    constexpr qint64 TB = GB * 1024;

    if (bytes >= TB) return QStringLiteral("%1 TB").arg(bytes / double(TB), 0, 'f', 2);
    if (bytes >= GB) return QStringLiteral("%1 GB").arg(bytes / double(GB), 0, 'f', 2);
    if (bytes >= MB) return QStringLiteral("%1 MB").arg(bytes / double(MB), 0, 'f', 1);
    if (bytes >= KB) return QStringLiteral("%1 KB").arg(bytes / double(KB), 0, 'f', 0);
    return QStringLiteral("%1 B").arg(bytes);
}

QString formatSpeed(qint64 bytesPerSecond)
{
    if (bytesPerSecond <= 0) return QStringLiteral("0 B/s");
    return formatSize(bytesPerSecond) + QStringLiteral("/s");
}

QString formatEta(qint64 seconds)
{
    if (seconds < 0 || seconds > 60LL * 60 * 24 * 365) return QStringLiteral("--");
    if (seconds < 60) return QStringLiteral("%1s").arg(seconds);

    qint64 days = seconds / 86400;
    qint64 hours = (seconds % 86400) / 3600;
    qint64 minutes = (seconds % 3600) / 60;

    if (days > 0) return QStringLiteral("%1d %2h").arg(days).arg(hours);
    if (hours > 0) return QStringLiteral("%1h %2m").arg(hours).arg(minutes, 2, 10, QChar('0'));
    return QStringLiteral("%1m %2s").arg(minutes).arg(seconds % 60, 2, 10, QChar('0'));
}

QString formatRatio(qint64 uploaded, qint64 downloaded)
{
    if (downloaded <= 0) {
        return uploaded > 0 ? QStringLiteral("\u221E") : QStringLiteral("0.00");
    }
    double ratio = double(uploaded) / double(downloaded);
    return QStringLiteral("%1").arg(ratio, 0, 'f', 2);
}

QString formatTimestamp(qint64 unixSecondsSinceEpoch)
{
    if (unixSecondsSinceEpoch <= 0) return QStringLiteral("--");
    QDateTime dt = QDateTime::fromSecsSinceEpoch(unixSecondsSinceEpoch);
    QDateTime now = QDateTime::currentDateTime();
    qint64 diffSecs = dt.secsTo(now);

    if (diffSecs < 60) return QStringLiteral("agora");
    if (diffSecs < 3600) return QStringLiteral("há %1 min").arg(diffSecs / 60);
    if (dt.date() == now.date()) return dt.toString(QStringLiteral("HH:mm"));
    if (dt.date().daysTo(now.date()) < 7) return QStringLiteral("há %1 dias").arg(dt.date().daysTo(now.date()));
    return dt.toString(QStringLiteral("dd/MM/yyyy"));
}

QString formatPercent(float fraction0to1)
{
    return QStringLiteral("%1%").arg(fraction0to1 * 100.0, 0, 'f', 1);
}

} // namespace Utils
