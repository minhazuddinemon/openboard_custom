/*
 * Copyright (C) 2015-2025 Département de l'Instruction Publique (DIP-SEM)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 */

#include "UBTheme.h"

#include <QApplication>
#include <QColor>
#include <QFile>
#include <QList>
#include <QPalette>
#include <QPointer>
#include <QWidget>

#include "core/UBSettings.h"
#include "frameworks/UBPlatformUtils.h"

namespace {

bool sInitialized = false;
QString sAppBaseStyleSheet;
QString sDockBaseStyleSheet;
QPalette sLightPalette;
QList<QPointer<QWidget>> sDockWidgets;

QString readFile(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return QString();
  return QString::fromUtf8(file.readAll());
}

const char *darkAppStyleSheet() {
  return R"QSS(
QMainWindow, QDialog { background-color: #2B2B2B; }
QWidget:enabled { color: #E6E6E6; }
QWidget:disabled { color: #8A8A8A; }

QMenu { background-color: #2B2B2B; color: #E6E6E6; border: 1px solid #444444; }
QMenu::item { background-color: transparent; color: #E6E6E6; }
QMenu::item:selected { background-color: #2d7ff9; color: #FFFFFF; }
QMenu::separator { background-color: #444444; border: 1px dotted #666666; }

QToolBar { background-color: #2B2B2B; border-bottom: 1px solid #444444; }
QToolBar::handle { background-color: #333333; }
QToolBar::separator { border: 1px dotted #555555; }
QToolBar QToolButton:pressed { background-color: #3A3A3A; }
QToolButton#ubButtonMenu:pressed, QToolButton#ubButtonMenu:checked { background-color: #3A3A3A; }

QToolButton#ubButtonGroupLeft, QToolButton#ubButtonGroupCenter, QToolButton#ubButtonGroupRight,
QToolButton#color-ubButtonGroupLeft, QToolButton#color-ubButtonGroupCenter, QToolButton#color-ubButtonGroupRight,
QToolButton#shape-ubButtonGroupLeft, QToolButton#shape-ubButtonGroupCenter, QToolButton#shape-ubButtonGroupRight,
QToolButton#shapeStyle-ubButtonGroupLeft, QToolButton#shapeStyle-ubButtonGroupCenter, QToolButton#shapeStyle-ubButtonGroupRight
{
    background: #333333;
    border: 1px solid #444444;
}
QToolButton#ubButtonGroupLeft:checked, QToolButton#ubButtonGroupCenter:checked, QToolButton#ubButtonGroupRight:checked,
QToolButton#shape-ubButtonGroupLeft:checked, QToolButton#shape-ubButtonGroupCenter:checked, QToolButton#shape-ubButtonGroupRight:checked,
QToolButton#shapeStyle-ubButtonGroupLeft:checked, QToolButton#shapeStyle-ubButtonGroupCenter:checked, QToolButton#shapeStyle-ubButtonGroupRight:checked
{
    background-color: #2d7ff9;
}

QComboBox, QPushButton, QComboBox QAbstractItemView { background-color: #3A3A3A; color: #E6E6E6; }
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox { background-color: #3A3A3A; color: #E6E6E6; border: 1px solid #555555; }
QAbstractItemView { background-color: #2B2B2B; color: #E6E6E6; selection-background-color: #2d7ff9; selection-color: #FFFFFF; }
QTreeView#UBDocumentTreeView { background-color: #2B2B2B; color: #E6E6E6; }
QHeaderView::section { background-color: #333333; color: #E6E6E6; border: 1px solid #444444; }

QProgressBar { border: 1px solid #555555; border-radius: 3px; background-color: #3A3A3A; color: #E6E6E6; }
QProgressBar::chunk { background-color: #2d7ff9; }

QTabWidget::pane { border: 1px solid #444444; }
QTabBar::tab { background-color: #333333; color: #E6E6E6; border: 1px solid #444444; }
QTabBar::tab:selected { background-color: #2d7ff9; color: #FFFFFF; }

QSlider::groove:horizontal { background-color: #555555; border: 1px solid #444444; height: 2px; }
QSlider::handle:horizontal { background-color: #8A8A8A; border: 1px solid #222222; }

QScrollBar:vertical, QScrollBar:horizontal { background-color: #2B2B2B; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background-color: #555555; border: 1px solid #333333; border-radius: 6px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background-color: #444444; border: 1px solid #333333; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background-color: transparent; }

QToolTip { background-color: #333333; color: #E6E6E6; border: 1px solid #555555; }
QStatusBar { background-color: #2B2B2B; color: #E6E6E6; }
QGroupBox { color: #E6E6E6; border: 1px solid #444444; margin-top: 6px; }
QCheckBox, QRadioButton { color: #E6E6E6; }
QColorDialog { background-color: #2B2B2B; }

/* Documents view */
QTreeView, QTreeView#UBDocumentTreeView { background-color: #2B2B2B; color: #E6E6E6; alternate-background-color: #252525; }
QTreeView::item { color: #E6E6E6; }
QTreeView::item:hover { background-color: #3A3A3A; }
QTreeView::item:selected { background-color: #2d7ff9; color: #FFFFFF; }
QSplitter::handle { background-color: #333333; }
QSplitter::handle:hover { background-color: #2d7ff9; }
QComboBox, QComboBox QAbstractItemView { background-color: #3A3A3A; color: #E6E6E6; }
QComboBox QAbstractItemView::item:selected { background-color: #2d7ff9; color: #FFFFFF; }
QGraphicsView#thumbnailWidget { background-color: #2B2B2B; }
)QSS";
}

const char *darkDockStyleSheet() {
  return R"QSS(
QWidget#DockPaletteWidgetBox,
QWidget#UBBoardThumbnailsView,
QWidget#UBLibPathViewer,
QWidget#UBLibNavigatorWidget,
QWidget#UBLibItemProperties,
QWidget#UBDownloadWidget,
QWidget#UBTeacherGuideWidget,
QWidget#UBFeaturesNavigatorWidget,
QWidget#PathList,
QWidget#UBFeaturesCentralWidget
{
    background: #2B2B2B;
    border-radius: 10px;
    border: 2px solid #333333;
    color: #E6E6E6;
}
QGraphicsView#UBBoardThumbnailsView { background: #2B2B2B; border: 2px solid #333333; }
QWidget#mAdditionalDataContainer { background: #2B2B2B; border-radius: 10px; border: 2px solid #333333; }
QWidget#UBMediaVideoContainer { border: 2px solid #333333; }
QWidget#UBLibWebView { background: #2B2B2B; border: 2px solid #333333; }
QLabel#DockPaletteWidgetTitle { color: #FFFFFF; }

QListView { background: #2B2B2B; color: #E6E6E6; border: 0px; }
QListView::item { color: #E6E6E6; }
QListView::item:selected { background: #2d7ff9; color: #FFFFFF; }

QLineEdit#UBTGLineEdit,
QLabel#UBTGMediaDropMeLabel,
QComboBox#DockPaletteWidgetComboBox,
QTextEdit#TeacherStudentBox
{
    background: #3A3A3A;
    color: #E6E6E6;
    border: 1px solid #555555;
}
QComboBox#DockPaletteWidgetComboBox:drop-down { background: #3A3A3A; }
QComboBox#DockPaletteWidgetComboBox::down-arrow { background: #3A3A3A; border: 2px solid #555555; }

QPushButton#DockPaletteWidgetButton { background-color: #3A3A3A; color: #E6E6E6; }
QPushButton#DockPaletteWidgetButton::checked { background-color: #2d7ff9; color: #FFFFFF; }

UBTGAdaptableText { background-color: #3A3A3A; color: #E6E6E6; border: 1px solid #555555; }
QLabel#UBTGEditionDocumentTitle, QLabel#UBTGPresentationDocumentTitle, QLabel#UBTGPageNumberLabel { color: #E6E6E6; }
QFrame#UBTGSeparator { background-color: #444444; }

QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #555555; border: 2px solid #333333; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { background: #444444; border: 2px solid #333333; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
)QSS";
}

QPalette darkPalette(const QPalette &light) {
  QPalette palette = light;
  const QColor window(0x2B, 0x2B, 0x2B);
  const QColor base(0x20, 0x20, 0x20);
  const QColor text(0xE6, 0xE6, 0xE6);
  const QColor disabled(0x8A, 0x8A, 0x8A);
  const QColor button(0x33, 0x33, 0x33);

  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, base);
  palette.setColor(QPalette::AlternateBase, window);
  palette.setColor(QPalette::ToolTipBase, button);
  palette.setColor(QPalette::ToolTipText, text);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::Button, button);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::BrightText, Qt::white);
  palette.setColor(QPalette::Highlight, QColor(0x2d, 0x7f, 0xf9));
  palette.setColor(QPalette::HighlightedText, Qt::white);
  palette.setColor(QPalette::Link, QColor(0x6f, 0xa8, 0xff));

  palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);

  return palette;
}

} // namespace

namespace UBTheme {

void initialize() {
  if (sInitialized)
    return;

  sLightPalette = QApplication::palette();

  // Keep the existing application style sheet (it may be absent in a dev
  // build); the dark mode is layered on top of it.
  sAppBaseStyleSheet =
      readFile(UBPlatformUtils::applicationEtcDirectory() + "/" +
               QCoreApplication::applicationName() + ".css");
  sDockBaseStyleSheet = readFile(":style.qss");

  sInitialized = true;
}

void apply() {
  initialize();

  const bool dark = UBSettings::settings()->isDarkBackground();

  QApplication::setPalette(dark ? darkPalette(sLightPalette) : sLightPalette);

  QString appStyle = sAppBaseStyleSheet;
  if (dark)
    appStyle += QString::fromUtf8(darkAppStyleSheet());
  qApp->setStyleSheet(appStyle);

  const QString dockStyle =
      sDockBaseStyleSheet +
      (dark ? QString::fromUtf8(darkDockStyleSheet()) : QString());

  for (const QPointer<QWidget> &widget : sDockWidgets) {
    if (widget)
      widget->setStyleSheet(dockStyle);
  }
}

void applyDockStyleSheet(QWidget *widget) {
  if (!widget)
    return;

  initialize();

  widget->setAttribute(Qt::WA_StyledBackground, true);

  if (!sDockWidgets.contains(widget))
    sDockWidgets.append(QPointer<QWidget>(widget));

  widget->setStyleSheet(sDockBaseStyleSheet +
                        (UBSettings::settings()->isDarkBackground()
                             ? QString::fromUtf8(darkDockStyleSheet())
                             : QString()));
}

} // namespace UBTheme
