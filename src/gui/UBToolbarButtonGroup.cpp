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




#include "UBToolbarButtonGroup.h"

#include <QtGui>
#include <QLayout>
#include <QStyleOption>

#include "core/UBApplication.h"
#include "core/UBSettings.h"

#include "board/UBDrawingController.h"

#include "core/memcheck.h"

UBToolbarButtonGroup::UBToolbarButtonGroup(QToolBar *toolBar, const QList<QAction*> &actions, QString objectNameprefix)
    : QWidget(toolBar)
    , mActions(actions)
    , mCurrentIndex(-1)
    , mDisplayLabel(true)
    , mActionGroup(0)
{
    Q_ASSERT(actions.size() > 0);

    mToolButton = qobject_cast<QToolButton*>(toolBar->layout()->itemAt(0)->widget());
    Q_ASSERT(mToolButton);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    QHBoxLayout *horizontalLayout = new QHBoxLayout();
    horizontalLayout->setSpacing(0);
    verticalLayout->addStretch();
    verticalLayout->addLayout(horizontalLayout);
    verticalLayout->addStretch();

    mActionGroup = new QActionGroup(this);
    mActionGroup->setExclusive(true);

    QSize buttonSize;

    int i = 0;

    foreach(QAction *action, actions)
    {
        // Only checkable actions are part of the exclusive color group. The
        // custom-color picker is a plain action button at the end of the group.
        if (action->isCheckable())
            mActionGroup->addAction(action);

        QToolButton *button = new QToolButton(this);
        mButtons.append(button);
        button->setDefaultAction(action);
        button->setCheckable(action->isCheckable());

        if(i == 0)
        {
            objectNameprefix.length() > 0 ?
                button->setObjectName(objectNameprefix + "-ubButtonGroupLeft")
                : button->setObjectName("ubButtonGroupLeft");
        }
        else if (i == actions.size() - 1)
        {
            objectNameprefix.length() > 0 ?
                button->setObjectName(objectNameprefix + "-ubButtonGroupRight")
                : button->setObjectName("ubButtonGroupRight");

        }
        else
        {
            objectNameprefix.length() > 0 ?
                button->setObjectName(objectNameprefix + "-ubButtonGroupCenter")
                : button->setObjectName("ubButtonGroupCenter");
        }

        if (action->isCheckable())
            connect(button, SIGNAL(triggered(QAction*)), this, SLOT(selected(QAction*)));

        horizontalLayout->addWidget(button);
        mLabel = action->text();
        buttonSize = button->sizeHint();
        i++;
    }

    mObjectNamePrefix = objectNameprefix;

    // The colour swatch group signals its selection by drawing the selected
    // swatch larger (see swatchIcon); the other top-bar groups get an accent
    // background. The desktop palettes keep their look.
    if (objectNameprefix != "desktop" && objectNameprefix != "color")
    {
        setStyleSheet("QToolButton:checked {"
                      " background-color: #2d7ff9;"
                      " border-radius: 6px;"
                      " }");
    }
}

UBToolbarButtonGroup::~UBToolbarButtonGroup()
{
    // NOOP
}

void UBToolbarButtonGroup::setLabel(const QString& label)
{
    mLabel = label;
}

void UBToolbarButtonGroup::setIcon(const QIcon &icon, int index)
{
    Q_ASSERT(index < mActions.size());

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    foreach(QObject *widget, mActions.at(index)->associatedObjects())
#else
    foreach(QWidget *widget, mActions.at(index)->associatedWidgets())
#endif
    {
        QToolButton *button = qobject_cast<QToolButton*>(widget);
        if (button)
        {
            // change icon at action, so that updates of action do not overwrite the icon
            for (QAction* action : button->actions())
            {
                action->setIcon(icon);
            }
        }
    }
}

void UBToolbarButtonGroup::setColor(const QColor &color, int index)
{
    // One drawing routine for every swatch group. The fixed colour actions
    // are shared between the board and the desktop palettes, so using the
    // same routine everywhere prevents one group from overwriting another
    // group's icons with a different style.
    while (mColors.size() <= index)
        mColors.append(QColor());
    mColors[index] = color;
    setIcon(swatchIcon(color, index == mCurrentIndex), index);
}

QIcon UBToolbarButtonGroup::swatchIcon(const QColor &color, bool selected) const
{
    constexpr int iconSize = 32;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // One shape for every swatch: a small square with a slight, fixed
    // corner rounding. Only the size differs: the unselected swatch is
    // drawn at the original size with transparent margin, the selected one
    // grows by about 3 px on each side and fills the cell.
    const qreal cell = selected ? 30.0 : 22.0;
    const QRectF rect((iconSize - cell) / 2.0, (iconSize - cell) / 2.0,
                      cell, cell);

    painter.setBrush(color);
    if (selected)
    {
        // Border in a shade of the swatch itself, chosen to stay visible on
        // every colour, including white and yellow.
        const bool lightSwatch = qGray(color.rgb()) >= 200
                || color.lightness() >= 200;
        const QColor border = lightSwatch ? color.darker(180)
                                          : color.lighter(160);
        painter.setPen(QPen(border, 2.0));
    }
    else
    {
        painter.setPen(Qt::NoPen);
    }

    // Same corner rounding for selected and unselected (not scaled with
    // the size), so the selected swatch is just a bigger copy.
    constexpr qreal cornerRadius = 3.0;
    painter.drawRoundedRect(rect, cornerRadius, cornerRadius);
    painter.end();
    return QIcon(pixmap);
}

void UBToolbarButtonGroup::refreshColorIcons()
{
    for (int i = 0; i < mColors.size() && i < mButtons.size(); ++i)
    {
        if (i < mActions.size() && mActions.at(i)->isCheckable())
            setIcon(swatchIcon(mColors.at(i), i == mCurrentIndex), i);
    }
}

void UBToolbarButtonGroup::selected(QAction *action)
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    foreach(QObject *widget, action->associatedObjects())
#else
    foreach(QWidget *widget, action->associatedWidgets())
#endif
    {
        QToolButton *button = qobject_cast<QToolButton*>(widget);
        if (button)
        {
            int i = 0;
            foreach(QAction *eachAction, mActions)
            {
                if (eachAction == action)
                {
                    setCurrentIndex(i);
                    emit activated(i);
                    break;
                }
                i++;
            }
        }
    }
}

int UBToolbarButtonGroup::currentIndex() const
{
    return mCurrentIndex;
}

void UBToolbarButtonGroup::setCurrentIndex(int index)
{
    Q_ASSERT(index < mButtons.size());

    if (index != mCurrentIndex)
    {
        for(int i = 0; i < mButtons.size(); i++)
        {
            mButtons.at(i)->setChecked(i == index);
        }
        mCurrentIndex = index;
        refreshColorIcons();
        emit currentIndexChanged(index);
    }
}

void UBToolbarButtonGroup::clearSelection()
{
    if (mCurrentIndex == -1)
        return;

    // Temporarily disable the exclusive QActionGroup so the currently checked
    // button can be unchecked programmatically.
    const bool exclusive = mActionGroup->isExclusive();
    mActionGroup->setExclusive(false);

    for (QToolButton *button : mButtons)
        button->setChecked(false);

    mActionGroup->setExclusive(exclusive);

    mCurrentIndex = -1;
    refreshColorIcons();
    emit currentIndexChanged(-1);
}


void UBToolbarButtonGroup::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QStyleOptionToolButton option;
    QPixmap emptyPixmap(32, 32);
    emptyPixmap.fill(Qt::transparent);
    QIcon emptyIcon(emptyPixmap);

    option.initFrom(mToolButton);

    option.text = mLabel;
    option.font = mToolButton->font();

    int pointSize = mToolButton->font().pointSize();
    if (pointSize > 0)
        option.font.setPointSize(pointSize);
    else
    {
        int pixelSize = mToolButton->font().pixelSize();
        if (pixelSize > 0)
            option.font.setPixelSize(pixelSize);
    }

    option.rect = rect();
    option.icon = emptyIcon; // non null icon is required for style()->drawControl(QStyle::CE_ToolButtonLabel, ...) to work correctly
    option.iconSize = emptyPixmap.size();
    option.toolButtonStyle = mDisplayLabel ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly;

    style()->drawControl(QStyle::CE_ToolButtonLabel, &option, &painter, this);
}


void UBToolbarButtonGroup::colorPaletteChanged()
{
    bool isDarkBackground = UBSettings::settings()->isDarkBackground();

    QList<QColor> colors;

    if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Pen
        || UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Line
        || UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Laser
        || UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Shape)
    {
        colors = UBSettings::settings()->penColors(isDarkBackground);
    }
    else if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Marker)
    {
        colors = UBSettings::settings()->markerColors(isDarkBackground);
    }

    for (int i = 0; i < mButtons.size() && i < colors.size(); i++)
    {
        if (mActions.at(i)->isCheckable())
            setColor(colors.at(i), i);
    }
}

void UBToolbarButtonGroup::displayText(QVariant display)
{
    mDisplayLabel = display.toBool();
    QVBoxLayout* verticalLayout = (QVBoxLayout*)layout();
    // Keep the button row vertically centered whether or not the group label
    // is painted underneath the icons (labels are hidden in compact mode).
    verticalLayout->setStretch(0, 1);
    verticalLayout->setStretch(2, 1);
    update();
}
