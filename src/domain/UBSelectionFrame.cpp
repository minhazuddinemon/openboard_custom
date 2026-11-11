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


#include "UBSelectionFrame.h"

#include <QtGui>
#include <QtSvg>
#include <QSet>
#include <QMenu>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QToolButton>
#include <QColorDialog>
#include <QCursor>
#include <QStyle>

#include "domain/UBItem.h"
#include "domain/UBGraphicsItemZLevelUndoCommand.h"
#include "domain/UBGraphicsItemColorUndoCommand.h"
#include "domain/UBGraphicsGroupContainerItem.h"
#include "domain/UBGraphicsPolygonItem.h"
#include "domain/UBGraphicsStrokesGroup.h"
#include "domain/UBGraphicsScene.h"
#include "board/UBBoardController.h"
#include "core/UBSettings.h"
#include "core/UBApplication.h"
#include "gui/UBResources.h"
#include "gui/UBMainWindow.h"
#include "core/UBApplication.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"

namespace {

// Collects the stroke/vector items contained in item. Group containers are
// traversed so that recoloring a grouped selection also affects the strokes
// stored inside the group.
void collectStrokeItems(QGraphicsItem *item, QList<QGraphicsItem *> &result)
{
    if (!item)
        return;

    const int itemType = item->type();

    if (itemType == UBGraphicsStrokesGroup::Type
            || itemType == UBGraphicsPolygonItem::Type)
    {
        result << item;
        return;
    }

    if (itemType == UBGraphicsGroupContainerItem::Type)
    {
        foreach (QGraphicsItem *child, item->childItems())
            collectStrokeItems(child, result);
    }
}

} // namespace

UBSelectionFrame::UBSelectionFrame()
    : mThickness(UBSettings::settings()->objectFrameWidth)
    , mAntiscaleRatio(1.0)
    , mCursorRotationAngle(0)
    , mItemRotationAngle(0)
    , mCurrentScaleTool(NoScale)
    , mDeleteButton(0)
    , mDuplicateButton(0)
    , mZOrderUpButton(0)
    , mZOrderDownButton(0)
    , mGroupButton(0)
    , mChangeColorButton(0)
    , mRotateButton(0)
    , mBottomRightResizeGripSvgItem(0)
    , mBottomResizeGripSvgItem(0)
    , mLeftResizeGripSvgItem(0)
    , mRightResizeGripSvgItem(0)
    , mTopResizeGripSvgItem(0)
    , mBottomRightResizeGrip(0)
    , mBottomResizeGrip(0)
    , mLeftResizeGrip(0)
    , mRightResizeGrip(0)
    , mTopResizeGrip(0)
{
    setLocalBrush(QBrush(UBSettings::paletteColor));
    setPen(Qt::NoPen);
    setData(UBGraphicsItemData::ItemLayerType, QVariant(UBItemLayerType::Control));
    setData(UBGraphicsItemData::itemLayerType, QVariant(itemLayerType::SelectionFrame)); //Necessary to set if we want z value to be assigned correctly
    setFlags(QGraphicsItem::ItemSendsGeometryChanges | QGraphicsItem::ItemIsSelectable | ItemIsMovable);

    connect(UBApplication::boardController, SIGNAL(zoomChanged(qreal)), this, SLOT(onZoomChanged(qreal)));

    // Resize handles. They are plain visual/hit-test children of the frame;
    // the frame itself handles the drag in UBSelectionFrame::mouseMoveEvent().
    const struct {
        const char *svg;
        QGraphicsSvgItem **svgItem;
        QGraphicsRectItem **hitItem;
        Qt::CursorShape cursor;
    } grips[] = {
        { ":/images/resize.svg",       &mBottomRightResizeGripSvgItem, &mBottomRightResizeGrip, Qt::SizeFDiagCursor },
        { ":/images/resizeBottom.svg", &mBottomResizeGripSvgItem,      &mBottomResizeGrip,      Qt::SizeVerCursor   },
        { ":/images/resizeLeft.svg",   &mLeftResizeGripSvgItem,        &mLeftResizeGrip,        Qt::SizeHorCursor   },
        { ":/images/resizeRight.svg",  &mRightResizeGripSvgItem,       &mRightResizeGrip,       Qt::SizeHorCursor   },
        { ":/images/resizeTop.svg",    &mTopResizeGripSvgItem,         &mTopResizeGrip,         Qt::SizeVerCursor   },
    };

    for (const auto &grip : grips)
    {
        QGraphicsSvgItem *svgItem = new QGraphicsSvgItem(grip.svg, this);
        svgItem->setAcceptedMouseButtons(Qt::NoButton);
        svgItem->setZValue(2);
        *grip.svgItem = svgItem;

        QGraphicsRectItem *hitItem = new QGraphicsRectItem(this);
        hitItem->setPen(Qt::NoPen);
        hitItem->setBrush(Qt::NoBrush);
        hitItem->setCursor(QCursor(grip.cursor));
        hitItem->setZValue(3);
        *grip.hitItem = hitItem;
    }

    onZoomChanged(UBApplication::boardController->currentZoom());
    placeResizeGrips();
}

void UBSelectionFrame::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    QPainterPath path;
    QRectF shRect = option->rect;
    path.addRoundedRect(shRect, adjThickness() / 2, adjThickness() / 2);

    if (rect().width() > 1 && rect().height() > 1) {
        QPainterPath extruded;
        extruded.addRect(shRect.adjusted(adjThickness(), adjThickness(), (adjThickness() * -1), (adjThickness() * -1)));
        path = path.subtracted(extruded);
    }

    painter->fillPath(path, mLocalBrush);
}

QRectF UBSelectionFrame::boundingRect() const
{
    return rect().adjusted(-adjThickness(), -adjThickness(), adjThickness(), adjThickness());
}

QPainterPath UBSelectionFrame::shape() const
{
    QPainterPath resShape;
    QRectF ownRect = rect();
    QRectF shRect = ownRect.adjusted(-adjThickness(), -adjThickness(), adjThickness(), adjThickness());
    resShape.addRoundedRect(shRect, adjThickness() / 2, adjThickness() / 2);

    if (rect().width() > 1 && rect().height() > 1) {
        QPainterPath extruded;
        extruded.addRect(ownRect);
        resShape = resShape.subtracted(extruded);
    }

    return resShape;
}

void UBSelectionFrame::setEnclosedItems(const QList<QGraphicsItem*> pGraphicsItems)
{
    // Hide buttons from the previous selection; otherwise e.g. the group
    // button remains visible after reducing the selection to a single item.
    clearButtons();

    mButtons.clear();
    mButtons.append(mDeleteButton);
    mCursorRotationAngle = 0;
    mItemRotationAngle = 0;

    QRegion resultRegion;
    UBGraphicsFlags resultFlags = GF_NONE;
    mEnclosedtems.clear();

    // If at least one of the enclosed items is locked, the entire selection is
    // considered to be locked.
    mIsLocked = false;

    foreach (QGraphicsItem *nextItem, pGraphicsItems) {
        UBGraphicsItemDelegate *nextDelegate = UBGraphicsItem::Delegate(nextItem);
        if (nextDelegate) {
            mIsLocked = (mIsLocked || nextDelegate->isLocked());
            mEnclosedtems.append(nextDelegate);
            resultRegion |= nextItem->boundingRegion(nextItem->sceneTransform());
            resultFlags |= nextDelegate->ubflags();
        }
    }

    QRectF resultRect = resultRegion.boundingRect();
    setRect(resultRect);

    mButtons = buttonsForFlags(resultFlags);
    placeButtons();

    if (resultRect.isEmpty()) {
        hide();
    }

    if (mIsLocked) {
        QColor baseColor = UBSettings::paletteColor;
        baseColor.setAlphaF(baseColor.alphaF() / 3);
        setLocalBrush(QBrush(baseColor));
    }
    else
        setLocalBrush(QBrush(UBSettings::paletteColor));
}

void UBSelectionFrame::updateRect()
{
    QRectF result;

    foreach (UBGraphicsItemDelegate *curDelegateItem, mEnclosedtems) {
        result |= curDelegateItem->delegated()->sceneBoundingRect();
    }

    setRect(result);

    placeButtons();

    if (result.isEmpty()) {
        setVisible(false);
    }
}

void UBSelectionFrame::updateScale()
{
    setScale(-UBApplication::boardController->currentZoom());
}

Qt::CursorShape UBSelectionFrame::hoverCursor(const QPointF &scenePos) const
{
    if (mIsLocked || mEnclosedtems.isEmpty())
        return Qt::CrossCursor;

    auto contains = [&scenePos](QGraphicsRectItem *grip) {
        return grip && grip->isVisible()
                && grip->contains(grip->mapFromScene(scenePos));
    };

    if (contains(mBottomRightResizeGrip))
        return Qt::SizeFDiagCursor;
    if (contains(mRightResizeGrip) || contains(mLeftResizeGrip))
        return Qt::SizeHorCursor;
    if (contains(mTopResizeGrip) || contains(mBottomResizeGrip))
        return Qt::SizeVerCursor;

    foreach (DelegateButton *button, mButtons) {
        if (button && button->isVisible()
                && button->contains(button->mapFromScene(scenePos)))
            return Qt::PointingHandCursor;
    }

    // Dragging the frame ring moves the whole selection.
    if (shape().contains(mapFromScene(scenePos)))
        return Qt::SizeAllCursor;

    return Qt::CrossCursor;
}

void UBSelectionFrame::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    mPressedPos = mLastMovedPos = event->pos();
    mStartingBounds = rect();
    mLastTranslateOffset = QPointF();
    mCursorRotationAngle = 0;
    mItemRotationAngle = 0;

    // Snapshot every enclosed item before any modification so that move,
    // rotate and scale operations can be restored by a single undo step.
    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        d->startUndoStep();
    }

    mCurrentScaleTool = scaleToolFromPos(event->scenePos());

    if (mCurrentScaleTool != NoScale) {
        mOperationMode = om_scaling;
        beginGroupScale(event->scenePos());
    } else if (scene()->itemAt(event->scenePos(), transform()) == mRotateButton) {
        mOperationMode = om_rotating;
    } else {
        mOperationMode = om_moving;
    }
}

void UBSelectionFrame::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mIsLocked)
        return;

    if (mOperationMode == om_scaling) {
        applyGroupScale(event->scenePos());
        updateRect();
        return;
    }

    QPointF dp = event->pos() - mPressedPos;
    QPointF rotCenter = mapToScene(rect().center());

    QLineF startLine(sceneBoundingRect().center(), event->lastScenePos());
    QLineF currentLine(sceneBoundingRect().center(), event->scenePos());
    qreal dAngle = startLine.angleTo(currentLine);
    mCursorRotationAngle = std::fmod(mCursorRotationAngle - dAngle + 360., 360.);

    // snap to grid and angle
    const auto* ubscene = dynamic_cast<UBGraphicsScene*>(scene());

    if (ubscene && ubscene->isSnapping())
    {
        if (mOperationMode == om_moving)
        {
            Qt::Corner corner;
            QRectF movedBounds = mStartingBounds.translated(dp);
            QPointF snapVector = ubscene->snap(movedBounds, &corner);
            dp += snapVector;
        }
        else if (mOperationMode == om_rotating)
        {
            qreal step = UBSettings::settings()->rotationAngleStep->get().toReal();
            qreal snappedAngle = qRound(mCursorRotationAngle / step) * step;
            dAngle = mItemRotationAngle - snappedAngle;
            mItemRotationAngle = std::fmod(snappedAngle + 360., 360.);
        }
    }
    else
    {
        mItemRotationAngle = mCursorRotationAngle;
    }

    if (mOperationMode == om_rotating)
    {
        UBApplication::boardController->setCursorFromAngle(mItemRotationAngle);
    }

    foreach (UBGraphicsItemDelegate *curDelegate, mEnclosedtems)
    {
        switch (static_cast<int>(mOperationMode)) {
        case om_moving : {
            QGraphicsItem *item = curDelegate->delegated();
            QTransform ownTransform = item->transform();
            QTransform dTransform(
                        ownTransform.m11()
                        , ownTransform.m12()
                        , ownTransform.m13()

                        , ownTransform.m21()
                        , ownTransform.m22()
                        , ownTransform.m23()

                        , ownTransform.m31() + (dp - mLastTranslateOffset).x()
                        , ownTransform.m32() + (dp - mLastTranslateOffset).y()
                        , ownTransform.m33()
                        );

            item->setTransform(dTransform);
        } break;

        case om_rotating : {
            QGraphicsItem *item = curDelegate->delegated();
            QTransform ownTransform = item->transform();

            QPointF nextRotCenter = item->mapFromScene(rotCenter);

            qreal cntrX = nextRotCenter.x();
            qreal cntrY = nextRotCenter.y();

            ownTransform.translate(cntrX, cntrY);
            ownTransform.rotate(-dAngle);
            ownTransform.translate(-cntrX, -cntrY);

            item->update();
            item->setTransform(ownTransform, false);

        } break;

        }
    }

    updateRect();
    mLastMovedPos = event->pos();
    mLastTranslateOffset = dp;
}

void UBSelectionFrame::mouseReleaseEvent(QGraphicsSceneMouseEvent */*event*/)
{
    mPressedPos = mLastMovedPos = mLastTranslateOffset = QPointF();

    if (mOperationMode == om_moving || mOperationMode == om_rotating
            || mOperationMode == om_scaling) {
        UBApplication::undoStack->beginMacro(UBSettings::undoCommandTransactionName);
        foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
            d->commitUndoStep();
        }
        UBApplication::undoStack->endMacro();
    }
    mOperationMode = om_idle;
    mCurrentScaleTool = NoScale;
    mScalingInitialTransforms.clear();
    mScalingInitialParentSceneTransforms.clear();

}

void UBSelectionFrame::onZoomChanged(qreal pZoom)
{
    mAntiscaleRatio = 1 / (UBApplication::boardController->systemScaleFactor() * pZoom);

    placeButtons();
}

void UBSelectionFrame::remove()
{
    UBApplication::undoStack->beginMacro(UBSettings::undoCommandTransactionName);
    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        d->remove(true);
    }
    UBApplication::undoStack->endMacro();

    updateRect();
}

static bool sortByZ(UBGraphicsItemDelegate* A, UBGraphicsItemDelegate* B)
{
    return (A->delegated()->data(UBGraphicsItemData::ItemOwnZValue).toReal()
            < B->delegated()->data(UBGraphicsItemData::ItemOwnZValue).toReal() );
}

void UBSelectionFrame::duplicate()
{
    UBApplication::undoStack->beginMacro(UBSettings::undoCommandTransactionName);

    // The mEnclosedtems list items are in order of selection. To avoid losing their
    // relative zValues when duplicating, we re-order the list.
    std::sort(mEnclosedtems.begin(), mEnclosedtems.end(), sortByZ);

    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        d->duplicate();
    }
    UBApplication::undoStack->endMacro();

    updateRect();
}

void UBSelectionFrame::increaseZlevelUp()
{
    QList<QGraphicsItem*> selItems = sortedByZ(scene()->selectedItems());

    QList<QGraphicsItem*>::iterator itemIt = selItems.end();
    while(itemIt != selItems.begin()){
        itemIt--;
        QGraphicsItem* item = *itemIt;
        ubscene()->changeZLevelTo(item, UBZLayerController::up);
    }

    addSelectionUndo(selItems, UBZLayerController::up);
}

void UBSelectionFrame::increaseZlevelTop()
{
    QList<QGraphicsItem*> selItems = sortedByZ(scene()->selectedItems());
    foreach (QGraphicsItem *item, selItems) {
        ubscene()->changeZLevelTo(item, UBZLayerController::top);
    }
    addSelectionUndo(selItems, UBZLayerController::top);
}

void UBSelectionFrame::increaseZlevelDown()
{
    QList<QGraphicsItem*> selItems = sortedByZ(scene()->selectedItems());
    foreach (QGraphicsItem *item, selItems) {
        ubscene()->changeZLevelTo(item, UBZLayerController::down);
    }
    addSelectionUndo(selItems, UBZLayerController::down);
}

void UBSelectionFrame::increaseZlevelBottom()
{
    QList<QGraphicsItem*> selItems = sortedByZ(scene()->selectedItems());
    QListIterator<QGraphicsItem*> iter(selItems);
    iter.toBack();
    while (iter.hasPrevious()) {
        ubscene()->changeZLevelTo(iter.previous(), UBZLayerController::bottom);
    }
    addSelectionUndo(selItems, UBZLayerController::bottom);
}

void UBSelectionFrame::groupItems()
{
    UBGraphicsGroupContainerItem *groupItem = ubscene()->createGroup(enclosedGraphicsItems());

    groupItem->setSelected(true);
    UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
    qDebug() << "Grouping items";
}

void UBSelectionFrame::addSelectionUndo(QList<QGraphicsItem*> items, UBZLayerController::moveDestination dest){
    if(!items.empty()){
        qreal topItemLevel = items.at(0)->data(UBGraphicsItemData::ItemOwnZValue).toReal();
        UBGraphicsItemZLevelUndoCommand* cmd = new UBGraphicsItemZLevelUndoCommand(ubscene(), items, topItemLevel, dest);
        UBApplication::undoStack->push(cmd);
    }
}

void UBSelectionFrame::translateItem(QGraphicsItem */*item*/, const QPointF &/*translatePoint*/)
{
}

void UBSelectionFrame::placeButtons()
{
    placeResizeGrips();

    if (!mButtons.count()) {
        return;
    }

    QTransform tr;
    tr.scale(mAntiscaleRatio, mAntiscaleRatio);
    mDeleteButton->setParentItem(this);
    mDeleteButton->setTransform(tr);

    QRectF frRect = boundingRect();

    qreal topX = frRect.left() - mDeleteButton->renderer()->viewBox().width() * mAntiscaleRatio / 2;
    qreal topY = frRect.top() - mDeleteButton->renderer()->viewBox().height() * mAntiscaleRatio / 2;

    qreal bottomX = topX;
    qreal bottomY = frRect.bottom() - mDeleteButton->renderer()->viewBox().height() * mAntiscaleRatio / 2;

    mDeleteButton->setPos(topX, topY);
    mDeleteButton->show();

    int i = 1, j = 0, k = 0;
    while ((i + j + k) < mButtons.size())  {
        DelegateButton* button = mButtons[i + j];

        if (button->getSection() == Qt::TopLeftSection) {
            button->setParentItem(this);
            button->setPos(topX + (i++ * 1.6 * adjThickness()), topY);
            button->setTransform(tr);
        } else if (button->getSection() == Qt::BottomLeftSection) {
            button->setParentItem(this);
            button->setPos(bottomX + (++j * 1.6 * adjThickness()), bottomY);
            button->setTransform(tr);
        } else if (button->getSection() == Qt::NoSection) {
            button->setParentItem(this);
            placeExceptionButton(button, tr);
            k++;
        } else {
            ++k;
        }
            button->show();
    }
}

void UBSelectionFrame::placeExceptionButton(DelegateButton *pButton, QTransform pTransform)
{
    QRectF frRect = boundingRect();

    if (pButton == mRotateButton) {
        qreal topX = frRect.right() - ((mThickness + 5) * mAntiscaleRatio);
        qreal topY = frRect.top() + (5 * mAntiscaleRatio);
        mRotateButton->setPos(topX, topY);
        mRotateButton->setTransform(pTransform);
    }
}

void UBSelectionFrame::clearButtons()
{
    foreach (DelegateButton *b, mButtons)
    {
        b->setParentItem(0);
        b->hide();
    }

    mButtons.clear();
}

inline std::shared_ptr<UBGraphicsScene> UBSelectionFrame::ubscene()
{
    auto scenePtr = dynamic_cast<UBGraphicsScene*>(scene());
    return scenePtr ? scenePtr->shared_from_this() : nullptr;
}

void UBSelectionFrame::setCursorFromAngle(QString angle)
{
    QWidget *controlViewport = UBApplication::boardController->controlView()->viewport();

    QSize cursorSize(45,30);


    QImage mask_img(cursorSize, QImage::Format_Mono);
    mask_img.fill(0xff);
    QPainter mask_ptr(&mask_img);
    mask_ptr.setBrush( QBrush( QColor(0, 0, 0) ) );
    mask_ptr.drawRoundedRect(0,0, cursorSize.width()-1, cursorSize.height()-1, 6, 6);
    QBitmap bmpMask = QBitmap::fromImage(mask_img);


    QPixmap pixCursor(cursorSize);
    pixCursor.fill(QColor(Qt::white));

    QPainter painter(&pixCursor);

    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    painter.setBrush(QBrush(Qt::white));
    painter.setPen(QPen(QColor(Qt::black)));
    painter.drawRoundedRect(1,1,cursorSize.width()-2,cursorSize.height()-2,6,6);
    painter.setFont(QFont("Arial", 10));
    painter.drawText(1,1,cursorSize.width(),cursorSize.height(), Qt::AlignCenter, angle.append(QChar(176)));
    painter.end();

    pixCursor.setMask(bmpMask);
    controlViewport->setCursor(QCursor(pixCursor));
}

QList<QGraphicsItem*> UBSelectionFrame::sortedByZ(const QList<QGraphicsItem *> &pItems)
{
    //select only items wiht the same z-level as item's one and push it to sortedItems QMultiMap
    // It means: keep only the selected items and remove the selection frame from the list
    QMultiMap<qreal, QGraphicsItem*> sortedItems;
    foreach (QGraphicsItem *tmpItem, pItems) {
        if (tmpItem->type() == Type) {
            continue;
        }
        sortedItems.insert(tmpItem->data(UBGraphicsItemData::ItemOwnZValue).toReal(), tmpItem);
    }

    return sortedItems.values();
}

void UBSelectionFrame::showStrokeColorMenu()
{
    if (mEnclosedtems.isEmpty())
        return;

    const auto scenePtr = ubscene();
    const bool darkBackground = scenePtr ? scenePtr->isDarkBackground()
                                         : UBSettings::settings()->isDarkBackground();

    // The palette holds the five fixed colors followed by the two most recent
    // custom colors (UBSettings::colorPaletteSize == 7).
    const QList<QColor> colors = UBSettings::settings()->penColors(darkBackground);

    QMenu menu;

    QWidget *swatchWidget = new QWidget(&menu);
    QHBoxLayout *swatchLayout = new QHBoxLayout(swatchWidget);
    swatchLayout->setContentsMargins(4, 4, 4, 4);
    swatchLayout->setSpacing(2);

    foreach (const QColor &color, colors) {
        QToolButton *swatchButton = new QToolButton(swatchWidget);
        swatchButton->setAutoRaise(true);
        swatchButton->setFixedSize(24, 24);
        swatchButton->setToolTip(color.name(QColor::HexArgb));

        QPixmap swatch(18, 18);
        swatch.fill(color);
        QPainter swatchPainter(&swatch);
        swatchPainter.setPen(QPen(QColor(0, 0, 0, 90), 1));
        swatchPainter.drawRect(0, 0, swatch.width() - 1, swatch.height() - 1);
        swatchPainter.end();

        swatchButton->setIcon(QIcon(swatch));
        swatchButton->setIconSize(QSize(18, 18));

        connect(swatchButton, &QToolButton::clicked, &menu,
                [this, &menu, color]() {
            applyStrokeColor(color);
            menu.close();
        });

        swatchLayout->addWidget(swatchButton);
    }

    QWidgetAction *swatchAction = new QWidgetAction(&menu);
    swatchAction->setDefaultWidget(swatchWidget);
    menu.addAction(swatchAction);
    menu.addSeparator();

    QAction *customAction = menu.addAction(tr("Custom Color..."));
    connect(customAction, &QAction::triggered, &menu, [this]() {
        const QColor currentColor = UBSettings::settings()->currentPenColor();
        const QColor chosenColor = QColorDialog::getColor(
                    currentColor, 0, tr("Choose stroke color"),
                    QColorDialog::ShowAlphaChannel);
        if (chosenColor.isValid())
            applyStrokeColor(chosenColor);
    });

    // Anchor the popup to the button in the graphics view.
    QPoint globalPos = QCursor::pos();
    if (UBApplication::boardController && UBApplication::boardController->controlView()
            && mChangeColorButton) {
        UBBoardView *view = UBApplication::boardController->controlView();
        const QPoint viewportPos =
                view->mapFromScene(mChangeColorButton->sceneBoundingRect().bottomLeft());
        globalPos = view->viewport()->mapToGlobal(viewportPos);
    }

    menu.exec(globalPos);
}

void UBSelectionFrame::applyStrokeColor(const QColor &color)
{
    if (!color.isValid())
        return;

    const auto scenePtr = ubscene();
    const bool darkBackground = scenePtr ? scenePtr->isDarkBackground()
                                         : UBSettings::settings()->isDarkBackground();

    // Store the twin colors so that switching the board theme keeps the
    // recolored strokes readable.
    const QColor inverted = UBDrawingController::invertedColor(color);

    UBGraphicsItemColorUndoCommand::ColorState after;
    after.current = color;
    after.light = darkBackground ? inverted : color;
    after.dark = darkBackground ? color : inverted;

    QList<UBGraphicsItemColorUndoCommand::Entry> entries;
    QSet<QGraphicsItem*> processed;

    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        QList<QGraphicsItem*> strokeItems;
        collectStrokeItems(d->delegated(), strokeItems);

        foreach (QGraphicsItem *item, strokeItems) {
            if (processed.contains(item))
                continue;
            processed.insert(item);

            const UBGraphicsItemColorUndoCommand::ColorState before =
                    UBGraphicsItemColorUndoCommand::stateOf(item);

            if (before != after) {
                UBGraphicsItemColorUndoCommand::Entry entry;
                entry.item = item;
                entry.before = before;
                entry.after = after;
                entries << entry;
            }
        }
    }

    if (entries.isEmpty())
        return;

    UBApplication::undoStack->push(new UBGraphicsItemColorUndoCommand(entries));
    updateRect();
}

void UBSelectionFrame::placeResizeGrips()
{
    const qreal gripSize = qMax<qreal>(adjThickness(), 8.0);
    // The frame border (and all DelegateButtons) live on boundingRect(); rect()
    // is the inner content rectangle.
    const QRectF r = boundingRect();
    const bool visible = !mIsLocked && !mEnclosedtems.isEmpty() && !rect().isEmpty();

    auto placeGrip = [gripSize](QGraphicsSvgItem *svgItem, QGraphicsRectItem *hitItem,
                                const QRectF &hitRect) {
        if (!svgItem || !hitItem)
            return;

        hitItem->setRect(hitRect);

        const QRectF svgBounds = svgItem->boundingRect();
        if (!svgBounds.isEmpty()) {
            QTransform tr;
            tr.scale(gripSize / svgBounds.width(), gripSize / svgBounds.height());
            svgItem->setTransform(tr);
            svgItem->setPos(hitRect.topLeft()
                            - QPointF(svgBounds.left() * tr.m11(),
                                      svgBounds.top() * tr.m22()));
        }
    };

    placeGrip(mBottomRightResizeGripSvgItem, mBottomRightResizeGrip,
              QRectF(r.right() - gripSize, r.bottom() - gripSize, gripSize, gripSize));
    placeGrip(mBottomResizeGripSvgItem, mBottomResizeGrip,
              QRectF(r.center().x() - gripSize / 2.0, r.bottom() - gripSize, gripSize, gripSize));
    placeGrip(mLeftResizeGripSvgItem, mLeftResizeGrip,
              QRectF(r.left(), r.center().y() - gripSize / 2.0, gripSize, gripSize));
    placeGrip(mRightResizeGripSvgItem, mRightResizeGrip,
              QRectF(r.right() - gripSize, r.center().y() - gripSize / 2.0, gripSize, gripSize));
    placeGrip(mTopResizeGripSvgItem, mTopResizeGrip,
              QRectF(r.center().x() - gripSize / 2.0, r.top(), gripSize, gripSize));

    QGraphicsSvgItem *svgs[] = {
        mBottomRightResizeGripSvgItem, mBottomResizeGripSvgItem,
        mLeftResizeGripSvgItem, mRightResizeGripSvgItem, mTopResizeGripSvgItem
    };
    QGraphicsRectItem *hits[] = {
        mBottomRightResizeGrip, mBottomResizeGrip,
        mLeftResizeGrip, mRightResizeGrip, mTopResizeGrip
    };

    for (int i = 0; i < 5; ++i) {
        if (svgs[i])
            svgs[i]->setVisible(visible);
        if (hits[i])
            hits[i]->setVisible(visible);
    }
}

UBSelectionFrame::ScaleTool UBSelectionFrame::scaleToolFromPos(const QPointF &scenePos) const
{
    if (mIsLocked || mEnclosedtems.isEmpty())
        return NoScale;

    auto contains = [&scenePos](QGraphicsRectItem *grip) {
        return grip && grip->isVisible()
                && grip->contains(grip->mapFromScene(scenePos));
    };

    if (contains(mBottomRightResizeGrip))
        return ScaleBottomRight;
    if (contains(mRightResizeGrip))
        return ScaleRight;
    if (contains(mLeftResizeGrip))
        return ScaleLeft;
    if (contains(mBottomResizeGrip))
        return ScaleBottom;
    if (contains(mTopResizeGrip))
        return ScaleTop;

    return NoScale;
}

void UBSelectionFrame::beginGroupScale(const QPointF &/*scenePos*/)
{
    mScalingStartBounds = rect();
    mScalingInitialTransforms.clear();
    mScalingInitialParentSceneTransforms.clear();

    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        QGraphicsItem *item = d->delegated();
        mScalingInitialTransforms.insert(item, item->transform());
        mScalingInitialParentSceneTransforms.insert(
                    item,
                    item->parentItem() ? item->parentItem()->sceneTransform()
                                       : QTransform());
    }
}

void UBSelectionFrame::applyGroupScale(const QPointF &scenePos)
{
    const QRectF startBounds = mScalingStartBounds;
    if (startBounds.width() <= 0.0 || startBounds.height() <= 0.0)
        return;

    qreal scaleX = 1.0;
    qreal scaleY = 1.0;
    QPointF fixedPoint = startBounds.topLeft();

    switch (mCurrentScaleTool) {
    case ScaleBottomRight: {
        // Corner scaling keeps the group aspect ratio.
        const qreal sx = (scenePos.x() - startBounds.left()) / startBounds.width();
        const qreal sy = (scenePos.y() - startBounds.top()) / startBounds.height();
        if (sx > 0.0 && sy > 0.0) {
            const qreal scale = (sx + sy) / 2.0;
            scaleX = scale;
            scaleY = scale;
        }
        break;
    }
    case ScaleRight:
        fixedPoint = startBounds.topLeft();
        scaleX = (scenePos.x() - startBounds.left()) / startBounds.width();
        break;
    case ScaleLeft:
        fixedPoint = startBounds.topRight();
        scaleX = (startBounds.right() - scenePos.x()) / startBounds.width();
        break;
    case ScaleBottom:
        fixedPoint = startBounds.topLeft();
        scaleY = (scenePos.y() - startBounds.top()) / startBounds.height();
        break;
    case ScaleTop:
        fixedPoint = startBounds.bottomLeft();
        scaleY = (startBounds.bottom() - scenePos.y()) / startBounds.height();
        break;
    default:
        return;
    }

    // Never flip or collapse the selection.
    const qreal minScale = 0.02;
    scaleX = qMax(scaleX, minScale);
    scaleY = qMax(scaleY, minScale);

    QTransform scaling;
    scaling.translate(fixedPoint.x(), fixedPoint.y());
    scaling.scale(scaleX, scaleY);
    scaling.translate(-fixedPoint.x(), -fixedPoint.y());

    for (auto it = mScalingInitialTransforms.cbegin();
         it != mScalingInitialTransforms.cend(); ++it)
    {
        QGraphicsItem *item = it.key();
        const QTransform initialTransform = it.value();
        const QTransform parentSceneTransform =
                mScalingInitialParentSceneTransforms.value(item);

        // Qt composes an item's scene transform as:
        //     transform() * translate(pos()) * parentSceneTransform
        // Keeping pos() untouched and inserting the scaling at the end gives
        // the exact scene-space result, without re-applying translations.
        const QPointF pos = item->pos();
        const QTransform newTransform =
                initialTransform
                * QTransform::fromTranslate(pos.x(), pos.y())
                * parentSceneTransform
                * scaling
                * parentSceneTransform.inverted()
                * QTransform::fromTranslate(-pos.x(), -pos.y());

        item->setTransform(newTransform);
    }
}

QList<DelegateButton*> UBSelectionFrame::buttonsForFlags(UBGraphicsFlags fls) {

    QList<DelegateButton*> result;

    if (!mDeleteButton) {
        mDeleteButton = new DelegateButton(":/images/close.svg", this, 0, Qt::TopLeftSection);
        connect(mDeleteButton, SIGNAL(clicked()), this, SLOT(remove()));
    }
    result << mDeleteButton;

    if (fls | GF_DUPLICATION_ENABLED) {
        if (!mDuplicateButton) {
            mDuplicateButton = new DelegateButton(":/images/duplicate.svg", this, 0, Qt::TopLeftSection);
            connect(mDuplicateButton, SIGNAL(clicked(bool)), this, SLOT(duplicate()));
        }
        result <<  mDuplicateButton;
    }

    // "Change stroke color" is only meaningful when the selection actually
    // contains stroke/vector items.
    bool hasStrokeItems = false;
    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        QList<QGraphicsItem*> strokeItems;
        collectStrokeItems(d->delegated(), strokeItems);
        if (!strokeItems.isEmpty()) {
            hasStrokeItems = true;
            break;
        }
    }

    if (hasStrokeItems) {
        if (!mChangeColorButton) {
            mChangeColorButton = new DelegateButton(":/images/color.svg", this, 0, Qt::TopLeftSection);
            mChangeColorButton->setShowProgressIndicator(false);
            mChangeColorButton->setToolTip(tr("Apply the current pen color to the selected strokes"));
            connect(mChangeColorButton, SIGNAL(clicked(bool)), this, SLOT(showStrokeColorMenu()));
        }
        result << mChangeColorButton;
    }

    if (mEnclosedtems.count() >= 2) {
        if (!mGroupButton) {
            mGroupButton = new DelegateButton(":/images/groupItems.svg", this, 0, Qt::TopLeftSection);
            mGroupButton->setShowProgressIndicator(false);
            connect(mGroupButton, SIGNAL(clicked()), this, SLOT(groupItems()));
        }
        result << mGroupButton;
    }

    if (fls | GF_ZORDER_MANIPULATIONS_ALLOWED) {
        if (!mZOrderUpButton) {
            mZOrderUpButton = new DelegateButton(":/images/z_layer_up.svg", this, 0, Qt::BottomLeftSection);
            mZOrderUpButton->setShowProgressIndicator(true);
            connect(mZOrderUpButton, SIGNAL(clicked()), this, SLOT(increaseZlevelUp()));
            connect(mZOrderUpButton, SIGNAL(longClicked()), this, SLOT(increaseZlevelTop()));
        }

        if (!mZOrderDownButton) {
            mZOrderDownButton = new DelegateButton(":/images/z_layer_down.svg", this, 0, Qt::BottomLeftSection);
            mZOrderDownButton->setShowProgressIndicator(true);
            connect(mZOrderDownButton, SIGNAL(clicked()), this, SLOT(increaseZlevelDown()));
            connect(mZOrderDownButton, SIGNAL(longClicked()), this, SLOT(increaseZlevelBottom()));
        }

        result << mZOrderUpButton;
        result << mZOrderDownButton;
    }

    if (fls | GF_REVOLVABLE) {
        if (!mRotateButton) {
            mRotateButton = new DelegateButton(":/images/rotate.svg", this, 0, Qt::NoSection);
            mRotateButton->setCursor(UBResources::resources()->rotateCursor);
            mRotateButton->setShowProgressIndicator(false);
            mRotateButton->setTransparentToMouseEvent(true);
        }
        result << mRotateButton;
    }

    return result;
}

QList<QGraphicsItem*> UBSelectionFrame::enclosedGraphicsItems()
{
    QList<QGraphicsItem*> result;
    foreach (UBGraphicsItemDelegate *d, mEnclosedtems) {
        result << d->delegated();
    }

    return result;
}


