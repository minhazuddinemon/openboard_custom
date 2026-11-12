/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * Copyright (C) 2013 Open Education Foundation
 *
 * Copyright (C) 2010-2013 Groupement d'Intérêt Public pour
 * l'Education Numérique en Afrique (GIP ENA)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License,
 * with a specific linking exception for the OpenSSL project's
 * "OpenSSL" library (or with modified versions of it that use the
 * same license as the "OpenSSL" library).
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */




#include "UBStylusPalette.h"

#include <QtGui>
#include <QAction>
#include <QRadialGradient>

#include "UBMainWindow.h"

#include "core/UBApplication.h"
#include "core/UBSettings.h"
#include "core/UBApplicationController.h"
#include "core/UBShortcutManager.h"


#include "board/UBDrawingController.h"

#include "core/memcheck.h"

namespace {

QPixmap renderStrokeOnlySelectionPixmap(bool active)
{
    constexpr int iconSize = 48;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF center(iconSize / 2.0, iconSize / 2.0);
    const qreal radius = iconSize * 0.44;

    if (active) {
        // Same white radial sphere used by the standard *On.svg palette icons
        // (e.g. snapOn.svg): transparent in the middle, solid white at the rim.
        QRadialGradient halo(center, radius);
        halo.setColorAt(0.0, QColor(255, 255, 255, 0));
        halo.setColorAt(1.0, QColor(255, 255, 255, 255));

        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawEllipse(center, radius, radius);
    }

    // Off-state icons are light on the dark stylus palette; the active state
    // uses dark ink so it stays readable on the white halo.
    const QColor ink = active ? QColor(35, 31, 32) : QColor(230, 230, 230);

    painter.setPen(QPen(ink, 1.8, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(7.0, 12.0, 34.0, 24.0));

    painter.setPen(QPen(ink, 3.5, Qt::SolidLine, Qt::RoundCap));
    QPainterPath stroke;
    stroke.moveTo(12.0, 31.0);
    stroke.cubicTo(18.0, 13.0, 28.0, 34.0, 36.0, 16.0);
    painter.drawPath(stroke);

    return pixmap;
}

QIcon createStrokeOnlySelectionIcon()
{
    QIcon icon;
    icon.addPixmap(renderStrokeOnlySelectionPixmap(false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(renderStrokeOnlySelectionPixmap(true), QIcon::Normal, QIcon::On);
    return icon;
}

QPixmap renderLassoPixmap(bool active)
{
    constexpr int iconSize = 48;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF center(iconSize / 2.0, iconSize / 2.0);
    const qreal radius = iconSize * 0.44;

    if (active) {
        // Same white radial sphere as the standard *On.svg palette icons.
        QRadialGradient halo(center, radius);
        halo.setColorAt(0.0, QColor(255, 255, 255, 0));
        halo.setColorAt(1.0, QColor(255, 255, 255, 255));

        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawEllipse(center, radius, radius);
    }

    const QColor ink = active ? QColor(35, 31, 32) : QColor(230, 230, 230);
    painter.setBrush(Qt::NoBrush);

    QPainterPath loop;
    loop.moveTo(9.0, 29.0);
    loop.cubicTo(4.0, 17.0, 15.0, 6.0, 28.0, 8.0);
    loop.cubicTo(42.0, 10.0, 44.0, 25.0, 34.0, 32.0);
    loop.cubicTo(26.0, 38.0, 13.0, 37.0, 9.0, 29.0);

    painter.setPen(QPen(ink, 2.6, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(loop);

    // Lasso tail.
    painter.setPen(QPen(ink, 2.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(9.0, 29.0), QPointF(6.0, 41.0));

    return pixmap;
}

QIcon createLassoIcon()
{
    QIcon icon;
    icon.addPixmap(renderLassoPixmap(false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(renderLassoPixmap(true), QIcon::Normal, QIcon::On);
    return icon;
}

QPixmap renderShapePixmap(bool active)
{
    constexpr int iconSize = 48;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF center(iconSize / 2.0, iconSize / 2.0);
    const qreal radius = iconSize * 0.44;

    if (active) {
        // Same white radial sphere as the standard *On.svg palette icons.
        QRadialGradient halo(center, radius);
        halo.setColorAt(0.0, QColor(255, 255, 255, 0));
        halo.setColorAt(1.0, QColor(255, 255, 255, 255));

        painter.setPen(Qt::NoPen);
        painter.setBrush(halo);
        painter.drawEllipse(center, radius, radius);
    }

    const QColor ink = active ? QColor(35, 31, 32) : QColor(230, 230, 230);
    painter.setPen(QPen(ink, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    // Rounded rectangle plus a small circle, representing the shape tool.
    painter.drawRoundedRect(QRectF(6.0, 13.0, 25.0, 22.0), 3.0, 3.0);
    painter.drawEllipse(QPointF(33.0, 32.0), 7.0, 7.0);

    return pixmap;
}

QIcon createShapeIcon()
{
    QIcon icon;
    icon.addPixmap(renderShapePixmap(false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(renderShapePixmap(true), QIcon::Normal, QIcon::On);
    return icon;
}

} // namespace

UBStylusPalette::UBStylusPalette(QWidget *parent, Qt::Orientation orient)
    : UBActionPalette(Qt::TopLeftCorner, parent, orient)
    , mLastSelectedId(-1)
    , mActionLasso(0)
    , mActionLaser(0)
    , mActionShape(0)
    , mStrokeOnlySelectionAction(0)
    , mStrokeOnlySelectionButton(0)
{
    QList<QAction*> actions;

    // Store the actual tool id on each action. The palette's exclusive group
    // assigns its own sequential "id" property, which is not stable when
    // actions are inserted, so double-click handling uses "stylusTool".
    auto addToolAction = [&actions](QAction *action, int tool) {
        action->setProperty("stylusTool", tool);
        actions << action;
        return action;
    };

    addToolAction(UBApplication::mainWindow->actionPen, UBStylusTool::Pen);
    addToolAction(UBApplication::mainWindow->actionEraser, UBStylusTool::Eraser);
    addToolAction(UBApplication::mainWindow->actionMarker, UBStylusTool::Marker);
    addToolAction(UBApplication::mainWindow->actionSelector, UBStylusTool::Selector);

    mActionLasso = new QAction(tr("Lasso Selection"), this);
    mActionLasso->setCheckable(true);
    mActionLasso->setIcon(createLassoIcon());
    mActionLasso->setToolTip(tr("Lasso selection: draw a loop around items"));
    connect(mActionLasso, &QAction::triggered,
            UBDrawingController::drawingController(), &UBDrawingController::setLassoMode);
    addToolAction(mActionLasso, UBStylusTool::Lasso);

    addToolAction(UBApplication::mainWindow->actionPlay, UBStylusTool::Play);
    addToolAction(UBApplication::mainWindow->actionHand, UBStylusTool::Hand);
    addToolAction(UBApplication::mainWindow->actionZoomIn, UBStylusTool::ZoomIn);
    addToolAction(UBApplication::mainWindow->actionZoomOut, UBStylusTool::ZoomOut);

    // Virtual laser pointer: the trail is drawn as a transient, glowing stroke
    // that fades out after a short hold (see UBGraphicsLaserStroke).
    mActionLaser = new QAction(UBApplication::mainWindow->actionPointer->icon(),
                               tr("Laser Pointer"), this);
    mActionLaser->setCheckable(true);
    mActionLaser->setToolTip(tr("Virtual Laser Pointer"));
    connect(mActionLaser, &QAction::triggered,
            UBDrawingController::drawingController(), &UBDrawingController::setLaserMode);
    addToolAction(mActionLaser, UBStylusTool::Laser);

    // Shape tool replaces the Line button in the palette. The Line tool and its
    // action are kept intact for existing documents and shortcuts.
    mActionShape = new QAction(createShapeIcon(), tr("Shape"), this);
    mActionShape->setCheckable(true);
    mActionShape->setToolTip(tr("Shape tool: draw rectangles and ellipses"));
    connect(mActionShape, &QAction::triggered,
            UBDrawingController::drawingController(), &UBDrawingController::setShapeMode);
    addToolAction(mActionShape, UBStylusTool::Shape);

    addToolAction(UBApplication::mainWindow->actionText, UBStylusTool::Text);
    addToolAction(UBApplication::mainWindow->actionCapture, UBStylusTool::Capture);

    actions << UBApplication::mainWindow->actionSnap;
    UBApplication::mainWindow->actionSnap->setProperty("ungrouped", true);

    // Stroke-only selection filter, placed next to the snap toggle.
    mStrokeOnlySelectionAction = new QAction(tr("Stroke-only selection"), this);
    mStrokeOnlySelectionAction->setCheckable(true);
    mStrokeOnlySelectionAction->setIcon(createStrokeOnlySelectionIcon());
    mStrokeOnlySelectionAction->setToolTip(tr("Only select drawn strokes with the selection tool"));
    mStrokeOnlySelectionAction->setProperty("ungrouped", true);
    mStrokeOnlySelectionAction->setChecked(
                UBSettings::settings()->boardStrokeOnlySelection->get().toBool());
    connect(mStrokeOnlySelectionAction, &QAction::toggled,
            this, &UBStylusPalette::strokeOnlySelectionToggled);
    actions << mStrokeOnlySelectionAction;

    setActions(actions);

    // The button is created by UBActionPalette exactly like the snap/magnet
    // button; its checked appearance comes from the QIcon::On pixmap above.
    mStrokeOnlySelectionButton = getButtonFromAction(mStrokeOnlySelectionAction);
    if (mStrokeOnlySelectionButton) {
        mStrokeOnlySelectionButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        updateStrokeOnlySelectionButton(mStrokeOnlySelectionAction->isChecked());
    }

    // Keep the visual state in sync when another palette/settings page changes
    // the persisted preference.
    connect(UBSettings::settings()->boardStrokeOnlySelection, SIGNAL(changed(QVariant)),
            this, SLOT(strokeOnlySelectionSettingChanged(QVariant)));

    // Keep the lasso button checked state in sync with the actual tool.
    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int,int)),
            this, SLOT(stylusToolChanged(int)));
    mActionLasso->setChecked(
                UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Lasso);
    mActionLaser->setChecked(
                UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Laser);
    mActionShape->setChecked(
                UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Shape);

    setButtonIconSize(QSize(42, 42));
    groupActions();

    UBShortcutManager::shortcutManager()->addActionGroup(mActionGroup);

    adjustSizeAndPosition();

    initPosition();

    foreach(const UBActionPaletteButton* button, mButtons)
    {
        connect(button, SIGNAL(doubleClicked()), this, SLOT(stylusToolDoubleClicked()));
    }

}

void UBStylusPalette::initPosition()
{
    QWidget* pParentW = parentWidget();
    if(!pParentW) return ;

    mCustomPosition = true;

    QPoint pos;
    int parentWidth = pParentW->width();
    int parentHeight = pParentW->height();

    if(UBSettings::settings()->appToolBarOrientationVertical->get().toBool()){
        int posX = border();
        int posY = (parentHeight / 2) - (height() / 2);
        pos.setX(posX);
        pos.setY(posY);
    }
    else {
        int posX = (parentWidth / 2) - (width() / 2);
        int posY = parentHeight - border() - height();
        pos.setX(posX);
        pos.setY(posY);
    }
    moveInsideParent(pos);
}

UBStylusPalette::~UBStylusPalette()
{
    if (mActionGroup)
    {
        UBShortcutManager::shortcutManager()->removeActionGroup(mActionGroup);
    }
}

void UBStylusPalette::stylusToolDoubleClicked()
{
    QAction *checkedAction = mActionGroup ? mActionGroup->checkedAction() : 0;
    if (!checkedAction)
        return;

    emit stylusToolDoubleClicked(checkedAction->property("stylusTool").toInt());
}

void UBStylusPalette::stylusToolChanged(int tool)
{
    if (mActionLasso)
        mActionLasso->setChecked(tool == UBStylusTool::Lasso);

    if (mActionLaser)
        mActionLaser->setChecked(tool == UBStylusTool::Laser);

    if (mActionShape)
        mActionShape->setChecked(tool == UBStylusTool::Shape);
}

void UBStylusPalette::strokeOnlySelectionToggled(bool enabled)
{
    updateStrokeOnlySelectionButton(enabled);
    UBSettings::settings()->boardStrokeOnlySelection->set(enabled);
}

void UBStylusPalette::strokeOnlySelectionSettingChanged(QVariant value)
{
    const bool enabled = value.toBool();

    // Avoid feeding the value back into UBSettings through the toggled signal.
    if (mStrokeOnlySelectionAction && mStrokeOnlySelectionAction->isChecked() != enabled) {
        const bool wasBlocked = mStrokeOnlySelectionAction->blockSignals(true);
        mStrokeOnlySelectionAction->setChecked(enabled);
        mStrokeOnlySelectionAction->blockSignals(wasBlocked);
    }

    updateStrokeOnlySelectionButton(enabled);
}

void UBStylusPalette::updateStrokeOnlySelectionButton(bool enabled)
{
    if (!mStrokeOnlySelectionButton)
        return;

    // Match the standard palette buttons: the checked state selects the
    // QIcon::On pixmap (white circular halo) instead of a rectangular style.
    mStrokeOnlySelectionButton->setCheckable(true);
    mStrokeOnlySelectionButton->setChecked(enabled);
    mStrokeOnlySelectionButton->update();
}


