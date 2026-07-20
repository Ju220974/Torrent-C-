#pragma once

#include <QString>
#include <cstdint>

// Small collection of formatting helpers shared across the UI layer.
// Keeping these free functions in one place avoids duplicating
// human-readable formatting logic across models, dialogs and panels.
namespace Utils {

// "1.2 GB", "845 KB", "3 B" ...
QString formatSize(qint64 bytes);

// "1.2 MB/s", "0 B/s" ...
QString formatSpeed(qint64 bytesPerSecond);

// "12m 30s", "1h 05m", "--" when unknown/infinite
QString formatEta(qint64 seconds);

// "1.53" style ratio, "∞" when uploaded > 0 and downloaded == 0
QString formatRatio(qint64 uploaded, qint64 downloaded);

// "3 days ago", "12:45", falls back to date for older timestamps
QString formatTimestamp(qint64 unixSecondsSinceEpoch);

// Percent string with one decimal, e.g. "42.7%"
QString formatPercent(float fraction0to1);

} // namespace Utils
