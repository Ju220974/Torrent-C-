#pragma once

class QApplication;

// Palette-based theming (not stylesheets) so every native Qt widget -
// including ones added later - stays consistent for free.
namespace Theme {

enum class Mode { Light, Dark };

void apply(QApplication &app, Mode mode);

} // namespace Theme
