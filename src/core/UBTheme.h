/*
 * Copyright (C) 2015-2025 Département de l'Instruction Publique (DIP-SEM)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#ifndef UBTHEME_H_
#define UBTHEME_H_

class QWidget;

// Central application theming. Applies the light look exactly as before, and a
// consistent dark scheme when the board's DarkBackground setting is enabled.
// Re-apply when the setting changes by connecting to
// UBSettings::colorContextChanged.
namespace UBTheme {
void initialize();
void apply();

// Used by the SET_STYLE_SHEET() macro for dock palettes. Applies the base
// dock style sheet plus the dark overrides when dark mode is active, and
// registers the widget so it is updated when the theme changes.
void applyDockStyleSheet(QWidget *widget);
} // namespace UBTheme

#endif /* UBTHEME_H_ */
