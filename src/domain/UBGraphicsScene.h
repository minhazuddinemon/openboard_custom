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


#ifndef UBGRAPHICSSCENE_H_
#define UBGRAPHICSSCENE_H_

#include <QtGui>
#include <QPointer>
#include <optional>
#include <utility>

#include "frameworks/UBCoreGraphicsScene.h"

#include "core/UB.h"

#include "UBItem.h"
#include "UBGraphicsLaserStroke.h"

class QGraphicsObject;
class UBGraphicsPixmapItem;
class UBGraphicsSvgItem;
class UBGraphicsPolygonItem;
class UBGraphicsMediaItem;
class UBGraphicsWidgetItem;
class UBGraphicsW3CWidgetItem;
class UBGraphicsAppleWidgetItem;
class UBToolWidget;
class UBGraphicsPDFItem;
class UBGraphicsTextItem;
class UBGraphicsRuler;
class UBGraphicsProtractor;
class UBGraphicsCompass;
class UBDocumentProxy;
class UBGraphicsCurtainItem;
class UBGraphicsStroke;
class UBMagnifierParams;
class UBMagnifier;
class UBGraphicsCache;
class UBGraphicsGroupContainerItem;
class UBSelectionFrame;
class UBBoardView;
class UBGraphicsLaserStroke;
class QTimer;

const double PI = 4.0 * atan(1.0);

class UBZLayerController : public QObject
{
    Q_OBJECT

public:
    struct ItemLayerTypeData {
        ItemLayerTypeData() : bottomLimit(0), topLimit(0), curValue(0), incStep(1) {;}
        ItemLayerTypeData(qreal bot, qreal top, qreal increment = 1) : bottomLimit(bot), topLimit(top), curValue(bot), incStep(increment) {;}
        qreal bottomLimit; //bottom bound of the layer
        qreal topLimit;//top bound of the layer
        qreal curValue;//current value of variable
        qreal incStep;//incremental step
    };

    enum moveDestination {
        up
        , down
        , top
        , bottom
    };

    typedef QMap<itemLayerType::Enum, ItemLayerTypeData> ScopeMap;

    UBZLayerController(QGraphicsScene *scene);

    qreal getBottomLimit(itemLayerType::Enum key) const {return scopeMap.value(key).bottomLimit;}
    qreal getTopLimit(itemLayerType::Enum key) const {return scopeMap.value(key).topLimit;}
    bool validLayerType(itemLayerType::Enum key) const {return scopeMap.contains(key);}

    static qreal errorNum() {return errorNumber;}

    qreal generateZLevel(itemLayerType::Enum key);
    qreal generateZLevel(QGraphicsItem *item);

    qreal changeZLevelTo(QGraphicsItem *item, moveDestination dest);
    itemLayerType::Enum typeForData(QGraphicsItem *item) const;
    void setLayerType(QGraphicsItem *pItem, itemLayerType::Enum pNewType);
    void shiftStoredZValue(QGraphicsItem *item, qreal zValue);

    bool zLevelAvailable(QGraphicsItem* item);

private:
    ScopeMap scopeMap;
    static qreal errorNumber;
    QGraphicsScene *mScene;
};

class UBGraphicsScene: public UBCoreGraphicsScene, public UBItem, public std::enable_shared_from_this<UBGraphicsScene>
{
    Q_OBJECT

    public:

    enum clearCase {
        clearItemsAndAnnotations = 0
        , clearAnnotations
        , clearItems
        , clearBackground
    };

    //        tmp stub for divide addings scene objects from undo mechanism implementation
        void enableUndoRedoStack(){mUndoRedoStackEnabled = true;}
        void setURStackEnable(bool enable){mUndoRedoStackEnabled = enable;}
        bool isURStackIsEnabled(){return mUndoRedoStackEnabled;}

        UBGraphicsScene(std::shared_ptr<UBDocumentProxy>document, bool enableUndoRedoStack = true);
        virtual ~UBGraphicsScene();

        virtual UBItem* deepCopy() const;

        virtual void copyItemParameters(UBItem *copy) const {Q_UNUSED(copy);}

        std::shared_ptr<UBGraphicsScene> sceneDeepCopy() const;

        void clearContent(clearCase pCase = clearItemsAndAnnotations);
        void saveWidgetSnapshots();

        bool inputDevicePress(const QPointF& scenePos, const qreal& pressure = 1.0, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
        bool inputDeviceMove(const QPointF& scenePos, const qreal& pressure = 1.0, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
        bool inputDeviceRelease(int tool = -1, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

        // Used by UBBoardView to decide whether a lasso press must be forwarded
        // natively to the graphics item framework (selection frame controls or
        // selected items) instead of starting a new lasso path.
        bool lassoShouldForwardPress(const QPointF& scenePos) const;

        // Cursor to use while hovering in lasso mode: resize cursors over grips,
        // pointing hand over frame buttons, crosshair over the canvas.
        Qt::CursorShape lassoHoverCursor(const QPointF& scenePos) const;

        void leaveEvent (QEvent* event);

        void addItem(QGraphicsItem* item);
        void removeItem(QGraphicsItem* item);

        void addItems(const QSet<QGraphicsItem*>& item);
        void removeItems(const QSet<QGraphicsItem*>& item);

        UBGraphicsWidgetItem* addWidget(const QUrl& pWidgetUrl, const QPointF& pPos = QPointF(0, 0));
        UBGraphicsAppleWidgetItem* addAppleWidget(const QUrl& pWidgetUrl, const QPointF& pPos = QPointF(0, 0));
        UBGraphicsW3CWidgetItem* addW3CWidget(const QUrl& pWidgetUrl, const QPointF& pPos = QPointF(0, 0));
        void addGraphicsWidget(UBGraphicsWidgetItem* graphicsWidget, const QPointF& pPos = QPointF(0, 0));

        UBGraphicsMediaItem* addMedia(const QUrl& pMediaFileUrl, bool shouldPlayAsap, const QPointF& pPos = QPointF(0, 0));
        UBGraphicsMediaItem* addVideo(const QUrl& pVideoFileUrl, bool shouldPlayAsap, const QPointF& pPos = QPointF(0, 0));
        UBGraphicsMediaItem* addAudio(const QUrl& pAudioFileUrl, bool shouldPlayAsap, const QPointF& pPos = QPointF(0, 0));
        UBGraphicsSvgItem* addSvg(const QUrl& pSvgFileUrl, const QPointF& pPos = QPointF(0, 0), const QByteArray pData = QByteArray());
        UBGraphicsTextItem* addText(const QString& pString, const QPointF& pTopLeft = QPointF(0, 0));

        UBGraphicsTextItem*  addTextWithFont(const QString& pString, const QPointF& pTopLeft = QPointF(0, 0)
                , int pointSize = -1, const QString& fontFamily = "", bool bold = false, bool italic = false);
        UBGraphicsTextItem* addTextHtml(const QString &pString = QString(), const QPointF& pTopLeft = QPointF(0, 0));

        UBGraphicsGroupContainerItem *createGroup(QList<QGraphicsItem*> items);
        void addGroup(UBGraphicsGroupContainerItem *groupItem);

        QGraphicsItem* setAsBackgroundObject(QGraphicsItem* item, bool pAdaptTransformation = false, bool expand = false);
        void unsetBackgroundObject();

        QGraphicsItem* backgroundObject() const
        {
            return mBackgroundObject;
        }

        bool isBackgroundObject(const QGraphicsItem* item) const
        {
            return item == mBackgroundObject;
        }

        QGraphicsItem* scaleToFitDocumentSize(QGraphicsItem* item, bool center = false, int margin = 0, bool expand = false);

        QRectF normalizedSceneRect(qreal ratio = -1.0);

        QGraphicsItem *itemForUuid(QUuid uuid);

        void moveTo(const QPointF& pPoint);
        void drawLineTo(const QPointF& pEndPoint, const qreal& pWidth, bool bLineStyle);
        void drawLineTo(const QPointF& pEndPoint, const qreal& pStartWidth, const qreal& endWidth, bool bLineStyle);
        void eraseLineTo(const QPointF& pEndPoint, const qreal& pWidth);
        bool strokeEraseAt(const QPointF& scenePos);
        void drawArcTo(const QPointF& pCenterPoint, qreal pSpanAngle);
        void drawCurve(const QList<QPair<QPointF, qreal> > &points);
        void drawCurve(const QList<QPointF>& points, qreal startWidth, qreal endWidth);
        void drawShapePreview(const QPointF& pEndPoint, qreal pWidth);

        // Finalizes an in-progress free triangle through the normal
        // inputDeviceRelease() commit path.
        void commitShapeEditing();

        // Stops the Pen/Marker hold-to-straighten timer (e.g. on page change).
        void stopStraightenHold();

        bool isEmpty() const;

        bool inputDeviceIsPressed() const { return mInputDeviceIsPressed; }
        int itemCount() const { return mItemCount; }

        void setDocument(std::shared_ptr<UBDocumentProxy> pDocument);

        std::shared_ptr<UBDocumentProxy> document() const
        {
            return mDocument;
        }

        bool isDarkBackground() const
        {
            return mDarkBackground;
        }

        bool isLightBackground() const
        {
            return !mDarkBackground;
        }

        UBPageBackground pageBackground() const
        {
            return mPageBackground;
        }

        int backgroundGridSize() const
        {
            return mBackgroundGridSize;
        }

        bool intermediateLines() const
        {
            return mIntermediateLines;
        }

        bool hasBackground()
        {
            return (mBackgroundObject != 0);
        }

        void addRuler(QPointF center);
        void addAxes(QPointF center);
        void addProtractor(QPointF center);
        void addCompass(QPointF center);
        void addTriangle(QPointF center);
        void addMagnifier(UBMagnifierParams params);

        void addMask(const QPointF &center = QPointF());
        void addCache();
        UBGraphicsCache* graphicsCache();

        bool isSnapping() const;

        // Stroke-only selection filter (persisted in UBSettings).
        bool isStrokeOnlySelection() const;
        static bool isStrokeOnlySelectableItem(QGraphicsItem* item);

        QPointF snap(const QPointF& point, double* force = nullptr, std::optional<QPointF> proposedPoint = {}, QPointF* gridSnapPoint = nullptr) const;
        QPointF snap(const std::vector<QPointF>& corners, int* snapIndex = nullptr) const;
        QPointF snap(const QRectF& rect, Qt::Corner* corner = nullptr) const;
        static QRectF itemRect(const QGraphicsItem* item);

        class SceneViewState
        {
            public:
                SceneViewState()
                {
                }

                SceneViewState(qreal pZoomFactor, int pHorizontalPosition, int pVerticalPostition, QPointF sceneCenter = QPointF())// 1595/1605
                    : zoomFactor{pZoomFactor}
                    , horizontalPosition{pHorizontalPosition}
                    , verticalPostition{pVerticalPostition}
                    , mLastSceneCenter{sceneCenter}
                {
                }

                qreal zoomFactor{1};
                int horizontalPosition{0};
                int verticalPostition{0};
                QPointF mLastSceneCenter{};
        };

        SceneViewState viewState() const
        {
            return mViewState;
        }

        void setViewState(const SceneViewState& pViewState)
        {
            mViewState = pViewState;
        }

        virtual void setRenderingQuality(UBItem::RenderingQuality pRenderingQuality, UBItem::CacheBehavior cacheBehavior);

        QList<QUrl> relativeDependenciesOfItem(QGraphicsItem* item) const;
        QList<QUrl> relativeDependencies() const;

        QSize nominalSize();

        QSize sceneSize();
        QSizeF sceneSizeF() const;

        void setNominalSize(const QSize& pSize);

        void setNominalSize(int pWidth, int pHeight);

        qreal changeZLevelTo(QGraphicsItem *item, UBZLayerController::moveDestination dest, bool addUndo=false);

        enum RenderingContext
        {
            Screen = 0, NonScreen, PdfExport, Podcast
        };

        void setRenderingContext(RenderingContext pRenderingContext)
        {
            mRenderingContext = pRenderingContext;
        }

        RenderingContext renderingContext() const
        {
            return mRenderingContext;
        }

        QSet<QGraphicsItem*> tools(){ return mTools;}

        void registerTool(QGraphicsItem* item)
        {
            mTools << item;
        }

        const QPointF& previousPoint()
        {
            return mPreviousPoint;
        }

        void setSelectedZLevel(QGraphicsItem *item);
        void setOwnZlevel(QGraphicsItem *item);

        static QUuid getPersonalUuid(QGraphicsItem *item);

        UBGraphicsPolygonItem* polygonToPolygonItem(const QPolygonF pPolygon);
        void clearSelectionFrame();
        UBBoardView *controlView();
        void notifyZChanged(QGraphicsItem *item, qreal zValue);
        void deselectAllItemsExcept(QGraphicsItem* graphicsItem);

        QRectF annotationsBoundingRect() const;

public slots:
        void updateSelectionFrame();
        void updateSelectionFrameWrapper(int);
        void initStroke();
        void hideTool();

        void setBackground(bool pIsDark, UBPageBackground pBackground);
        void setBackgroundZoomFactor(qreal zoom);
        void setBackgroundGridSize(int pSize);
        void setIntermediateLines(bool checked);
        void setDrawingMode(bool bModeDesktop);
        void deselectAllItems();

        UBGraphicsPixmapItem* addPixmap(const QPixmap& pPixmap,
            QGraphicsItem* replaceFor,
            const QPointF& pPos = QPointF(0,0),
            qreal scaleFactor = 1.0,
            bool pUseAnimation = false,
            bool useProxyForDocumentPath = false);

        UBGraphicsPixmapItem* addImage(QByteArray pData,
            QGraphicsItem* replaceFor,
            const QPointF& pPos = QPointF(0,0),
            qreal scaleFactor = 1.0,
            bool pUseAnimation = false,
            bool useProxyForDocumentPath = false);

        void textUndoCommandAdded(UBGraphicsTextItem *textItem);

        void setToolCursor(int tool);

        void selectionChangedProcessing();
        void moveMagnifier();
        void moveMagnifier(QPoint newPos, bool forceGrab = false);
        void closeMagnifier();
        void zoomInMagnifier();
        void zoomOutMagnifier();
        void changeMagnifierMode(int mode);
        void resizedMagnifier(qreal newPercent);

        void stylusToolChanged(int tool, int previousTool);

        void controlViewportChanged();

signals:
        void zoomChanged(qreal zoomFactor);

    protected:

        UBGraphicsPolygonItem* lineToPolygonItem(const QLineF& pLine, const qreal& pWidth);
        UBGraphicsPolygonItem* lineToPolygonItem(const QLineF &pLine, const qreal &pStartWidth, const qreal &pEndWidth);

        UBGraphicsPolygonItem* arcToPolygonItem(const QLineF& pStartRadius, qreal pSpanAngle, qreal pWidth);
        UBGraphicsPolygonItem* curveToPolygonItem(const QList<QPair<QPointF, qreal> > &points);
        UBGraphicsPolygonItem* curveToPolygonItem(const QList<QPointF> &points, qreal startWidth, qreal endWidth);
        void addPolygonItemToCurrentStroke(UBGraphicsPolygonItem* polygonItem);

        void clearShapePreview();
        void cancelShapePreview();
        void addShapeOutline(int kind, const QVector<QPointF>& points, qreal pWidth);
        void updateShapeHandle();
        void removeShapeHandle();
        void updateShapeAngleLabel(const QPointF& scenePos, qreal angle);
        void removeShapeAngleLabel();
        QPointF alignLineEndToAxis(const QPointF& start, const QPointF& end);
        void updateShapeStrokePreview(const QVector<QPointF>& points, qreal width, int style);
        void removeShapeStrokePreview();
        void finishShapeStrokePreview();
        bool isPointOnShapeHandle(const QPointF& scenePos) const;

        void initPolygonItem(UBGraphicsPolygonItem*);

        QGraphicsItem* strokeErasableItemFor(QGraphicsItem* item) const;

        void drawEraser(const QPointF& pEndPoint, bool pressed = true);
        void redrawEraser(bool pressed);
        void hideEraser();
        void drawPointer(const QPointF& pEndPoint, bool isFirstDraw = false);
        void drawMarkerCircle(const QPointF& pEndPoint);
        void drawPenCircle(const QPointF& pEndPoint);
        void hideMarkerCircle();
        void hidePenCircle();
        void DisposeMagnifierQWidgets();


        virtual void keyReleaseEvent(QKeyEvent * keyEvent);

        void recolorAllItems();

        virtual void drawItems (QPainter * painter, int numItems,
                               QGraphicsItem * items[], const QStyleOptionGraphicsItem options[], QWidget * widget = 0);

        QGraphicsItem* rootItem(QGraphicsItem* item) const;

        virtual void drawBackground(QPainter *painter, const QRectF &rect);


    private:
        void setDocumentUpdated();
        void updateBackground();
        void createEraiser();
        void createPointer();
        void createMarkerCircle();
        void createPenCircle();
        void updateEraserColor();
        void updateMarkerCircleColor();
        void updatePenCircleColor();
        bool hasTextItemWithFocus(UBGraphicsGroupContainerItem* item);
        void simplifyCurrentStroke();

        // Hold-to-straighten for Pen and Marker strokes.
        void startStraightenHold(const QPointF& startPoint);
        void straightenHoldTimeout();
        void updateStraightenedLine(const QPointF& endPoint);
        void discardStrokeForStraightening();

        // Returns the z-value for a freshly created highlighter (Marker)
        // stroke: just below the lowest pen stroke, above any non-stroke item
        // that sits below that pen stroke, and above older highlighter
        // strokes. Returns the group's current z when no pen stroke exists.
        qreal zForNewMarkerStroke(const QGraphicsItem* markerStroke) const;

        // Lasso selection helpers.
        void startLasso(const QPointF& scenePos);
        void updateLassoPath(const QPointF& scenePos);
        void finishLasso();
        void clearLassoPreview();
        QList<QGraphicsItem*> lassoSelectionItems(const QPainterPath& path) const;

        // Laser pointer helpers. Laser strokes are transient: they are added
        // through QGraphicsScene directly so they never touch the document
        // model, item count, undo stack or persistence.
        void startLaserStroke(const QPointF& scenePos);
        void updateLaserStroke(const QPointF& scenePos);
        void finishLaserStroke();
        void updateLaserStrokes();
        void scheduleLaserCleanup();

        QGraphicsEllipseItem* mEraser;
        QGraphicsEllipseItem* mPointer; // "laser" pointer
        QGraphicsEllipseItem* mMarkerCircle; // dotted circle around marker
        QGraphicsEllipseItem* mPenCircle; // dotted circle around pen

        QGraphicsPathItem* mLassoPathItem;
        QPainterPath mLassoPath;

        QTimer* mLaserTimer;
        QList<QPointer<UBGraphicsLaserStroke>> mLaserStrokes;
        QPointer<UBGraphicsLaserStroke> mCurrentLaserStroke;
        qint64 mLaserIdleMs;
        bool mLaserCleanupScheduled;

        QSet<QGraphicsItem*> mAddedItems;
        QSet<QGraphicsItem*> mRemovedItems;
        QMultiMap<UBGraphicsGroupContainerItem*, QUuid> mStrokeEraseGroups;

        std::shared_ptr<UBDocumentProxy> mDocument;

        bool mDarkBackground;
        UBPageBackground mPageBackground;
        int mBackgroundGridSize;
        bool mIntermediateLines;

        bool mIsDesktopMode;
        qreal mZoomFactor;

        QGraphicsItem* mBackgroundObject;

        QPointF mPreviousPoint;
        qreal mPreviousWidth;
        qreal mDistanceFromLastStrokePoint;
        QPointF mCurrentPoint;

        QList<UBGraphicsPolygonItem*> mPreviousPolygonItems;

        SceneViewState mViewState;

        bool mInputDeviceIsPressed;

        QPointF mShapeStartPoint;
        bool mShapeGestureActive;
        bool mShapeEditing;
        bool mShapeHandleDragging;
        bool mShapeForceCommit;
        // 0 = free, 1 = snapped horizontal, 2 = snapped vertical.
        int mAxisAlignAxis;
        QRectF mShapeEditingDragRect;
        int mShapeEditingKind;
        QPointF mShapeHandlePos;
        QGraphicsItem* mShapeHandle;
        QGraphicsObject* mShapeAngleLabel;
        QGraphicsObject* mShapeStrokePreview;

        QSet<QGraphicsItem*> mTools;

        UBGraphicsPolygonItem *mArcPolygonItem;

        QSize mNominalSize;

        RenderingContext mRenderingContext;

        UBGraphicsStroke* mCurrentStroke;

        int mItemCount;

        bool mHasCache;
        //        tmp stub for divide addings scene objects from undo mechanism implementation
        bool mUndoRedoStackEnabled;

        UBMagnifier *magniferControlViewWidget;
        UBMagnifier *magniferDisplayViewWidget;

        UBZLayerController *mZLayerController;
        UBGraphicsPolygonItem* mpLastPolygon;
        UBGraphicsPolygonItem* mTempPolygon;

        QTimer* mStraightenTimer;
        bool mStraightenPending;
        bool mStraightenActive;
        QPointF mStraightenStartPoint;
        QPointF mStraightenHoldAnchor;
        QPointF mStraightenEndPoint;
        qreal mStraightenWidth;
        qreal mStraightenPathLength;
        UBGraphicsPolygonItem* mStraightenPolygon;

        bool mDrawWithCompass;
        UBGraphicsPolygonItem *mCurrentPolygon;
        UBSelectionFrame *mSelectionFrame;

        UBGraphicsCache* mGraphicsCache;
};



#endif /* UBGRAPHICSSCENE_H_ */
