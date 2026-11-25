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




#include "UBBoardController.h"

#include <QtWidgets>
#include <QColorDialog>

#include "adaptors/UBMetadataDcSubsetAdaptor.h"
#include "adaptors/UBSvgSubsetAdaptor.h"

#include "board/UBBoardPaletteManager.h"
#include "gui/UBStylusPalette.h"
#include "board/UBBoardView.h"
#include "board/UBDrawingController.h"
#include "board/UBFeaturesController.h"

#include "frameworks/UBFileSystemUtils.h"
#include "frameworks/UBStringUtils.h"
#include "frameworks/UBPlatformUtils.h"

#include "core/UBApplication.h"
#include "core/UBHealthMonitor.h"
#include "core/UBApplicationController.h"
#include "core/UBDisplayManager.h"
#include "core/UBDocumentManager.h"
#include "core/UBDownloadManager.h"
#include "core/UBMimeData.h"
#include "core/UBPersistenceManager.h"
#include "core/UBSetting.h"
#include "core/UBSettings.h"
#include "core/UBSettings.h"

#include "document/UBDocument.h"
#include "document/UBDocumentController.h"
#include "document/UBDocumentProxy.h"

#include "domain/UBGraphicsGroupContainerItem.h"
#include "domain/UBShapeGeometry.h"
#include "domain/UBGraphicsItemUndoCommand.h"
#include "domain/UBGraphicsMediaItem.h"
#include "domain/UBGraphicsPDFItem.h"
#include "domain/UBGraphicsPixmapItem.h"
#include "domain/UBGraphicsSvgItem.h"
#include "domain/UBGraphicsTextItem.h"
#include "domain/UBGraphicsWidgetItem.h"
#include "domain/UBItem.h"
#include "domain/UBPageSizeUndoCommand.h"

#include "gui/UBFeaturesWidget.h"
#include "gui/UBKeyboardPalette.h"
#include "gui/UBMagnifer.h"
#include "gui/UBMainWindow.h"
#include "gui/UBMessageWindow.h"
#include "gui/UBThumbnailScene.h"
#include "gui/UBToolWidget.h"
#include "gui/UBToolbarButtonGroup.h"

#include "podcast/UBPodcastController.h"

#include "tools/UBToolsManager.h"

#include "web/UBEmbedController.h"
#include "web/UBEmbedParser.h"

#include "core/memcheck.h"

namespace {

QIcon createCustomColorPickerIcon()
{
    constexpr int iconSize = 32;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor border = QApplication::palette().color(QPalette::Mid);
    const QColor plus = QApplication::palette().color(QPalette::Text);

    painter.setPen(QPen(border, 1.5));
    painter.drawRoundedRect(QRectF(2.0, 2.0, iconSize - 4.0, iconSize - 4.0), 4.0, 4.0);

    painter.setPen(QPen(plus, 2.5));
    painter.drawLine(QPointF(iconSize / 2.0, 8.0), QPointF(iconSize / 2.0, iconSize - 8.0));
    painter.drawLine(QPointF(8.0, iconSize / 2.0), QPointF(iconSize - 8.0, iconSize / 2.0));

    return QIcon(pixmap);
}

QIcon createWidthSettingsIcon()
{
    constexpr int iconSize = 32;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor track = QApplication::palette().color(QPalette::Text);
    const QColor knob = QApplication::palette().color(QPalette::Base);
    const int rows[] = { 9, 16, 23 };
    const int knobs[] = { 11, 21, 14 };

    for (int i = 0; i < 3; ++i)
    {
        painter.setPen(QPen(track, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(4.0, rows[i]), QPointF(iconSize - 4.0, rows[i]));

        painter.setBrush(knob);
        painter.setPen(QPen(track, 1.0));
        painter.drawEllipse(QPointF(knobs[i], rows[i]), 3.0, 3.0);
    }

    return QIcon(pixmap);
}

QIcon createLineWidthIcon(qreal width)
{
    constexpr int iconSize = 32;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal ratio = qBound<qreal>(0.0, width / 60.0, 1.0);
    const qreal strokeWidth = 1.0 + ratio * 14.0;

    painter.setPen(QPen(QApplication::palette().color(QPalette::Text),
                        strokeWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(5.0, iconSize / 2.0),
                     QPointF(iconSize - 5.0, iconSize / 2.0));

    return QIcon(pixmap);
}

QIcon createEraserWidthIcon(qreal width)
{
    constexpr int iconSize = 32;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal ratio = qBound<qreal>(0.0, width / 300.0, 1.0);
    const qreal diameter = 6.0 + ratio * 20.0;
    const QRectF rect((iconSize - diameter) / 2.0,
                      (iconSize - diameter) / 2.0,
                      diameter, diameter);

    painter.setPen(QPen(QApplication::palette().color(QPalette::Text), 1.5));
    painter.setBrush(QApplication::palette().color(QPalette::Button));
    painter.drawEllipse(rect);

    return QIcon(pixmap);
}

QIcon createStrokeEraserIcon()
{
    constexpr int iconSize = 32;
    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor ink = QApplication::palette().color(QPalette::Text);
    const QColor paper = QApplication::palette().color(QPalette::Base);

    // Vector line with a small erased gap.
    painter.setPen(QPen(ink, 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(4.0, 10.0), QPointF(17.0, 10.0));
    painter.drawLine(QPointF(23.0, 10.0), QPointF(28.0, 10.0));

    // Eraser block touching the line.
    painter.save();
    painter.translate(21.0, 21.0);
    painter.rotate(-30.0);
    painter.setPen(QPen(ink, 1.5));
    painter.setBrush(paper);
    painter.drawRoundedRect(QRectF(-8.0, -6.0, 16.0, 12.0), 2.0, 2.0);
    painter.restore();

    // Precision contact dot.
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawEllipse(QPointF(20.0, 10.0), 2.5, 2.5);

    return QIcon(pixmap);
}

QIcon createShapeKindIcon(int kind)
{
    constexpr int iconSize = 32;
    constexpr qreal margin = 4.0;

    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor ink = QApplication::palette().color(QPalette::Text);
    const bool openShape = kind >= UBShapeLineKind;
    const qreal iconPenWidth = openShape ? 1.5 : 2.0;
    painter.setPen(QPen(ink, iconPenWidth, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    // Use the same geometry as the board. A slightly wide box keeps the ellipse
    // icon distinguishable from the circle icon.
    QPolygonF polygon;
    const QRectF iconDragRect = (kind == 12)
            ? QRectF(0.0, 0.72, 1.0, 0.0)   // horizontal base for the free triangle
            : QRectF(0.0, 0.0, 1.0, 0.72);
    const QVector<QPointF> shape = UBShapeGeometry::pointsForKind(kind, iconDragRect);
    for (const QPointF& point : shape)
        polygon << point;

    if (!polygon.isEmpty())
    {
        const QRectF bounds = polygon.boundingRect();
        const qreal available = iconSize - 2.0 * margin;
        const qreal scale = (bounds.width() > 0.0 && bounds.height() > 0.0)
                ? qMin(available / bounds.width(), available / bounds.height())
                : 1.0;
        const QPointF offset((iconSize - bounds.width() * scale) / 2.0 - bounds.left() * scale,
                             (iconSize - bounds.height() * scale) / 2.0 - bounds.top() * scale);

        for (int i = 0; i < polygon.size(); ++i)
        {
            const QPointF point = polygon.at(i);
            polygon[i] = QPointF(offset.x() + point.x() * scale,
                                 offset.y() + point.y() * scale);
        }

        if (openShape)
            painter.drawPolyline(polygon);
        else
            painter.drawPolygon(polygon);

        // Small identifying marks so each editable kind is recognisable.
        if (kind == 6)
        {
            // Parallelogram: filled dot at the top-edge midpoint.
            painter.setPen(Qt::NoPen);
            painter.setBrush(ink);
            painter.drawEllipse((polygon.at(1) + polygon.at(2)) / 2.0, 1.8, 1.8);
        }
        else if (kind == 10)
        {
            // Equilateral triangle: one short tick on each side.
            painter.setPen(QPen(ink, 1.2, Qt::SolidLine, Qt::RoundCap));
            painter.setBrush(Qt::NoBrush);
            for (int i = 0; i < 3; ++i)
            {
                const QPointF a = polygon.at(i);
                const QPointF b = polygon.at((i + 1) % 3);
                const QLineF side(a, b);
                if (side.length() > 0.0)
                {
                    const QPointF perp(-side.dy() / side.length(),
                                       side.dx() / side.length());
                    const QPointF mid = (a + b) / 2.0;
                    painter.drawLine(mid - perp * 2.4, mid + perp * 2.4);
                }
            }
        }
        else if (kind == 11)
        {
            // Right-angle triangle: small square in the right-angle corner.
            painter.setPen(QPen(ink, 1.2));
            painter.setBrush(Qt::NoBrush);
            const QPointF corner = polygon.at(0);
            painter.drawRect(QRectF(corner, corner + QPointF(4.0, 4.0)));
        }
        else if (kind == 12)
        {
            // Free triangle: filled dot at the apex.
            painter.setPen(Qt::NoPen);
            painter.setBrush(ink);
            painter.drawEllipse(polygon.at(2), 1.8, 1.8);
        }
        // Kind 4 (isosceles triangle) deliberately has no mark.
    }

    return QIcon(pixmap);
}

QIcon createShapeStrokeStyleIcon(int style)
{
    constexpr int iconSize = 32;

    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor ink = QApplication::palette().color(QPalette::Text);
    QPen pen(ink, 2.0, Qt::SolidLine, Qt::RoundCap);
    if (style == UBShapeStrokeStyleDashed)
        pen.setStyle(Qt::DashLine);
    else if (style == UBShapeStrokeStyleDotted)
        pen.setStyle(Qt::DotLine);

    painter.setPen(pen);
    painter.drawLine(QPointF(6.0, iconSize / 2.0),
                     QPointF(iconSize - 6.0, iconSize / 2.0));

    return QIcon(pixmap);
}

QIcon createMoreShapesIcon()
{
    constexpr int iconSize = 32;

    QPixmap pixmap(iconSize, iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor ink = QApplication::palette().color(QPalette::Text);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);

    const qreal dot = 3.0;
    const qreal step = 8.0;
    const qreal origin = (iconSize - 2.0 * step) / 2.0;

    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            painter.drawEllipse(QPointF(origin + column * step,
                                        origin + row * step), dot / 2.0, dot / 2.0);
        }
    }

    return QIcon(pixmap);
}

} // namespace

UBBoardController::UBBoardController(UBMainWindow* mainWindow)
    : UBDocumentContainer(mainWindow->centralWidget())
    , mMainWindow(mainWindow)
    , mActiveScene(0)
    , mActiveSceneIndex(-1)
    , mPaletteManager(0)
    , mEraserWidthChoice(nullptr)
    , mShapeChoice(nullptr)
    , mStrokeEraserActionIndex(-1)
    , mSoftwareUpdateDialog(0)
    , mMessageWindow(0)
    , mEmbedController(nullptr)
    , mControlView(0)
    , mDisplayView(0)
    , mControlContainer(0)
    , mControlLayout(0)
    , mZoomFactor(1.0)
    , mIsClosing(false)
    , mSystemScaleFactor(1.0)
    , mCleanupDone(false)
    , mCacheWidgetIsEnabled(false)
    , mDeletingSceneIndex(-1)
    , mMovingSceneIndex(-1)
    , mActionGroupText(tr("Group"))
    , mActionUngroupText(tr("Ungroup"))
    , mAutosaveTimer(0)
{
    mZoomFactor = UBSettings::settings()->boardZoomFactor->get().toDouble();

    int penColorIndex = UBSettings::settings()->penColorIndex();
    int markerColorIndex = UBSettings::settings()->markerColorIndex();

    mPenColorOnDarkBackground = UBSettings::settings()->penColors(true).at(penColorIndex);
    mPenColorOnLightBackground = UBSettings::settings()->penColors(false).at(penColorIndex);
    mMarkerColorOnDarkBackground = UBSettings::settings()->markerColors(true).at(markerColorIndex);
    mMarkerColorOnLightBackground = UBSettings::settings()->markerColors(false).at(markerColorIndex);
}


namespace
{
void collectDocumentProxies(const UBDocumentTreeModel* model,
                            const QModelIndex& parent,
                            QList<std::shared_ptr<UBDocumentProxy>>& documents)
{
    const int rows = model->rowCount(parent);
    for (int i = 0; i < rows; ++i)
    {
        const QModelIndex index = model->index(i, 0, parent);
        if (!index.isValid())
            continue;

        if (model->isDocument(index))
        {
            if (!model->inTrash(index))
                documents.append(model->proxyData(index));
        }
        else if (model->hasChildren(index))
        {
            collectDocumentProxies(model, index, documents);
        }
    }
}
} // namespace

std::shared_ptr<UBDocumentProxy> UBBoardController::mostRecentlyUsedDocument() const
{
    UBDocumentTreeModel* model =
            UBPersistenceManager::persistenceManager()->mDocumentTreeStructureModel;
    if (!model)
        return std::shared_ptr<UBDocumentProxy>();

    QList<std::shared_ptr<UBDocumentProxy>> documents;
    collectDocumentProxies(model, QModelIndex(), documents);

    std::shared_ptr<UBDocumentProxy> mostRecent;
    QDateTime mostRecentDate;
    for (const std::shared_ptr<UBDocumentProxy>& document : documents)
    {
        if (!document)
            continue;

        QDateTime date = UBStringUtils::fromUtcIsoDate(
                document->metaData(UBSettings::documentUpdatedAt).toString());
        if (!date.isValid())
            date = QFileInfo(document->persistencePath()).lastModified();

        if (!mostRecent || date > mostRecentDate)
        {
            mostRecent = document;
            mostRecentDate = date;
        }
    }
    return mostRecent;
}

void UBBoardController::init()
{
    setupViews();
    setupToolbar();

    connect(UBApplication::undoStack, SIGNAL(canUndoChanged(bool))
            , this, SLOT(undoRedoStateChange(bool)));

    connect(UBApplication::undoStack, SIGNAL(canRedoChanged(bool))
            , this, SLOT(undoRedoStateChange(bool)));

    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int))
            , this, SLOT(setToolCursor(int)));

    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int))
            , this, SLOT(stylusToolChanged(int)));

    connect(UBApplication::app(), SIGNAL(lastWindowClosed())
            , this, SLOT(lastWindowClosed()));

    connect(UBDownloadManager::downloadManager(), SIGNAL(downloadModalFinished()), this, SLOT(onDownloadModalFinished()));
    connect(UBDownloadManager::downloadManager(), SIGNAL(addDownloadedFileToBoard(bool,QUrl,QUrl,QString,QByteArray,QPointF,QSize,bool)), this, SLOT(downloadFinished(bool,QUrl,QUrl,QString,QByteArray,QPointF,QSize,bool)));

    auto persistenceManager{UBPersistenceManager::persistenceManager()};
    connect(persistenceManager, &UBPersistenceManager::documentSceneDuplicated, this, &UBBoardController::documentSceneDuplicated);
    connect(persistenceManager, &UBPersistenceManager::documentSceneMoved, this, &UBBoardController::documentSceneMoved);
    connect(persistenceManager, &UBPersistenceManager::documentSceneDeleted, this, &UBBoardController::documentSceneDeleted);

    // Open the most recently used document on startup. Only create a new
    // one when no document exists yet.
    std::shared_ptr<UBDocumentProxy> doc = mostRecentlyUsedDocument();
    if (!doc)
        doc = UBPersistenceManager::persistenceManager()->createNewDocument();

    if (doc)
    {
        mInitialDocumentScene = setActiveDocumentScene(doc);
    }

    connect(UBApplication::displayManager, &UBDisplayManager::screenRolesAssigned, this, [this](){
        initBackgroundGridSize();
    });

    undoRedoStateChange(true);
}


UBBoardController::~UBBoardController()
{
    delete mDisplayView;
}

/**
 * @brief Set the default background grid size to appear as roughly 1cm on screen
 */
void UBBoardController::initBackgroundGridSize()
{
    // Besides adjusting for DPI, we also need to scale the grid size by the ratio of the control view size
    // to document size. Here we approximate this ratio as (document resolution) / (screen resolution).
    // Later on, this is calculated by `updateSystemScaleFactor` and stored in `mSystemScaleFactor`.

    qreal dpi = UBApplication::displayManager->logicalDpi(ScreenRole::Control);

    //qDebug() << "dpi: " << dpi;

    qreal screenY = UBApplication::displayManager->screenSize(ScreenRole::Control).height();
    qreal documentY = mActiveScene->nominalSize().height();
    qreal resolutionRatio = documentY / screenY;

    //qDebug() << "resolution ratio: " << resolutionRatio;

    int gridSize = (resolutionRatio * 10. * dpi) / UBGeometryUtils::inchSize;

    UBSettings::settings()->crossSize = gridSize;
    UBSettings::settings()->defaultCrossSize = gridSize;
    mActiveScene->setBackgroundGridSize(gridSize);

    //qDebug() << "grid size: " << gridSize;
}

int UBBoardController::currentPage() const
{
    return mActiveSceneIndex + 1;
}

void UBBoardController::setupViews()
{
    mControlContainer = new QWidget(mMainWindow->centralWidget());

    mControlLayout = new QHBoxLayout(mControlContainer);
    mControlLayout->setContentsMargins(0, 0, 0, 0);

    mControlView = new UBBoardView(this, mControlContainer, true, false);
    mControlView->setObjectName(CONTROLVIEW_OBJ_NAME);
    mControlView->setInteractive(true);
    mControlView->setMouseTracking(true);

    mControlView->grabGesture(Qt::SwipeGesture);

    mControlView->setTransformationAnchor(QGraphicsView::NoAnchor);

    mControlLayout->addWidget(mControlView);
    mControlContainer->setObjectName("ubBoardControlContainer");
    mMainWindow->addBoardWidget(mControlContainer);

    connect(mControlView, SIGNAL(resized(QResizeEvent*)), this, SLOT(boardViewResized(QResizeEvent*)));

    // TODO UB 4.x Optimization do we have to create the display view even if their is
    // only 1 screen
    //
    mDisplayView = new UBBoardView(this, UBItemLayerType::FixedBackground, UBItemLayerType::Tool, 0);
    mDisplayView->setInteractive(false);
    mDisplayView->setTransformationAnchor(QGraphicsView::NoAnchor);

    mPaletteManager = new UBBoardPaletteManager(mControlContainer, this);

    mMessageWindow = new UBMessageWindow(mControlContainer);
    mMessageWindow->hide();

    connect(this, SIGNAL(activeSceneChanged()), mPaletteManager, SLOT(activeSceneChanged()));
}


void UBBoardController::setupLayout()
{
    if(mPaletteManager)
        mPaletteManager->setupLayout();

    // Apply the stylus-palette clearance offset as soon as the palettes have
    // been laid out, so the initial board view is centered correctly from the
    // start instead of only after the first clear/resize.
    centerOnDefaultPosition();
}


void UBBoardController::setBoxing(QRect displayRect)
{
    if (displayRect.isNull())
    {
        mControlView->setBoxing({});
        return;
    }

    // compute boxing based on the assumed widget size for fullscreen
    QSize centralWidgetSize = mMainWindow->centralWidget()->size();
    QSize controlWindowSize = mMainWindow->size();
    QSize controlScreenSize = UBApplication::displayManager->screenSize(ScreenRole::Control);
    qreal controlWidth = controlScreenSize.width();
    qreal controlHeight = controlScreenSize.height() - controlWindowSize.height() + centralWidgetSize.height();
    qreal displayWidth = (qreal)displayRect.width();
    qreal displayHeight = (qreal)displayRect.height();

    qreal displayRatio = displayWidth / displayHeight;
    qreal controlRatio = controlWidth / controlHeight;

    if (displayRatio < controlRatio)
    {
        // Pillarboxing
        int boxWidth = (centralWidgetSize.width() - (displayWidth * (controlHeight / displayHeight))) / 2;

        if (boxWidth < 0)
        {
            boxWidth = 0;
        }

        mControlView->setBoxing({boxWidth, 0, boxWidth, 0});
    }
    else if (displayRatio > controlRatio)
    {
        // Letterboxing
        int boxHeight = (centralWidgetSize.height() - (displayHeight * (controlWidth / displayWidth))) / 2;

        if (boxHeight < 0)
        {
            boxHeight = 0;
        }

        mControlView->setBoxing({0, boxHeight, 0, boxHeight});
    }
    else
    {
        // No boxing
        mControlView->setBoxing({});
    }
}

void UBBoardController::setCursorFromAngle(qreal angle, const QPoint offset)
{
        QString displayedAngle = QString::number(angle, 'f', 1);
        QWidget *controlViewport = controlView()->viewport();

        QSize cursorSize(45,30);
        QSize bitmapSize = cursorSize;
        int hotX = -1;
        int hotY = -1;

        if (!offset.isNull())
        {
            bitmapSize.setWidth(std::max(bitmapSize.width(), 2 * std::abs(offset.x())));
            bitmapSize.setHeight(std::max(bitmapSize.height(), 2 * std::abs(offset.y())));
            hotX = bitmapSize.width() / 2 - offset.x();
            hotY = bitmapSize.height() / 2 - offset.y();
        }

        QSize origin = (bitmapSize - cursorSize) / 2;

        QImage mask_img(bitmapSize, QImage::Format_Mono);
        mask_img.fill(0xff);
        QPainter mask_ptr(&mask_img);
        mask_ptr.setBrush( QBrush( QColor(0, 0, 0) ) );
        mask_ptr.drawRoundedRect(origin.width(), origin.height(), cursorSize.width()-1, cursorSize.height()-1, 6, 6);
        QBitmap bmpMask = QBitmap::fromImage(mask_img);


        QPixmap pixCursor(bitmapSize);
        pixCursor.fill(QColor(Qt::white));

        QPainter painter(&pixCursor);

        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        painter.setBrush(QBrush(Qt::white));
        painter.setPen(QPen(QColor(Qt::black)));
        painter.drawRoundedRect(origin.width() + 1, origin.height() + 1,cursorSize.width()-2,cursorSize.height()-2,6,6);
        painter.setFont(QFont("Arial", 10));
        painter.drawText(origin.width() + 1, origin.height() + 1,cursorSize.width(),cursorSize.height(), Qt::AlignCenter, displayedAngle.append(QChar(176)));
        painter.end();

        pixCursor.setMask(bmpMask);
        controlViewport->setCursor(QCursor(pixCursor, hotX, hotY));
}


void UBBoardController::setupToolbar()
{
    UBSettings *settings = UBSettings::settings();

    // Setup color choice widget: five fixed colors, the two most recent
    // custom colors and a custom-color picker (8 buttons in total).
    QList<QAction *> colorActions;
    colorActions.append(mMainWindow->actionColor0);
    colorActions.append(mMainWindow->actionColor1);
    colorActions.append(mMainWindow->actionColor2);
    colorActions.append(mMainWindow->actionColor3);
    colorActions.append(mMainWindow->actionColor4);

    QAction *recentCustomColor1 = new QAction(tr("Custom 1"), mMainWindow->boardToolBar);
    recentCustomColor1->setCheckable(true);
    recentCustomColor1->setToolTip(tr("Most recent custom color"));

    QAction *recentCustomColor2 = new QAction(tr("Custom 2"), mMainWindow->boardToolBar);
    recentCustomColor2->setCheckable(true);
    recentCustomColor2->setToolTip(tr("Previous custom color"));

    QAction *customColorPicker = new QAction(tr("Custom color..."), mMainWindow->boardToolBar);
    customColorPicker->setCheckable(false);
    customColorPicker->setToolTip(tr("Choose a custom color"));

    colorActions << recentCustomColor1 << recentCustomColor2 << customColorPicker;

    UBToolbarButtonGroup *colorChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, colorActions, "color");
    colorChoice->setLabel(tr("Color"));
    colorChoice->setIcon(createCustomColorPickerIcon(), colorActions.size() - 1);

    // Inserting the (now wider) color group before the background group
    // reflows the following toolbar groups automatically.
    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, colorChoice);

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), colorChoice, SLOT(displayText(QVariant)));
    connect(colorChoice, SIGNAL(activated(int)), this, SLOT(setColorIndex(int)));
    connect(customColorPicker, SIGNAL(triggered(bool)), this, SLOT(openCustomColorDialog()));
    connect(UBDrawingController::drawingController(), SIGNAL(colorIndexChanged(int)), colorChoice, SLOT(setCurrentIndex(int)));
    connect(UBDrawingController::drawingController(), SIGNAL(colorIndexChanged(int)), UBDrawingController::drawingController(), SIGNAL(colorPaletteChanged()));
    connect(UBDrawingController::drawingController(), SIGNAL(colorPaletteChanged()), colorChoice, SLOT(colorPaletteChanged()));
    connect(UBDrawingController::drawingController(), SIGNAL(colorPaletteChanged()), this, SLOT(colorPaletteChanged()));

    // Never select the picker action itself: the last valid color index is the
    // second recent-custom-color slot (slot 7).
    const int initialColorIndex = qBound(0, settings->penColorIndex(), colorActions.size() - 2);

    colorChoice->displayText(QVariant(false)); // icon-only: keep the toolbar compact
    colorChoice->colorPaletteChanged();
    colorChoice->setCurrentIndex(initialColorIndex);
    colorActions.at(initialColorIndex)->setChecked(true);

    // Setup line width choice widget: Fine, Medium, Custom (slot 3) and the
    // slider/settings trigger (slot 4).
    QAction *lineWidthSettings = new QAction(tr("Adjust"), mMainWindow->boardToolBar);
    lineWidthSettings->setCheckable(false);
    lineWidthSettings->setToolTip(tr("Adjust custom line width"));
    lineWidthSettings->setIcon(createWidthSettingsIcon());

    QList<QAction *> lineWidthActions;
    lineWidthActions.append(mMainWindow->actionLineSmall);
    lineWidthActions.append(mMainWindow->actionLineMedium);
    lineWidthActions.append(mMainWindow->actionLineLarge);
    lineWidthActions.append(lineWidthSettings);

    UBToolbarButtonGroup *lineWidthChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, lineWidthActions);
    lineWidthChoice->setLabel(tr("Line"));

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), lineWidthChoice, SLOT(displayText(QVariant)));

    connect(lineWidthChoice, SIGNAL(activated(int))
            , UBDrawingController::drawingController(), SLOT(setLineWidthIndex(int)));

    connect(UBDrawingController::drawingController(), SIGNAL(lineWidthIndexChanged(int))
            , lineWidthChoice, SLOT(setCurrentIndex(int)));

    connect(lineWidthSettings, SIGNAL(triggered(bool)), this, SLOT(openLineWidthSlider()));
    connect(UBDrawingController::drawingController(), SIGNAL(lineWidthIndexChanged(int))
            , this, SLOT(updateWidthButtonIcons()));
    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int, int))
            , this, SLOT(updateWidthButtonIcons()));

    const int initialLineWidthIndex = qBound(0, settings->penWidthIndex(), lineWidthActions.size() - 2);

    lineWidthChoice->displayText(QVariant(false)); // icon-only: keep the toolbar compact
    lineWidthChoice->setCurrentIndex(initialLineWidthIndex);
    lineWidthActions.at(initialLineWidthIndex)->setChecked(true);

    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, lineWidthChoice);

    //-----------------------------------------------------------//
    // Setup eraser width choice widget

    QAction *eraserWidthSettings = new QAction(tr("Adjust"), mMainWindow->boardToolBar);
    eraserWidthSettings->setCheckable(false);
    eraserWidthSettings->setToolTip(tr("Adjust custom eraser size"));
    eraserWidthSettings->setIcon(createWidthSettingsIcon());

    QAction *strokeEraser = new QAction(tr("Stroke"), mMainWindow->boardToolBar);
    strokeEraser->setCheckable(true);
    strokeEraser->setToolTip(tr("Stroke eraser (removes whole strokes)"));
    strokeEraser->setIcon(createStrokeEraserIcon());

    QList<QAction *> eraserWidthActions;
    eraserWidthActions.append(mMainWindow->actionEraserSmall);
    eraserWidthActions.append(mMainWindow->actionEraserMedium);
    eraserWidthActions.append(mMainWindow->actionEraserLarge);
    eraserWidthActions.append(eraserWidthSettings);
    eraserWidthActions.append(strokeEraser); // slot 5

    UBToolbarButtonGroup *eraserWidthChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, eraserWidthActions);
    eraserWidthChoice->setLabel(tr("Eraser"));

    mEraserWidthChoice = eraserWidthChoice;
    mStrokeEraserActionIndex = eraserWidthActions.size() - 1;

    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, eraserWidthChoice);

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), eraserWidthChoice, SLOT(displayText(QVariant)));
    connect(eraserWidthChoice, SIGNAL(activated(int)), this, SLOT(eraserWidthActionActivated(int)));
    connect(eraserWidthSettings, SIGNAL(triggered(bool)), this, SLOT(openEraserWidthSlider()));
    connect(UBDrawingController::drawingController(), SIGNAL(eraserWidthIndexChanged(int))
            , eraserWidthChoice, SLOT(setCurrentIndex(int)));
    connect(UBDrawingController::drawingController(), SIGNAL(eraserWidthIndexChanged(int))
            , this, SLOT(updateWidthButtonIcons()));
    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int, int))
            , this, SLOT(eraserToolChanged(int)));

    const int initialEraserWidthIndex = settings->strokeEraserSelected()
            ? mStrokeEraserActionIndex
            : qBound(0, settings->eraserWidthIndex(), int(UBWidth::Strong));

    eraserWidthChoice->displayText(QVariant(false)); // icon-only: keep the toolbar compact
    eraserWidthChoice->setCurrentIndex(initialEraserWidthIndex);
    eraserWidthActions.at(initialEraserWidthIndex)->setChecked(true);

    updateWidthButtonIcons();

    //-----------------------------------------------------------//
    // Setup compact shape group:
    // [Rectangle][Line][Recent 1][Recent 2][More shapes].

    mShapeRecentKinds = UBSettings::settings()->shapeRecentKinds();

    QAction *shapeRectangle = new QAction(UBShapeGeometry::nameForKind(0),
                                          mMainWindow->boardToolBar);
    shapeRectangle->setCheckable(true);
    shapeRectangle->setToolTip(tr("Draw: %1").arg(UBShapeGeometry::nameForKind(0)));
    shapeRectangle->setIcon(createShapeKindIcon(0));

    QAction *shapeLine = new QAction(tr("Line"), mMainWindow->boardToolBar);
    shapeLine->setCheckable(true);
    shapeLine->setToolTip(tr("Draw a line"));
    shapeLine->setIcon(createShapeKindIcon(UBShapeLineKind));

    QAction *shapeRecent1 = new QAction(tr("Recent shape"), mMainWindow->boardToolBar);
    shapeRecent1->setCheckable(true);

    QAction *shapeRecent2 = new QAction(tr("Recent shape"), mMainWindow->boardToolBar);
    shapeRecent2->setCheckable(true);

    mShapeRecentActions.clear();
    mShapeRecentActions << shapeRecent1 << shapeRecent2;

    QAction *shapeMore = new QAction(tr("More shapes"), mMainWindow->boardToolBar);
    shapeMore->setCheckable(false);
    shapeMore->setToolTip(tr("More shapes"));
    shapeMore->setIcon(createMoreShapesIcon());

    QList<QAction *> shapeActions;
    shapeActions << shapeRectangle << shapeLine << shapeRecent1
                 << shapeRecent2 << shapeMore;

    UBToolbarButtonGroup *shapeChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, shapeActions, "shape");
    shapeChoice->setLabel(tr("Shape"));
    mShapeChoice = shapeChoice;

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), shapeChoice, SLOT(displayText(QVariant)));
    connect(shapeChoice, SIGNAL(activated(int)), this, SLOT(shapeGroupActionActivated(int)));
    connect(shapeMore, SIGNAL(triggered(bool)), this, SLOT(showShapePicker()));
    connect(UBDrawingController::drawingController(), SIGNAL(stylusToolChanged(int, int))
            , this, SLOT(shapeToolChanged(int)));

    shapeChoice->displayText(QVariant(false));
    updateShapeRecentActions();
    shapeToolChanged(UBDrawingController::drawingController()->stylusTool());

    //-----------------------------------------------------------//
    // Setup shape stroke style group (solid / dashed / dotted).

    QAction *solidStyle = new QAction(tr("Solid"), mMainWindow->boardToolBar);
    solidStyle->setCheckable(true);
    solidStyle->setToolTip(tr("Solid outline"));
    solidStyle->setIcon(createShapeStrokeStyleIcon(UBShapeStrokeStyleSolid));

    QAction *dashedStyle = new QAction(tr("Dashed"), mMainWindow->boardToolBar);
    dashedStyle->setCheckable(true);
    dashedStyle->setToolTip(tr("Dashed outline"));
    dashedStyle->setIcon(createShapeStrokeStyleIcon(UBShapeStrokeStyleDashed));

    QAction *dottedStyle = new QAction(tr("Dotted"), mMainWindow->boardToolBar);
    dottedStyle->setCheckable(true);
    dottedStyle->setToolTip(tr("Dotted outline"));
    dottedStyle->setIcon(createShapeStrokeStyleIcon(UBShapeStrokeStyleDotted));

    QList<QAction *> styleActions;
    styleActions << solidStyle << dashedStyle << dottedStyle;

    UBToolbarButtonGroup *styleChoice =
            new UBToolbarButtonGroup(mMainWindow->boardToolBar, styleActions, "shapeStyle");
    styleChoice->setLabel(tr("Style"));

    connect(settings->appToolBarDisplayText, SIGNAL(changed(QVariant)), styleChoice, SLOT(displayText(QVariant)));
    connect(styleChoice, SIGNAL(activated(int)), this, SLOT(setShapeStrokeStyle(int)));

    styleChoice->displayText(QVariant(false));

    const int initialStrokeStyle = UBSettings::settings()->shapeStrokeStyle();
    styleChoice->setCurrentIndex(initialStrokeStyle);
    styleActions.at(initialStrokeStyle)->setChecked(true);

    mMainWindow->boardToolBar->insertSeparator(mMainWindow->actionBackgrounds);
    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, shapeChoice);
    mMainWindow->boardToolBar->insertWidget(mMainWindow->actionBackgrounds, styleChoice);
    mMainWindow->boardToolBar->insertSeparator(mMainWindow->actionBackgrounds);

    //-----------------------------------------------------------//

    UBApplication::app()->insertSpaceToToolbarBeforeAction(mMainWindow->boardToolBar, mMainWindow->actionBoard);

    UBApplication::app()->decorateActionMenu(mMainWindow->actionMenu);

    mMainWindow->actionBoard->setVisible(false);

    mMainWindow->webToolBar->hide();
    mMainWindow->documentToolBar->hide();

    connectToolbar();
    initToolbarTexts();
    setToolbarTexts();

    // Force icon-only labels on the main toolbars to reclaim vertical drawing
    // space on the board (labels under icons are not shown anymore).
    UBApplication::app()->toolBarDisplayTextChanged(QVariant(false));
}


void UBBoardController::setToolCursor(int tool)
{
    if (mActiveScene)
        mActiveScene->setToolCursor(tool);

    mControlView->setToolCursor(tool);
}


void UBBoardController::connectToolbar()
{
    connect(mMainWindow->actionAdd, SIGNAL(triggered()), this, SLOT(addItem()));
    connect(mMainWindow->actionNewPage, SIGNAL(triggered()), this, SLOT(addScene()));
    connect(mMainWindow->actionDuplicatePage, SIGNAL(triggered()), this, SLOT(duplicateScene()));

    connect(mMainWindow->actionClearPage, SIGNAL(triggered()), this, SLOT(clearScene()));
    connect(mMainWindow->actionEraseItems, SIGNAL(triggered()), this, SLOT(clearSceneItems()));
    connect(mMainWindow->actionEraseAnnotations, SIGNAL(triggered()), this, SLOT(clearSceneAnnotation()));
    connect(mMainWindow->actionEraseBackground,SIGNAL(triggered()),this,SLOT(clearSceneBackground()));

    connect(mMainWindow->actionUndo, SIGNAL(triggered()), this, SLOT(undoWithShapeCommit()));
    connect(mMainWindow->actionRedo, SIGNAL(triggered()), this, SLOT(redoWithShapeCommit()));
    connect(mMainWindow->actionRedo, SIGNAL(triggered()), this, SLOT(startScript()));
    connect(mMainWindow->actionBack, SIGNAL( triggered()), this, SLOT(previousScene()));
    connect(mMainWindow->actionForward, SIGNAL(triggered()), this, SLOT(nextScene()));
    connect(mMainWindow->actionSleep, SIGNAL(triggered()), this, SLOT(stopScript()));
    connect(mMainWindow->actionSleep, SIGNAL(triggered()), this, SLOT(blackout()));
    connect(mMainWindow->actionVirtualKeyboard, SIGNAL(triggered(bool)), this, SLOT(showKeyboard(bool)));
    connect(mMainWindow->actionImportPage, SIGNAL(triggered()), this, SLOT(importPage()));
}

void UBBoardController::startScript()
{
    freezeW3CWidgets(false);
}

void UBBoardController::stopScript()
{
    freezeW3CWidgets(true);
}

void UBBoardController::undoWithShapeCommit()
{
    if (mActiveScene)
        mActiveScene->commitShapeEditing();

    if (UBApplication::undoStack)
        UBApplication::undoStack->undo();
}

void UBBoardController::redoWithShapeCommit()
{
    if (mActiveScene)
        mActiveScene->commitShapeEditing();

    if (UBApplication::undoStack)
        UBApplication::undoStack->redo();
}

void UBBoardController::saveData(SaveFlags fls)
{
    UBHealthMonitor::breadcrumb("save requested");

    bool verbose = fls | sf_showProgress;
    if (verbose) {
        UBApplication::showMessage(tr("Saving document..."));
    }
    if (mActiveScene && mActiveScene->isModified()) {
        persistCurrentScene(true);
    }
    if (verbose) {
        UBApplication::showMessage(tr("Document has just been saved..."));
    }

    UBHealthMonitor::snapshot("document-saved");
}

void UBBoardController::documentSceneDuplicated(std::shared_ptr<UBDocumentProxy> proxy, int index)
{
    // index is duplicated page
    if (selectedDocument() == proxy)
    {
        if (UBApplication::applicationController->displayMode() == UBApplicationController::Board)
        {
            // directly change scene to new duplicate
            setActiveDocumentScene(index);
        }
        else if (index <= mActiveSceneIndex)
        {
            // just shift selection and remember for the next time we switch to Board mode
            mSwitchToSceneIndex = mActiveSceneIndex + 1;
        }
    }
}

void UBBoardController::documentSceneMoved(std::shared_ptr<UBDocumentProxy> proxy, int fromIndex, int toIndex)
{
    if (selectedDocument() == proxy)
    {
        int nextSceneIndex = mActiveSceneIndex;

        if (fromIndex < mActiveSceneIndex && toIndex >= mActiveSceneIndex)
        {
            --nextSceneIndex;
        }
        else if (fromIndex > mActiveSceneIndex && toIndex <= mActiveSceneIndex)
        {
            ++nextSceneIndex;
        }
        else if (fromIndex == mActiveSceneIndex)
        {
            nextSceneIndex = toIndex;
        }

        if (nextSceneIndex == mActiveSceneIndex)
        {
            // no change
            return;
        }

        if (UBApplication::applicationController->displayMode() == UBApplicationController::Board)
        {
            // directly change scene
            setActiveDocumentScene(nextSceneIndex);
        }
        else
        {
            // just remember for the next time we switch to Board mode
            mSwitchToSceneIndex = nextSceneIndex;
        }
    }
}

void UBBoardController::documentSceneDeleted(std::shared_ptr<UBDocumentProxy> proxy, int index)
{
    if (selectedDocument() == proxy)
    {
        int nextSceneIndex = mActiveSceneIndex;

        if (index < mActiveSceneIndex || (index == mActiveSceneIndex && index == proxy->pageCount() && index > 0))
        {
            --nextSceneIndex;
        }

        if (UBApplication::applicationController->displayMode() == UBApplicationController::Board)
        {
            // directly change scene
            setActiveDocumentScene(nextSceneIndex);
        }
        else
        {
            // just remember for the next time we switch to Board mode
            mSwitchToSceneIndex = nextSceneIndex;
        }
    }
}

void UBBoardController::initToolbarTexts()
{
    QList<QAction*> allToolbarActions;

    allToolbarActions << mMainWindow->boardToolBar->actions();
    allToolbarActions << mMainWindow->webToolBar->actions();
    allToolbarActions << mMainWindow->documentToolBar->actions();

    foreach(QAction* action, allToolbarActions)
    {
        QString nominalText = action->text();
        QString shortText = truncate(nominalText, 48);
        QPair<QString, QString> texts(nominalText, shortText);

        mActionTexts.insert(action, texts);
    }
}


void UBBoardController::setToolbarTexts()
{
    QSize iconSize;

    // Compact icon-only toolbar: keep icons small so the toolbar height (and
    // the vertical space it steals from the canvas) stays minimal.
    if (mMainWindow->width() <= 1280)
        iconSize = QSize(22, 22);
    else
        iconSize = QSize(26, 26);

    mMainWindow->boardToolBar->setIconSize(iconSize);
    mMainWindow->webToolBar->setIconSize(iconSize);
    mMainWindow->documentToolBar->setIconSize(iconSize);

    foreach(QAction* action, mActionTexts.keys())
    {
        QPair<QString, QString> texts = mActionTexts.value(action);

        if (mMainWindow->width() <= 1024)
            action->setText(texts.second);
        else
            action->setText(texts.first);

        action->setToolTip(texts.first);
    }
}


QString UBBoardController::truncate(QString text, int maxWidth) const
{
    QFontMetricsF fontMetrics(mMainWindow->font());
    return fontMetrics.elidedText(text, Qt::ElideRight, maxWidth);
}


void UBBoardController::stylusToolDoubleClicked(int tool)
{
    if (tool == UBStylusTool::ZoomIn || tool == UBStylusTool::ZoomOut)
    {
        zoomRestore();
    }
    else if (tool == UBStylusTool::Hand)
    {
        centerRestore();
    }
}



void UBBoardController::addScene()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    persistViewPositionOnCurrentScene();
    persistCurrentScene(false,true);

    auto document = UBDocument::getDocument(selectedDocument());
    document->createPage(mActiveSceneIndex + 1);

    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    setActiveDocumentScene(mActiveSceneIndex + 1);
    QApplication::restoreOverrideCursor();

    UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
}

void UBBoardController::addScene(std::shared_ptr<UBGraphicsScene> scene, bool replaceActiveIfEmpty)
{
    if (scene)
    {
        std::shared_ptr<UBGraphicsScene> clone = scene->sceneDeepCopy();

        if (scene->document() && (scene->document() != selectedDocument()))
        {
            foreach(QUrl relativeFile, scene->relativeDependencies())
            {
                QString source = scene->document()->persistencePath() + "/" + relativeFile.path();
                QString destination = selectedDocument()->persistencePath() + "/" + relativeFile.path();

                UBFileSystemUtils::copy(source, destination, true);
            }
        }

        auto document = UBDocument::getDocument(selectedDocument());

        if (replaceActiveIfEmpty && mActiveScene->isEmpty())
        {
            document->insertPage(clone, mActiveSceneIndex);
            setActiveDocumentScene(mActiveSceneIndex);
            deleteScene(mActiveSceneIndex + 1);
        }
        else
        {
            persistCurrentScene(false,true);
            document->insertPage(clone, mActiveSceneIndex + 1);
            setActiveDocumentScene(mActiveSceneIndex + 1);
        }

        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
    }
}


void UBBoardController::addScene(std::shared_ptr<UBDocumentProxy> proxy, int sceneIndex, bool replaceActiveIfEmpty)
{
    std::shared_ptr<UBGraphicsScene> scene = UBPersistenceManager::persistenceManager()->loadDocumentScene(proxy, sceneIndex);

    if (scene)
    {
        addScene(scene, replaceActiveIfEmpty);
    }
}

void UBBoardController::duplicateScene(int nIndex)
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    persistCurrentScene(false,true);

    duplicatePage(nIndex);

    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    setActiveDocumentScene(nIndex + 1);
    QApplication::restoreOverrideCursor();
}

void UBBoardController::duplicateScene()
{
    if (UBApplication::applicationController->displayMode() != UBApplicationController::Board)
        return;
    duplicateScene(mActiveSceneIndex);
}

UBGraphicsItem *UBBoardController::duplicateItem(UBItem *item)
{
    if (!item)
        return NULL;

    UBGraphicsItem *retItem = NULL;

    mLastCreatedItem = NULL;

    QUrl sourceUrl;
    QByteArray pData;

    //common parameters for any item
    QPointF itemPos;
    QSizeF itemSize;

    QGraphicsItem *commonItem = dynamic_cast<QGraphicsItem*>(item);
    if (commonItem)
    {
        qreal shifting = UBSettings::settings()->objectFrameWidth;
        itemPos = commonItem->pos() + QPointF(shifting,shifting);
        itemSize = commonItem->boundingRect().size();
        commonItem->setSelected(false);

    }

    UBMimeType::Enum itemMimeType;

    QString srcFile = item->sourceUrl().toLocalFile();
    if (srcFile.isEmpty())
        srcFile = item->sourceUrl().toString();

    QString contentTypeHeader;
    if (!srcFile.isEmpty())
        contentTypeHeader = UBFileSystemUtils::mimeTypeFromFileName(srcFile);

    if(NULL != qgraphicsitem_cast<UBGraphicsGroupContainerItem*>(commonItem))
        itemMimeType = UBMimeType::Group;
    else
        itemMimeType = UBFileSystemUtils::mimeTypeFromString(contentTypeHeader);

    switch(static_cast<int>(itemMimeType))
    {
    case UBMimeType::AppleWidget:
    case UBMimeType::W3CWidget:
        {
            UBGraphicsWidgetItem *witem = dynamic_cast<UBGraphicsWidgetItem*>(item);
            if (witem)
            {
                sourceUrl = witem->getOwnFolder();
            }
        }break;

    case UBMimeType::Video:
    case UBMimeType::Audio:
        {
            UBGraphicsMediaItem *mitem = dynamic_cast<UBGraphicsMediaItem*>(item);
            if (mitem)
            {
                sourceUrl = mitem->mediaFileUrl();
                downloadURL(sourceUrl, srcFile, itemPos, QSize(itemSize.width(), itemSize.height()), false, false);
                return NULL; // async operation
            }
        }break;

    case UBMimeType::VectorImage:
        {
            UBGraphicsSvgItem *viitem = dynamic_cast<UBGraphicsSvgItem*>(item);
            if (viitem)
            {
                pData = viitem->fileData();
                sourceUrl = item->sourceUrl();
            }
        }break;

    case UBMimeType::RasterImage:
        {
            UBGraphicsPixmapItem *pixitem = dynamic_cast<UBGraphicsPixmapItem*>(item);
            if (pixitem)
            {
                 QBuffer buffer(&pData);
                 buffer.open(QIODevice::WriteOnly);
                 QString format = UBFileSystemUtils::extension(item->sourceUrl().toString(QUrl::DecodeReserved));
                 pixitem->pixmap().save(&buffer, format.toLatin1());
            }
        }break;

    case UBMimeType::Group:
    {
        UBGraphicsGroupContainerItem* groupItem = dynamic_cast<UBGraphicsGroupContainerItem*>(item);
        UBGraphicsGroupContainerItem* duplicatedGroup = NULL;

        QList<QGraphicsItem*> duplicatedItems;
        QList<QGraphicsItem*> children = groupItem->childItems();

        mActiveScene->setURStackEnable(false);
        foreach(QGraphicsItem* pIt, children){
            UBItem* pItem = dynamic_cast<UBItem*>(pIt);
            if(pItem)
            {
                QGraphicsItem * itemToGroup = dynamic_cast<QGraphicsItem *>(duplicateItem(pItem));
                if (itemToGroup)
                {
                    itemToGroup->setZValue(pIt->zValue());
                    itemToGroup->setData(UBGraphicsItemData::ItemOwnZValue, pIt->data(UBGraphicsItemData::ItemOwnZValue).toReal());
                    duplicatedItems.append(itemToGroup);
                }
            }
        }
        duplicatedGroup = mActiveScene->createGroup(duplicatedItems);
        duplicatedGroup->setTransform(groupItem->transform());
        groupItem->copyItemParameters(duplicatedGroup);
        groupItem->setSelected(false);


        retItem = dynamic_cast<UBGraphicsItem *>(duplicatedGroup);

        QGraphicsItem * itemToAdd = dynamic_cast<QGraphicsItem *>(retItem);
        if (itemToAdd)
        {
            mActiveScene->addItem(itemToAdd);
            itemToAdd->setSelected(true);
        }
        mActiveScene->setURStackEnable(true);
    }break;

    case UBMimeType::UNKNOWN:
        {
            QGraphicsItem *copiedItem = dynamic_cast<QGraphicsItem*>(item);
            QGraphicsItem *gitem = dynamic_cast<QGraphicsItem*>(item->deepCopy());
            if (gitem)
            {
                mActiveScene->addItem(gitem);

                if (copiedItem)
                {
                    if (mActiveScene->tools().contains(copiedItem))
                    {
                        mActiveScene->registerTool(gitem);
                    }
                }
                gitem->setPos(itemPos);

                mLastCreatedItem = gitem;
                gitem->setSelected(true);
            }
            retItem = dynamic_cast<UBGraphicsItem *>(gitem);
        }break;
    }

    if (retItem)
    {
        QGraphicsItem *graphicsRetItem = dynamic_cast<QGraphicsItem *>(retItem);
        if (mActiveScene->isURStackIsEnabled()) { //should be deleted after scene own undo stack implemented
             UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(mActiveScene, 0, graphicsRetItem);
             UBApplication::undoStack->push(uc);
        }
        return retItem;
    }

    UBItem *createdItem = downloadFinished(true, sourceUrl, QUrl::fromLocalFile(srcFile), contentTypeHeader, pData, itemPos, QSize(itemSize.width(), itemSize.height()), false);
    if (createdItem)
    {
        createdItem->setSourceUrl(item->sourceUrl());
        item->copyItemParameters(createdItem);

        QGraphicsItem *createdGitem = dynamic_cast<QGraphicsItem*>(createdItem);
        if (createdGitem)
            createdGitem->setPos(itemPos);
        mLastCreatedItem = dynamic_cast<QGraphicsItem*>(createdItem);
        mLastCreatedItem->setSelected(true);

        retItem = dynamic_cast<UBGraphicsItem *>(createdItem);
    }

    return retItem;
}

void UBBoardController::deleteScene(int nIndex)
{
    if (selectedDocument()->pageCount()>=2)
    {
        mDeletingSceneIndex = nIndex;
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        persistCurrentScene();
        UBApplication::showMessage(tr("Deleting page %1").arg(nIndex+1), true);

        auto document = UBDocument::getDocument(selectedDocument());
        document->deletePages({nIndex});

        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
        UBMetadataDcSubsetAdaptor::persist(selectedDocument());

        if (nIndex >= pageCount())
            nIndex = pageCount()-1;
        setActiveDocumentScene(nIndex);
        UBApplication::showMessage(tr("Page %1 deleted").arg(nIndex+1));
        QApplication::restoreOverrideCursor();
        mDeletingSceneIndex = -1;
    }
}


void UBBoardController::clearScene()
{
    if (mActiveScene)
    {
        freezeW3CWidgets(true);
        mActiveScene->clearContent(UBGraphicsScene::clearItemsAndAnnotations);
        updateActionStates();
        centerOnDefaultPosition();
    }
}


void UBBoardController::clearSceneItems()
{
    if (mActiveScene)
    {
        freezeW3CWidgets(true);
        mActiveScene->clearContent(UBGraphicsScene::clearItems);
        updateActionStates();
        centerOnDefaultPosition();
    }
}


void UBBoardController::clearSceneAnnotation()
{
    if (mActiveScene)
    {
        mActiveScene->clearContent(UBGraphicsScene::clearAnnotations);
        updateActionStates();
        centerOnDefaultPosition();
    }
}

void UBBoardController::clearSceneBackground()
{
    if (mActiveScene)
    {
        mActiveScene->clearContent(UBGraphicsScene::clearBackground);
        updateActionStates();
        centerOnDefaultPosition();
    }
}

void UBBoardController::showDocumentsDialog()
{
    if (selectedDocument())
        persistCurrentScene();

    UBApplication::mainWindow->actionLibrary->setChecked(false);

}

void UBBoardController::libraryDialogClosed(int ret)
{
    Q_UNUSED(ret);

    mMainWindow->actionLibrary->setChecked(false);
}


void UBBoardController::blackout()
{
    UBApplication::applicationController->blackout();
}

void UBBoardController::showKeyboard(bool show)
{
    if(show)
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

    if(UBSettings::settings()->useSystemOnScreenKeyboard->get().toBool())
        UBPlatformUtils::showOSK(show);
    else
        mPaletteManager->showVirtualKeyboard(show);
}


void UBBoardController::zoomIn(QPointF scenePoint)
{
    if (mControlView->transform().m11() > UB_MAX_ZOOM)
    {
        qApp->beep();
        return;
    }
    zoom(mZoomFactor, scenePoint);
}


void UBBoardController::zoomOut(QPointF scenePoint)
{
    if ((mControlView->horizontalScrollBar()->maximum() == 0) && (mControlView->verticalScrollBar()->maximum() == 0))
    {
        // Do not zoom out if we reached the maximum
        qApp->beep();
        return;
    }

    qreal newZoomFactor = 1 / mZoomFactor;

    zoom(newZoomFactor, scenePoint);
}


void UBBoardController::zoomRestore()
{
    QTransform tr;

    tr.scale(mSystemScaleFactor, mSystemScaleFactor);
    mControlView->setTransform(tr);

    centerRestore();

    emit zoomChanged(1.0);

    emit controlViewportChanged();
    mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
}


void UBBoardController::centerRestore()
{
    // reset transformation and scrollbar values, leaving room for the
    // left-docked stylus palette
    centerOnDefaultPosition();

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    // workaround: foreground not repainted after scrolling on Qt5 (fixed in Qt6)
    // setForegroundBrush internally invokes the private function uopdateAll() unconditionally
    mControlView->setForegroundBrush(mControlView->foregroundBrush());
#endif

    persistViewPositionOnCurrentScene();
    UBApplication::applicationController->adjustDisplayView();
}


void UBBoardController::centerOn(QPointF scenePoint) const
{
    // centerOn without using scroll bars
    const auto before = mControlView->transform();

    // create a transformation with the same scaling where the scenePoint is in the center
    QTransform after;
    after.scale(before.m11(), before.m22());
    after.translate(-scenePoint.x(), -scenePoint.y());
    mControlView->setTransform(after);

    if (UBApplication::applicationController)
    {
        UBApplication::applicationController->adjustDisplayView();
    }
}


void UBBoardController::centerOnDefaultPosition()
{
    if (!mControlView)
        return;

    qreal paletteClearance = 0.0;

    // The offset only makes sense for the left-docked (vertical) palette;
    // a bottom-docked palette must not shift the page horizontally.
    const bool verticalPalette =
            UBSettings::settings()->appToolBarOrientationVertical->get().toBool();

    UBStylusPalette* stylusPalette = mPaletteManager ? mPaletteManager->stylusPalette() : 0;

    // Use isHidden() rather than isVisible(): at startup the palette is
    // already shown/laid out but its top-level window may not be visible yet,
    // which would otherwise suppress the offset on initial page load.
    if (verticalPalette && stylusPalette && !stylusPalette->isHidden()
            && stylusPalette->width() > 0)
    {
        // Reserve half of the stylus palette width so the page content is
        // centred in the canvas area to the right of the palette.
        paletteClearance = stylusPalette->width() / 2.0;
    }

    const qreal scale = mControlView->transform().m11();
    const qreal sceneOffset = (scale != 0.0) ? paletteClearance / scale : paletteClearance;

    // Centering on a point left of the origin shifts the page to the right.
    const QPointF target(-sceneOffset, 0.0);

    centerOn(target);
    mControlView->centerOn(target);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    // workaround: foreground not repainted after scrolling on Qt5
    mControlView->setForegroundBrush(mControlView->foregroundBrush());
#endif

    persistViewPositionOnCurrentScene();
}


void UBBoardController::zoom(const qreal ratio, QPointF scenePoint)
{
    qreal currentZoom = ratio * mControlView->transform().m11() / mSystemScaleFactor;
    qreal usedRatio = ratio;

    if (currentZoom > UB_MAX_ZOOM)
    {
        currentZoom = UB_MAX_ZOOM;
        usedRatio = currentZoom * mSystemScaleFactor / mControlView->transform().m11();
    }

    /*
     * The shiftFactor is calculated from the condition that the scenePoint should have the
     * same coordinates on the view after zooming. Let m11, m31 be the transformation parameters
     * before zoom and m11', m31' the parameters after zoom. The equations for scenePoint.x:
     *   m11' = m11 * ratio
     *   x * m11 + m31 = x * m11' + m31'
     * We now solve this equation to get the additional translation m31' - m31:
     *   m31' - m31 = x * (m11 - m11')
     *              = x * m11 * (1 - ratio)
     *              = x * m11' * (1 - ratio) / ratio
     * The translate function works in scene coordinates and multiplies its parameter internally
     * by the scale factor m11', so we omit this factor in the function call below.
     */
    const auto shiftFactor = (1 - usedRatio) / usedRatio;
    mControlView->scale(usedRatio, usedRatio);
    mControlView->translate(scenePoint.x() * shiftFactor, scenePoint.y() * shiftFactor);

    emit zoomChanged(currentZoom);
    UBApplication::applicationController->adjustDisplayView();

    emit controlViewportChanged();
    mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
}


void UBBoardController::handScroll(qreal dx, qreal dy)
{
    qreal antiScaleRatio = 1/(mSystemScaleFactor * currentZoom());
    mControlView->translate(dx*antiScaleRatio, dy*antiScaleRatio);

    UBApplication::applicationController->adjustDisplayView();

    emit controlViewportChanged();
}

void UBBoardController::persistViewPositionOnCurrentScene() const
{
    if (mActiveScene)
    {
        // calculate center from transformation
        const QPointF viewRelativeCenter = mControlView->transform().inverted().map(QPointF{0, 0});
        UBGraphicsScene::SceneViewState viewState
        {
            mControlView->transform().m11() / mSystemScaleFactor,
            mControlView->horizontalScrollBar()->value(),
            mControlView->verticalScrollBar()->value(),
            viewRelativeCenter
        };

        mActiveScene->setViewState(viewState);
    }
}

void UBBoardController::restoreViewPositionOnCurrentScene() const
{
    if (mActiveScene)
    {
        const auto viewState = mActiveScene->viewState();
        mControlView->horizontalScrollBar()->setValue(viewState.horizontalPosition);
        mControlView->verticalScrollBar()->setValue(viewState.verticalPostition);
        QTransform transform;
        double scale = viewState.zoomFactor * mSystemScaleFactor;
        transform.scale(scale, scale);
        mControlView->setTransform(transform);
        centerOn(viewState.mLastSceneCenter);
    }
}

void UBBoardController::previousScene()
{
    if (mActiveSceneIndex > 0)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(mActiveSceneIndex - 1);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}


void UBBoardController::nextScene()
{
    if (mActiveSceneIndex < selectedDocument()->pageCount() - 1)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(mActiveSceneIndex + 1);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}


void UBBoardController::firstScene()
{
    if (mActiveSceneIndex > 0)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(0);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}


void UBBoardController::lastScene()
{
    if (mActiveSceneIndex < selectedDocument()->pageCount() - 1)
    {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        setActiveDocumentScene(selectedDocument()->pageCount() - 1);
        QApplication::restoreOverrideCursor();
    }

    updateActionStates();
}

void UBBoardController::downloadURL(const QUrl& url, QString contentSourceUrl, const QPointF& pPos, const QSize& pSize, bool isBackground, bool internalData)
{
    QString sUrl = url.toString();
    qDebug() << "something has been dropped on the board! Url is: " << sUrl.left(255);

    QGraphicsItem *oldBackgroundObject = NULL;
    if (isBackground)
        oldBackgroundObject = mActiveScene->backgroundObject();

    if(url.scheme() == "openboardtool")
    {
        downloadFinished(true, url, QUrl(), "application/openboard-tool", QByteArray(), pPos, pSize, isBackground);
    }
    else if (url.scheme() == "file" || url.scheme() == "")
    {
        QUrl formedUrl = url.scheme() == "file" ? url : QUrl::fromLocalFile(sUrl);
        QString fileName = formedUrl.toLocalFile();
        QString contentType = UBFileSystemUtils::mimeTypeFromFileName(fileName);

        // directly add local file to document without copying
        QFile file(fileName);
        QByteArray data;

        if (file.open(QIODevice::ReadOnly))
        {
            data = file.readAll();
        }

        downloadFinished(true, formedUrl, QUrl(), contentType, data, pPos, pSize, isBackground, internalData);
        file.close();
    }
    else
    {
        // When we fall there, it means that we are dropping something from the web to the board
        sDownloadFileDesc desc;
        desc.modal = true;
        desc.srcUrl = sUrl;
        desc.currentSize = 0;
        desc.name = url.scheme() == "data" ? "Local data" : url.fileName();
        desc.totalSize = 0; // The total size will be retrieved during the download
        desc.pos = pPos;
        desc.size = pSize;
        desc.isBackground = isBackground;

        UBDownloadManager::downloadManager()->addFileToDownload(desc);
    }

    if (isBackground && oldBackgroundObject != mActiveScene->backgroundObject())
    {
        if (mActiveScene->isURStackIsEnabled()) { //should be deleted after scene own undo stack implemented
            UBGraphicsItemUndoCommand* uc = new UBGraphicsItemUndoCommand(mActiveScene, oldBackgroundObject, mActiveScene->backgroundObject());
            UBApplication::undoStack->push(uc);
        }
    }


}


UBItem *UBBoardController::downloadFinished(bool pSuccess, QUrl sourceUrl, QUrl contentUrl, QString pContentTypeHeader,
                                            QByteArray pData, QPointF pPos, QSize pSize,
                                            bool isBackground, bool internalData)
{
    QString mimeType = pContentTypeHeader;

    // In some cases "image/jpeg;charset=" is retourned by the drag-n-drop. That is
    // why we will check if an ; exists and take the first part (the standard allows this kind of mimetype)
    if(mimeType.isEmpty())
      mimeType = UBFileSystemUtils::mimeTypeFromFileName(sourceUrl.toString());

    int position=mimeType.indexOf(";");
    if(position != -1)
        mimeType=mimeType.left(position);

    UBMimeType::Enum itemMimeType = UBFileSystemUtils::mimeTypeFromString(mimeType);

    if (!pSuccess)
    {
        UBApplication::showMessage(tr("Downloading content %1 failed").arg(sourceUrl.toString()));
        return NULL;
    }


    mActiveScene->deselectAllItems();
    const QString scheme = sourceUrl.scheme();

    if (scheme != "file" && scheme != "openboardtool" && scheme != "data")
        UBApplication::showMessage(tr("Download finished"));

    if (UBMimeType::RasterImage == itemMimeType)
    {

        qDebug() << "accepting mime type" << mimeType << "as raster image";

        if (pData.length() == 0)
        {
            QFile file(sourceUrl.toLocalFile());

            if (file.open(QFile::ReadOnly))
            {
                pData = file.readAll();
                file.close();
            }
        }

        UBGraphicsPixmapItem* pixItem = mActiveScene->addImage(pData, nullptr, pPos, 1.);

        if (scheme == "data")
        {
            // create a shorter, but still unique URL using a hash function
            QCryptographicHash hash(QCryptographicHash::Md5);
            hash.addData(sourceUrl.toString().toLatin1());
            QByteArray result = hash.result();
            QString hashedUrl = "md5:" + result.toBase64();
            pixItem->setSourceUrl(hashedUrl);
        }
        else
        {
            pixItem->setSourceUrl(sourceUrl);
        }

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(pixItem, true);
        }
        else
        {
            mActiveScene->scaleToFitDocumentSize(pixItem, true, UBSettings::objectInControlViewMargin);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return pixItem;
    }
    else if (UBMimeType::VectorImage == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as vector image";

        UBGraphicsSvgItem* svgItem = mActiveScene->addSvg(sourceUrl, pPos, pData);
        svgItem->setSourceUrl(sourceUrl);

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(svgItem);
        }
        else
        {
            mActiveScene->scaleToFitDocumentSize(svgItem, true, UBSettings::objectInControlViewMargin);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return svgItem;
    }
    else if (UBMimeType::AppleWidget == itemMimeType) //mime type invented by us :-(
    {
        qDebug() << "accepting mime type" << mimeType << "as Apple widget";

        QUrl widgetUrl = sourceUrl;

        if (pData.length() > 0)
        {
            widgetUrl = expandWidgetToTempDir(pData, "wdgt");
        }

        UBGraphicsWidgetItem* appleWidgetItem = mActiveScene->addAppleWidget(widgetUrl, pPos);

        appleWidgetItem->setSourceUrl(sourceUrl);

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(appleWidgetItem);
        }
        else
        {
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return appleWidgetItem;
    }
    else if (UBMimeType::W3CWidget == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as W3C widget";
        QUrl widgetUrl = sourceUrl;

        if (pData.length() > 0)
        {
            widgetUrl = expandWidgetToTempDir(pData);
        }

        UBGraphicsWidgetItem *w3cWidgetItem = addW3cWidget(widgetUrl, pPos);

        if (isBackground)
        {
            mActiveScene->setAsBackgroundObject(w3cWidgetItem);
        }
        else
        {
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }

        return w3cWidgetItem;
    }
    else if (UBMimeType::Video == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as video";

        UBGraphicsMediaItem *mediaVideoItem = 0;
        QUuid uuid = QUuid::createUuid();
        if (pData.length() > 0)
        {
            QString destFile;
            bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
                sourceUrl.toString(),
                UBPersistenceManager::videoDirectory,
                uuid,
                destFile,
                &pData);
            if (!b)
            {
                UBApplication::showMessage(tr("Add file operation failed: file copying error"));
                return NULL;
            }

            QUrl url = QUrl::fromLocalFile(destFile);

            mediaVideoItem = mActiveScene->addMedia(url, false, pPos);
        }
        else
        {
            qDebug() << sourceUrl.toString();
            mediaVideoItem = addVideo(sourceUrl, false, pPos, true);
        }

        if(mediaVideoItem){
            if (contentUrl.isEmpty())
                mediaVideoItem->setSourceUrl(sourceUrl);
            else
                mediaVideoItem->setSourceUrl(contentUrl);
            mediaVideoItem->setUuid(uuid);
            connect(this, SIGNAL(activeSceneChanged()), mediaVideoItem, SLOT(activeSceneChanged()));
        }

        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

        return mediaVideoItem;
    }
    else if (UBMimeType::Audio == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as audio";

        UBGraphicsMediaItem *audioMediaItem = 0;

        QUuid uuid = QUuid::createUuid();
        if (pData.length() > 0)
        {
            QString destFile;
            bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
                sourceUrl.toString(),
                UBPersistenceManager::audioDirectory,
                uuid,
                destFile,
                &pData);
            if (!b)
            {
                UBApplication::showMessage(tr("Add file operation failed: file copying error"));
                return NULL;
            }

            QUrl url = QUrl::fromLocalFile(destFile);

            audioMediaItem = mActiveScene->addMedia(url, false, pPos);
        }
        else
        {
            audioMediaItem = addAudio(sourceUrl, false, pPos, true);
        }

        if(audioMediaItem){
            if (contentUrl.isEmpty())
                audioMediaItem->setSourceUrl(sourceUrl);
            else
                audioMediaItem->setSourceUrl(contentUrl);
            audioMediaItem->setUuid(uuid);
            connect(this, SIGNAL(activeSceneChanged()), audioMediaItem, SLOT(activeSceneChanged()));
        }

        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

        return audioMediaItem;
    }
    else if (UBMimeType::Flash == itemMimeType)
    {

        qDebug() << "accepting mime type" << mimeType << "as flash";

        QString sUrl = sourceUrl.toString();

        if (sUrl.startsWith("file://") || sUrl.startsWith("/"))
        {
            sUrl = sourceUrl.toLocalFile();
        }

        QSize size;

        if (pSize.height() > 0 && pSize.width() > 0)
            size = pSize;
        else
            size = mActiveScene->nominalSize() * .8;

        Q_UNUSED(internalData)

        QString widgetUrl = UBGraphicsW3CWidgetItem::createNPAPIWrapper(sUrl, mimeType, size);
        UBFileSystemUtils::deleteFile(sourceUrl.toLocalFile());
        emit npapiWidgetCreated(widgetUrl);

        if (widgetUrl.length() > 0)
        {
            UBGraphicsWidgetItem *widgetItem = mActiveScene->addW3CWidget(QUrl::fromLocalFile(widgetUrl), pPos);
            widgetItem->setUuid(QUuid::createUuid());
            widgetItem->setSourceUrl(QUrl::fromLocalFile(widgetUrl));
            qDebug() << widgetItem->getOwnFolder();
            qDebug() << widgetItem->getSnapshotPath();

            widgetItem->setSnapshotPath(widgetItem->getOwnFolder());

            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);

            return widgetItem;
        }
    }
    else if (UBMimeType::PDF == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "as PDF";
        qDebug() << "pdf data length: " << pData.size();
        qDebug() << "sourceurl : " + sourceUrl.toString();
        QString sUrl = sourceUrl.toString();

        int numberOfImportedDocuments = 0;

        if (!sourceUrl.isEmpty() && (sUrl.startsWith("file://") || sUrl.startsWith("/")))
        {
            QStringList fileNames;
            fileNames << sourceUrl.toLocalFile();
            numberOfImportedDocuments = UBDocumentManager::documentManager()->addFilesToDocument(selectedDocument(), fileNames);
        }
        else if(pData.size()){
            QTemporaryFile pdfFile("XXXXXX.pdf");
            if (pdfFile.open())
            {
                pdfFile.write(pData);
                pdfFile.close();
                QStringList fileNames;
                fileNames << pdfFile.fileName();
                numberOfImportedDocuments = UBDocumentManager::documentManager()->addFilesToDocument(selectedDocument(), fileNames);
            }
        }

        if (numberOfImportedDocuments > 0)
        {
            QDateTime now = QDateTime::currentDateTime();
            selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
            updateActionStates();
        }
    }
    else if (UBMimeType::OpenboardTool == itemMimeType)
    {
        qDebug() << "accepting mime type" << mimeType << "OpenBoard Tool";

        if (sourceUrl.toString() == UBToolsManager::manager()->compass.id)
        {
            mActiveScene->addCompass(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->ruler.id)
        {
            mActiveScene->addRuler(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->axes.id)
        {
            mActiveScene->addAxes(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->protractor.id)
        {
            mActiveScene->addProtractor(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->triangle.id)
        {
            mActiveScene->addTriangle(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->cache.id)
        {
            mActiveScene->addCache();
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->magnifier.id)
        {
            UBMagnifierParams params;
            params.x = controlContainer()->geometry().width() / 2;
            params.y = controlContainer()->geometry().height() / 2;
            params.zoom = 2;
            params.sizePercentFromScene = 20;
            mActiveScene->addMagnifier(params);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else if (sourceUrl.toString() == UBToolsManager::manager()->mask.id)
        {
            mActiveScene->addMask(pPos);
            UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Selector);
        }
        else
        {
            UBApplication::showMessage(tr("Unknown tool type %1").arg(sourceUrl.toString()));
        }
    }
    else if (UBMimeType::Html == itemMimeType)
    {
        if (!mEmbedController)
        {
            mEmbedController = new UBEmbedController(mControlView);
        }

        static const QRegularExpression matchTitle("<title>([^<]*)</title>");

        QRegularExpressionMatch match = matchTitle.match(pData);
        QString title = match.hasMatch() ? match.captured(1) : tr("Untitled");

        mEmbedController->pageTitleChanged(title);
        mEmbedController->pageUrlChanged(sourceUrl);
        mEmbedController->showEmbedDialog();

        UBEmbedParser* parser = new UBEmbedParser(this);
        connect(parser, &UBEmbedParser::parseResult, this, [this,parser](bool hasEmbeddedContent){
            QList<UBEmbedContent> list = parser->embeddedContent();
            mEmbedController->updateListOfEmbeddableContent(list);
            parser->deleteLater();
        });

        parser->parse(pData);
    }
    else if (UBMimeType::Document == itemMimeType)
    {
        QString documentFolderName = sourceUrl.toString().section('/', -2, -2); //section before "/metadata.rdf" is documentFolderName

        std::shared_ptr<UBDocumentProxy> document = UBPersistenceManager::persistenceManager()->mDocumentTreeStructureModel->findDocumentByFolderName(documentFolderName);

        if (document)
        {
            setActiveDocumentScene(document, document->lastVisitedSceneIndex());
        }
        else
        {
            UBApplication::showMessage(tr("Could not find document."));
        }
    }
    else
    {
        UBApplication::showMessage(tr("Unknown content type %1").arg(pContentTypeHeader));
        qWarning() << "ignoring mime type" << pContentTypeHeader ;
    }

    return NULL;
}

std::shared_ptr<UBGraphicsScene> UBBoardController::setActiveDocumentScene(int pSceneIndex)
{
    return setActiveDocumentScene(selectedDocument(), pSceneIndex);
}

std::shared_ptr<UBGraphicsScene> UBBoardController::setActiveDocumentScene(std::shared_ptr<UBDocumentProxy> pDocumentProxy, const int pSceneIndex, bool forceReload, bool onImport)
{
    UBApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    persistViewPositionOnCurrentScene();

    // Finalize an in-progress free triangle and stop the Pen/Marker
    // hold-to-straighten timer before leaving the page.
    if (mActiveScene)
    {
        mActiveScene->commitShapeEditing();
        mActiveScene->stopStraightenHold();
    }

    bool documentChange = selectedDocument() != pDocumentProxy;

    int index = pSceneIndex;
    int sceneCount = pDocumentProxy->pageCount();
    if (index >= sceneCount && sceneCount > 0)
        index = sceneCount - 1;

    std::shared_ptr<UBGraphicsScene> targetScene = UBPersistenceManager::persistenceManager()->loadDocumentScene(pDocumentProxy, index);

    bool sceneChange = targetScene != mActiveScene;

    if (targetScene)
    {
        if (mActiveScene && !onImport)
        {
            persistCurrentScene();
            freezeW3CWidgets(true);
            ClearUndoStack();
        }else
        {
            UBApplication::undoStack->clear();
        }

        mActiveScene = targetScene;
        mActiveSceneIndex = index;

        setDocument(pDocumentProxy, forceReload);

        updateSystemScaleFactor();

        if (mControlView->scene())
        {
            disconnect(UBApplication::undoStack.data(), SIGNAL(indexChanged(int)), mControlView->scene().get(), SLOT(updateSelectionFrameWrapper(int)));
        }

        mControlView->setScene(mActiveScene.get());
        connect(UBApplication::undoStack.data(), SIGNAL(indexChanged(int)), mControlView->scene().get(), SLOT(updateSelectionFrameWrapper(int)));

        mDisplayView->setScene(mActiveScene.get());
        mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
        pDocumentProxy->setDefaultDocumentSize(mActiveScene->nominalSize());
        updatePageSizeState();

        // Every page load/switch restores the per-page view state above; apply
        // the stylus-palette clearance offset afterwards so pages are centered
        // in the visible canvas by default.
        centerOnDefaultPosition();

        adjustDisplayViews();

        UBSettings::settings()->setDarkBackground(mActiveScene->isDarkBackground());
        UBSettings::settings()->setPageBackground(mActiveScene->pageBackground());

        freezeW3CWidgets(false);

        selectionChanged();

        updateBackgroundActionsState(mActiveScene->isDarkBackground(), mActiveScene->pageBackground());

        if (documentChange)
        {
            UBGraphicsTextItem::lastUsedTextColor = QColor(Qt::black);
        }

        if (sceneChange)
        {
            emit activeSceneChanged();
        }

        pDocumentProxy->setLastVisitedSceneIndex(mActiveSceneIndex);

        UBFeaturesController* featuresController = paletteManager()->featuresWidget()->getFeaturesController();

        QUrl url = QUrl::fromLocalFile(pDocumentProxy->persistencePath() + "/metadata.rdf");
        QString documentFolderName = pDocumentProxy->documentFolderName();

        if (!featuresController->isDocumentInFavoriteList(documentFolderName) && !featuresController->isInRecentlyOpenDocuments(documentFolderName))
        {
            featuresController->addToFavorite(url, pDocumentProxy->name(), true);

            // keep recent UBDocument instances alive for fast switching
            auto document = UBDocument::getDocument(pDocumentProxy);

            if (!mRecentDocuments.contains(document))
            {
                mRecentDocuments.append(UBDocument::getDocument(pDocumentProxy));
            }
        }

        auto document = UBDocument::getDocument(pDocumentProxy);
        document->thumbnailScene()->hightlightItem(mActiveSceneIndex, true);
    }
    else
    {
        qWarning() << "could not load document scene : '" << pDocumentProxy->persistencePath() << "', page index : " << pSceneIndex;
    }
    UBApplication::restoreOverrideCursor();

    return targetScene;
}

void UBBoardController::findUniquesItems(const QUndoCommand *parent, QSet<QGraphicsItem*> &items)
{
    if (parent->childCount()) {
        for (int i = 0; i < parent->childCount(); i++) {
            findUniquesItems(parent->child(i), items);
        }
    }

    // Undo command transaction macros. Process separatedly
    if (parent->text() == UBSettings::undoCommandTransactionName) {
        return;
    }

    const UBUndoCommand *undoCmd = static_cast<const UBUndoCommand*>(parent);
    if(undoCmd->getType() != UBUndoType::undotype_GRAPHICITEM)
        return;

    const UBGraphicsItemUndoCommand *cmd = dynamic_cast<const UBGraphicsItemUndoCommand*>(parent);

    // go through all added and removed objects, for create list of unique objects
    // grouped items will be deleted by groups, so we don't need do delete that items.
    QSetIterator<QGraphicsItem*> itAdded(cmd->GetAddedList());
    while (itAdded.hasNext())
    {
        QGraphicsItem* item = itAdded.next();
        if (!items.contains(item) &&
            !(item->parentItem() && UBGraphicsGroupContainerItem::Type == item->parentItem()->type()) &&
            !items.contains(item->parentItem())
            )
        {
            items.insert(item);
        }
    }

    QSetIterator<QGraphicsItem*> itRemoved(cmd->GetRemovedList());
    while (itRemoved.hasNext())
    {
        QGraphicsItem* item = itRemoved.next();
        if (!items.contains(item) &&
            !(item->parentItem() && UBGraphicsGroupContainerItem::Type == item->parentItem()->type()) &&
            !items.contains(item->parentItem())
            )
        {
            items.insert(item);
        }
    }
}

void UBBoardController::ClearUndoStack()
{
    QSet<QGraphicsItem*> uniqueItems;
    // go through all stack command
    for (int i = 0; i < UBApplication::undoStack->count(); i++) {
        findUniquesItems(UBApplication::undoStack->command(i), uniqueItems);
    }

    // Get items from clipboard in order not to delete an item that was cut
    // (using source URL of graphics items as a surrogate for equality testing)
    // This ensures that we can cut and paste a media item, widget, etc. from one page to the next.
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData* data = clipboard->mimeData();
    QList<QUrl> sourceURLs;

    if (data && data->hasFormat(UBApplication::mimeTypeUniboardPageItem)) {
        const UBMimeDataGraphicsItem* mimeDataGI = qobject_cast <const UBMimeDataGraphicsItem*>(data);

        if (mimeDataGI) {
            foreach (UBItem* sourceItem, mimeDataGI->items()) {
                sourceURLs << sourceItem->sourceUrl();
            }
        }
    }

    // go through all unique items, and check, if they are on scene, or not.
    // if not on scene, than item can be deleted
    QSetIterator<QGraphicsItem*> itUniq(uniqueItems);
    while (itUniq.hasNext())
    {
        QGraphicsItem* item = itUniq.next();
        UBGraphicsScene* scene = nullptr;
        if (item->scene()) {
            scene = dynamic_cast<UBGraphicsScene*>(item->scene());
        }

        bool inClipboard = false;
        UBItem* ubi = dynamic_cast<UBItem*>(item);
        if (ubi && sourceURLs.contains(ubi->sourceUrl()))
            inClipboard = true;

        if(!scene && !inClipboard)
        {
            if (!mActiveScene->deleteItem(item)){
                delete item;
                item = 0;
            }
        }
    }

    // clear stack, and command list
    UBApplication::undoStack->clear();
}

void UBBoardController::adjustDisplayViews()
{
    if (UBApplication::applicationController)
    {
        UBApplication::applicationController->adjustDisplayView();
        UBApplication::applicationController->adjustPreviousViews(mActiveSceneIndex, selectedDocument());
    }
}


int UBBoardController::autosaveTimeoutFromSettings() const
{
    int value = UBSettings::settings()->autoSaveInterval->get().toInt();
    int minute = 60 * 1000;

    return value * minute;
}

void UBBoardController::changeBackground(bool isDark, UBPageBackground pageBackground)
{
    bool currentIsDark = mActiveScene->isDarkBackground();
    UBPageBackground currentBackgroundType = mActiveScene->pageBackground();

    if ((isDark != currentIsDark) || (currentBackgroundType != pageBackground))
    {
        UBSettings::settings()->setDarkBackground(isDark);
        UBSettings::settings()->setPageBackground(pageBackground);

        mActiveScene->setBackground(isDark, pageBackground);

        emit backgroundChanged();
    }
}

void UBBoardController::boardViewResized(QResizeEvent* event)
{
    Q_UNUSED(event);

    int innerMargin = UBSettings::boardMargin;
    int userHeight = mControlContainer->height() - (2 * innerMargin);

    mMessageWindow->move(innerMargin, innerMargin + userHeight - mMessageWindow->height());
    mMessageWindow->adjustSizeAndPosition();

    UBApplication::applicationController->initViewState(
                mControlView->horizontalScrollBar()->value(),
                mControlView->verticalScrollBar()->value());

    updateSystemScaleFactor();

    centerOnDefaultPosition();

    if (mDisplayView && UBApplication::displayManager->hasDisplay()) {
        UBApplication::applicationController->adjustDisplayView();
        mDisplayView->centerOn(0,0);
        setBoxing(mDisplayView->geometry());
    }

    mPaletteManager->containerResized();

    UBApplication::boardController->controlView()->scene()->moveMagnifier();

}


void UBBoardController::showMessage(const QString& message, bool showSpinningWheel)
{
    mMessageWindow->showMessage(message, showSpinningWheel);
}


void UBBoardController::hideMessage()
{
    mMessageWindow->hideMessage();
}


void UBBoardController::setDisabled(bool disable)
{
    mMainWindow->boardToolBar->setDisabled(disable);
    mControlView->setDisabled(disable);
}


void UBBoardController::selectionChanged()
{
    updateActionStates();
    emit pageSelectionChanged(activeSceneIndex());
}


void UBBoardController::undoRedoStateChange(bool canUndo)
{
    Q_UNUSED(canUndo);

    mMainWindow->actionUndo->setEnabled(UBApplication::undoStack->canUndo());
    mMainWindow->actionRedo->setEnabled(UBApplication::undoStack->canRedo());

    updateActionStates();
}


void UBBoardController::updateActionStates()
{
    mMainWindow->actionBack->setEnabled(selectedDocument() && (mActiveSceneIndex > 0));
    mMainWindow->actionForward->setEnabled(selectedDocument() && (mActiveSceneIndex < selectedDocument()->pageCount() - 1));
    mMainWindow->actionErase->setEnabled(mActiveScene && !mActiveScene->isEmpty());
}


std::shared_ptr<UBGraphicsScene> UBBoardController::activeScene() const
{
    return mActiveScene;
}


int UBBoardController::activeSceneIndex() const
{
    return mActiveSceneIndex;
}

void UBBoardController::setActiveSceneIndex(int i)
{
    mActiveSceneIndex = i;
}

void UBBoardController::documentSceneChanged(std::shared_ptr<UBDocumentProxy> pDocumentProxy, int pIndex)
{
    Q_UNUSED(pIndex);

    if(selectedDocument() == pDocumentProxy)
    {
        setActiveDocumentScene(mActiveSceneIndex);
    }
}

void UBBoardController::autosaveTimeout()
{
    if (UBApplication::applicationController->displayMode() != UBApplicationController::Board) {
        //perform autosave only in board mode
        return;
    }

    saveData(sf_showProgress);
    UBSettings::settings()->save();
}

void UBBoardController::appMainModeChanged(UBApplicationController::MainMode md)
{
    int autoSaveInterval = autosaveTimeoutFromSettings();
    if (!autoSaveInterval) {
        return;
    }

    if (!mAutosaveTimer) {
        mAutosaveTimer = new QTimer(this);
        connect(mAutosaveTimer, SIGNAL(timeout()), this, SLOT(autosaveTimeout()));
    }

    if (md == UBApplicationController::Board) {
        mAutosaveTimer->start(autoSaveInterval);
    } else if (mAutosaveTimer->isActive()) {
        mAutosaveTimer->stop();
    }
}

void UBBoardController::closing()
{
    mIsClosing = true;
    lastWindowClosed();
    ClearUndoStack();
#ifdef Q_OS_OSX
    if (!UBPlatformUtils::errorOpeningVirtualKeyboard)
        showKeyboard(false);
#else
        showKeyboard(false);
#endif
}

void UBBoardController::lastWindowClosed()
{
    if (!mCleanupDone)
    {
        if (initialDocumentScene() && initialDocumentScene()->document())
        {
            if (initialDocumentScene()->isEmpty() && (initialDocumentScene()->document()->documentDate() == initialDocumentScene()->document()->lastUpdate()))
            {
                // intial scene or document have not been modified at all, so we can delete the document.
                UBPersistenceManager::persistenceManager()->deleteDocument(initialDocumentScene()->document());

                //if current scene is not the initial document scene, we still need to persist it to ensure no data can be lost this way
                if (activeScene() != initialDocumentScene())
                {
                    persistCurrentScene();
                    UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
                }
            }
            else
            {
                // if intial scene or document changed, then rather the initial document scene is the current scene,
                // or current scene changed and initial document scene has already been persisted.
                // Now, we just persist the current scene before closing the app, to ensure no data can be lost this way.
                persistCurrentScene();
                UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
            }
        }
        else
        { //should not happen ?
            persistCurrentScene();
            UBPersistenceManager::persistenceManager()->persistDocumentMetadata(selectedDocument());
        }

        mCleanupDone = true;
    }
}



void UBBoardController::setColorIndex(int pColorIndex)
{
    UBDrawingController* drawingController = UBDrawingController::drawingController();

    // When the laser pointer is active, clicking a swatch only updates the
    // laser colour. It must not switch the current tool back to the pen.
    if (drawingController->stylusTool() == UBStylusTool::Laser)
    {
        drawingController->setColorIndex(pColorIndex);

        const QList<QColor> colors =
                UBSettings::settings()->penColors(UBSettings::settings()->isDarkBackground());

        if (pColorIndex >= 0 && pColorIndex < colors.size())
            drawingController->setLaserColor(colors.at(pColorIndex));

        return;
    }

    UBDrawingController::drawingController()->setColorIndex(pColorIndex);

    // The Shape tool owns its selected colour index; selecting a swatch while
    // it is active must not update the Pen colour caches or emit penColorChanged.
    if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Shape)
        return;

    if (UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Marker &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Line &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Shape &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Text &&
            UBDrawingController::drawingController()->stylusTool() != UBStylusTool::Selector)
    {
        UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
    }

    if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Pen ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Line ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Shape ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Text ||
            UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Selector)
    {
        mPenColorOnDarkBackground = UBSettings::settings()->penColors(true).at(pColorIndex);
        mPenColorOnLightBackground = UBSettings::settings()->penColors(false).at(pColorIndex);

        if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Selector)
        {
            // If we are in mode board, then do that
            if(UBApplication::applicationController->displayMode() == UBApplicationController::Board)
            {
                UBDrawingController::drawingController()->setStylusTool(UBStylusTool::Pen);
                mMainWindow->actionPen->setChecked(true);
            }
        }

        emit penColorChanged();
    }
    else if (UBDrawingController::drawingController()->stylusTool() == UBStylusTool::Marker)
    {
        mMarkerColorOnDarkBackground = UBSettings::settings()->markerColors(true).at(pColorIndex);
        mMarkerColorOnLightBackground = UBSettings::settings()->markerColors(false).at(pColorIndex);
    }
}

void UBBoardController::colorPaletteChanged()
{
    mPenColorOnDarkBackground = UBSettings::settings()->penColor(true);
    mPenColorOnLightBackground = UBSettings::settings()->penColor(false);
    mMarkerColorOnDarkBackground = UBSettings::settings()->markerColor(true);
    mMarkerColorOnLightBackground = UBSettings::settings()->markerColor(false);
}


void UBBoardController::openCustomColorDialog()
{
    UBDrawingController *drawingController = UBDrawingController::drawingController();

    const QColor color = QColorDialog::getColor(drawingController->currentToolColor(),
                                                 mMainWindow,
                                                 tr("Select Custom Color"));

    if (!color.isValid())
        return;

    // Compute and persist the dark/light pair, then select the most recent
    // custom slot exactly like a regular swatch would.
    drawingController->setCustomColor(color);
    setColorIndex(UBDrawingController::CustomColorStartIndex);
}


void UBBoardController::openLineWidthSlider()
{
    showWidthSliderPopup(qobject_cast<QAction *>(sender()), true);
}


void UBBoardController::openEraserWidthSlider()
{
    showWidthSliderPopup(qobject_cast<QAction *>(sender()), false);
}


void UBBoardController::showWidthSliderPopup(QAction *anchorAction, bool lineWidth)
{
    UBSettings *settings = UBSettings::settings();
    UBDrawingController *drawingController = UBDrawingController::drawingController();

    const int minimum = lineWidth ? 1 : 10;
    const int maximum = lineWidth ? 60 : 300;

    const qreal currentWidth = lineWidth
            ? (drawingController->stylusTool() == UBStylusTool::Marker
                   ? settings->markerStrongWidth()
                   : settings->penStrongWidth())
            : settings->eraserStrongWidth();

    QMenu menu(mMainWindow);
    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);
    QLabel *valueLabel = new QLabel(container);
    QSlider *slider = new QSlider(Qt::Horizontal, container);

    slider->setRange(minimum, maximum);
    slider->setValue(qBound(minimum, qRound(currentWidth), maximum));
    slider->setSingleStep(1);
    slider->setPageStep(lineWidth ? 5 : 10);
    slider->setTickPosition(QSlider::TicksBelow);
    slider->setTickInterval(lineWidth ? 10 : 50);
    slider->setMinimumWidth(200);

    valueLabel->setAlignment(Qt::AlignCenter);
    valueLabel->setText(tr("%1 px").arg(slider->value()));

    layout->addWidget(valueLabel);
    layout->addWidget(slider);
    layout->setContentsMargins(10, 8, 10, 8);
    container->setLayout(layout);

    QWidgetAction *widgetAction = new QWidgetAction(&menu);
    widgetAction->setDefaultWidget(container);
    menu.addAction(widgetAction);

    connect(slider, &QSlider::valueChanged, &menu, [this, lineWidth, valueLabel](int value)
    {
        valueLabel->setText(tr("%1 px").arg(value));

        if (lineWidth)
            UBDrawingController::drawingController()->setCustomLineWidth(value);
        else
            UBDrawingController::drawingController()->setCustomEraserWidth(value);
    });

    QWidget *anchor = mMainWindow->boardToolBar;
    if (anchorAction)
    {
        const QList<QWidget *> widgets = anchorAction->associatedWidgets();
        for (QWidget *widget : widgets)
        {
            if (qobject_cast<QToolButton *>(widget))
            {
                anchor = widget;
                break;
            }
        }
    }

    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
}


void UBBoardController::updateWidthButtonIcons()
{
    UBSettings *settings = UBSettings::settings();
    const UBStylusTool::Enum tool = (UBStylusTool::Enum)
            UBDrawingController::drawingController()->stylusTool();

    const qreal lineWidth = tool == UBStylusTool::Marker ? settings->markerStrongWidth()
                         : tool == UBStylusTool::Shape  ? settings->shapeStrongWidth()
                                                        : settings->penStrongWidth();
    mMainWindow->actionLineLarge->setIcon(createLineWidthIcon(lineWidth));
    mMainWindow->actionLineLarge->setToolTip(tr("Custom Line Width (%1 px)").arg(qRound(lineWidth)));

    const qreal eraserWidth = settings->eraserStrongWidth();
    mMainWindow->actionEraserLarge->setIcon(createEraserWidthIcon(eraserWidth));
    mMainWindow->actionEraserLarge->setToolTip(tr("Custom Eraser Size (%1 px)").arg(qRound(eraserWidth)));
}


void UBBoardController::eraserWidthActionActivated(int index)
{
    UBDrawingController *drawingController = UBDrawingController::drawingController();

    if (index == mStrokeEraserActionIndex)
        drawingController->setStylusTool(UBStylusTool::StrokeEraser);
    else if (index >= 0 && index <= int(UBWidth::Strong))
        drawingController->setEraserWidthIndex(index);
}


void UBBoardController::eraserToolChanged(int tool)
{
    if (!mEraserWidthChoice)
        return;

    if (tool == UBStylusTool::StrokeEraser)
        mEraserWidthChoice->setCurrentIndex(mStrokeEraserActionIndex);
    else if (tool == UBStylusTool::Eraser)
        mEraserWidthChoice->setCurrentIndex(UBSettings::settings()->eraserWidthIndex());
    else if (mEraserWidthChoice->currentIndex() == mStrokeEraserActionIndex)
        mEraserWidthChoice->clearSelection();
}


void UBBoardController::shapeKindActionActivated(int index)
{
    chooseShapeKind(index, true);
}


void UBBoardController::shapeGroupActionActivated(int index)
{
    if (index == 1)
    {
        activateLineTool();
        return;
    }

    if (index == 0)
    {
        chooseShapeKind(0, false);
        return;
    }

    if (index == 2 || index == 3)
    {
        const int recentIndex = index - 2;
        if (recentIndex < mShapeRecentKinds.size())
            chooseShapeKind(mShapeRecentKinds.at(recentIndex), false);
    }
}


void UBBoardController::chooseShapeKind(int kind, bool updateRecent)
{
    if (kind < 0 || kind >= UBShapeGeometry::shapeKindCount())
        return;

    UBDrawingController *drawingController = UBDrawingController::drawingController();

    // Changing the shape kind while an editable shape is being edited finalizes
    // the current shape first.
    if (drawingController->stylusTool() == UBStylusTool::Shape && mActiveScene)
        mActiveScene->commitShapeEditing();

    drawingController->setShapeKind(kind);

    if (updateRecent && kind != 0 && kind != UBShapeLineKind)
    {
        mShapeRecentKinds.removeAll(kind);
        mShapeRecentKinds.prepend(kind);
        while (mShapeRecentKinds.size() > 2)
            mShapeRecentKinds.removeLast();

        UBSettings::settings()->setShapeRecentKinds(mShapeRecentKinds);
        updateShapeRecentActions();
    }

    refreshShapeGroupSelection();
}


void UBBoardController::refreshShapeGroupSelection()
{
    if (!mShapeChoice)
        return;

    UBDrawingController *drawingController = UBDrawingController::drawingController();
    const int tool = drawingController->stylusTool();

    if (tool == UBStylusTool::Line)
    {
        mShapeChoice->setCurrentIndex(1);
        return;
    }

    if (tool == UBStylusTool::Shape)
    {
        const int kind = drawingController->shapeKind();
        int index = -1;

        if (kind == 0)
            index = 0;
        else if (kind == UBShapeLineKind)
            index = 1;
        else if (mShapeRecentKinds.size() > 0 && kind == mShapeRecentKinds.at(0))
            index = 2;
        else if (mShapeRecentKinds.size() > 1 && kind == mShapeRecentKinds.at(1))
            index = 3;

        if (index >= 0)
            mShapeChoice->setCurrentIndex(index);
        else
            mShapeChoice->clearSelection();
        return;
    }

    mShapeChoice->clearSelection();
}


void UBBoardController::updateShapeRecentActions()
{
    for (int i = 0; i < mShapeRecentActions.size(); ++i)
    {
        QAction *action = mShapeRecentActions.at(i);
        if (!action)
            continue;

        if (i < mShapeRecentKinds.size())
        {
            const int kind = mShapeRecentKinds.at(i);
            const QString name = UBShapeGeometry::nameForKind(kind);
            action->setIcon(createShapeKindIcon(kind));
            action->setText(name);
            action->setToolTip(tr("Draw: %1").arg(name));
            action->setVisible(true);
        }
        else
        {
            action->setVisible(false);
        }
    }
}


void UBBoardController::setShapeStrokeStyle(int index)
{
    UBSettings::settings()->setShapeStrokeStyle(index);
}


void UBBoardController::activateLineTool()
{
    // The old Line tool code is kept, but the fixed button now selects the
    // Shape tool's open line kind.
    chooseShapeKind(UBShapeLineKind, false);
}


void UBBoardController::shapeToolChanged(int tool)
{
    Q_UNUSED(tool);
    refreshShapeGroupSelection();
}


void UBBoardController::showShapePicker()
{
    QAction *anchorAction = qobject_cast<QAction *>(sender());

    // Follow the existing showWidthSliderPopup() pattern: a QMenu hosting a
    // QWidgetAction, executed under the anchor button. QMenu handles closing on
    // outside click/Escape and keeps itself on screen.
    QMenu menu(mMainWindow);

    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);

    QLabel *title = new QLabel(tr("Shapes"), container);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(2);

    const int currentKind = UBSettings::settings()->shapeKind();
    constexpr int columns = 4;

    QButtonGroup *buttonGroup = new QButtonGroup(container);
    buttonGroup->setExclusive(true);

    const QVector<UBShapeGeometry::ShapeDefinition>& definitions =
            UBShapeGeometry::shapes();

    int position = 0;

    for (int i = 0; i < definitions.size(); ++i)
    {
        const UBShapeGeometry::ShapeDefinition& definition = definitions.at(i);

        // Equilateral triangle (10) is merged into the free triangle (12).
        if (definition.kind == 10)
            continue;

        QToolButton *button = new QToolButton(container);
        button->setIcon(createShapeKindIcon(definition.kind));
        button->setIconSize(QSize(32, 32));
        button->setFixedSize(40, 40);
        button->setCheckable(true);
        button->setChecked(definition.kind == currentKind);
        button->setToolTip(tr("Draw: %1")
                           .arg(UBShapeGeometry::nameForKind(definition.kind)));
        button->setAutoRaise(true);

        buttonGroup->addButton(button);
        grid->addWidget(button, position / columns, position % columns);
        ++position;

        const int kind = definition.kind;
        connect(button, &QToolButton::clicked, this, [this, kind, &menu]()
        {
            shapeKindActionActivated(kind);
            menu.close();
        });
    }

    layout->addLayout(grid);
    layout->setContentsMargins(8, 6, 8, 8);
    container->setLayout(layout);

    QWidgetAction *widgetAction = new QWidgetAction(&menu);
    widgetAction->setDefaultWidget(container);
    menu.addAction(widgetAction);

    QWidget *anchor = mMainWindow->boardToolBar;
    if (anchorAction)
    {
        const QList<QWidget *> widgets = anchorAction->associatedWidgets();
        for (QWidget *widget : widgets)
        {
            if (qobject_cast<QToolButton *>(widget))
            {
                anchor = widget;
                break;
            }
        }
    }

    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
}


qreal UBBoardController::currentZoom() const
{
    if (mControlView)
        return mControlView->transform().m11() / mSystemScaleFactor;
    else
        return 1.0;
}

void UBBoardController::removeTool(UBToolWidget* toolWidget)
{
    toolWidget->remove();
}

void UBBoardController::hide()
{
    UBApplication::mainWindow->actionLibrary->setChecked(false);
}

void UBBoardController::show()
{
    UBApplication::mainWindow->actionLibrary->setChecked(false);

    if (mSwitchToSceneIndex >= 0)
    {
        setActiveDocumentScene(mSwitchToSceneIndex);
        mSwitchToSceneIndex = -1;
    }
    else
    {
        setActiveDocumentScene(mActiveSceneIndex);
    }
}

void UBBoardController::persistCurrentScene(bool isAnAutomaticBackup, bool forceImmediateSave)
{
    // Finalize an in-progress free triangle before a real save/page/close
    // boundary. Automatic backups leave the editing state untouched; the
    // preview polygons are ordinary stroke polygons and serialize safely.
    if (!isAnAutomaticBackup && mActiveScene)
        mActiveScene->commitShapeEditing();

    if(UBPersistenceManager::persistenceManager()
            && selectedDocument() && mActiveScene && mActiveSceneIndex != mDeletingSceneIndex
            && (mActiveSceneIndex >= 0) && mActiveSceneIndex != mMovingSceneIndex)
    {
        mActiveScene->saveWidgetSnapshots();

        if (mActiveScene->isModified())
        {
            auto document = UBDocument::getDocument(selectedDocument());
            document->persistPage(mActiveScene, mActiveSceneIndex, isAnAutomaticBackup, forceImmediateSave);
        }
    }
}

void UBBoardController::updateSystemScaleFactor()
{
    if (mActiveScene)
    {
        qreal newScaleFactor = 1.0;
        QSize pageNominalSize = mActiveScene->nominalSize();
        // disabled: we're going to keep scale factor untouched if the size is custom
        QMap<DocumentSizeRatio::Enum, QSize> sizesMap = UBSettings::settings()->documentSizes;
      //  if(pageNominalSize == sizesMap.value(DocumentSizeRatio::Ratio16_9) || pageNominalSize == sizesMap.value(DocumentSizeRatio::Ratio4_3))
        {
            qreal hFactor = ((qreal)controlView()->size().width()) / ((qreal)pageNominalSize.width());
            qreal vFactor = ((qreal)controlView()->size().height()) / ((qreal)pageNominalSize.height());

            newScaleFactor = qMin(hFactor, vFactor);
        }

        if (mSystemScaleFactor != newScaleFactor)
            mSystemScaleFactor = newScaleFactor;

        restoreViewPositionOnCurrentScene();
        mActiveScene->setBackgroundZoomFactor(mControlView->transform().m11());
    }
    else
    {
        mSystemScaleFactor = 1.0;
    }
}


void UBBoardController::setWidePageSize(bool checked)
{
    Q_UNUSED(checked);
    QSize newSize = UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio16_9);

    if (mActiveScene->nominalSize() != newSize)
    {
        UBPageSizeUndoCommand* uc = new UBPageSizeUndoCommand(mActiveScene, mActiveScene->nominalSize(), newSize);
        UBApplication::undoStack->push(uc);

        setPageSize(newSize);
    }
}


void UBBoardController::setRegularPageSize(bool checked)
{
    Q_UNUSED(checked);
    QSize newSize = UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio4_3);

    if (mActiveScene->nominalSize() != newSize)
    {
        UBPageSizeUndoCommand* uc = new UBPageSizeUndoCommand(mActiveScene, mActiveScene->nominalSize(), newSize);
        UBApplication::undoStack->push(uc);

        setPageSize(newSize);
    }
}


void UBBoardController::setPageSize(QSize newSize)
{
    if (mActiveScene->nominalSize() != newSize)
    {
        mActiveScene->setNominalSize(newSize);

        persistViewPositionOnCurrentScene();

        updateSystemScaleFactor();
        updatePageSizeState();
        adjustDisplayViews();
        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

        UBSettings::settings()->pageSize->set(newSize);
    }
}

void UBBoardController::notifyCache(bool visible)
{
    if(visible)
        emit cacheEnabled();

    mCacheWidgetIsEnabled = visible;
}

void UBBoardController::updatePageSizeState()
{
    if (mActiveScene->nominalSize() == UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio16_9))
    {
        mMainWindow->actionWidePageSize->setChecked(true);
    }
    else if(mActiveScene->nominalSize() == UBSettings::settings()->documentSizes.value(DocumentSizeRatio::Ratio4_3))
    {
        mMainWindow->actionRegularPageSize->setChecked(true);
    }
    else
    {
        mMainWindow->actionCustomPageSize->setChecked(true);
    }
}


void UBBoardController::stylusToolChanged(int tool)
{
    if (UBPlatformUtils::hasVirtualKeyboard() && mPaletteManager->mKeyboardPalette)
    {
        UBStylusTool::Enum eTool = (UBStylusTool::Enum)tool;
        if(eTool != UBStylusTool::Selector && eTool != UBStylusTool::Text)
        {
            if(mPaletteManager->mKeyboardPalette->m_isVisible)
            {
#ifdef Q_OS_OSX
                if (!UBPlatformUtils::errorOpeningVirtualKeyboard)
                    UBApplication::mainWindow->actionVirtualKeyboard->activate(QAction::Trigger);
#else
                UBApplication::mainWindow->actionVirtualKeyboard->activate(QAction::Trigger);
#endif
            }
        }
    }

}


QUrl UBBoardController::expandWidgetToTempDir(const QByteArray& pZipedData, const QString& ext)
{
    QUrl widgetUrl;
    QTemporaryFile tmp;

    if (tmp.open())
    {
        tmp.write(pZipedData);
        tmp.flush();
        tmp.close();

        QString tmpDir = UBFileSystemUtils::createTempDir() + "." + ext;

        if (UBFileSystemUtils::expandZipToDir(tmp, tmpDir))
        {
            widgetUrl = QUrl::fromLocalFile(tmpDir);
        }
    }

    return widgetUrl;
}


void UBBoardController::grabScene(const QRectF& pSceneRect)
{
    if (mActiveScene)
    {
        /*
         * To get the pixel size on screen for the screenshot
         * we use the system scale factor to align to the pixels
         * and use an additional factor of two to provide more details
         */
        const auto scalingFactor = 2. * mSystemScaleFactor;
        const auto pixelSize = pSceneRect.size() * scalingFactor;
        QImage image(pixelSize.toSize(), QImage::Format_ARGB32);
        image.fill(Qt::transparent);

        QRectF targetRect{{0, 0}, pixelSize};
        QPainter painter(&image);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.setRenderHint(QPainter::LosslessImageRendering);

        mActiveScene->setRenderingContext(UBGraphicsScene::NonScreen);
        mActiveScene->setRenderingQuality(UBItem::RenderingQualityHigh, UBItem::CacheNotAllowed);

        mActiveScene->render(&painter, targetRect, pSceneRect);

        mActiveScene->setRenderingContext(UBGraphicsScene::Screen);
//        mActiveScene->setRenderingQuality(UBItem::RenderingQualityNormal);
        mActiveScene->setRenderingQuality(UBItem::RenderingQualityHigh, UBItem::CacheAllowed);

        mPaletteManager->addItem(QPixmap::fromImage(image), QPointF{}, 1. / scalingFactor);
        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
    }
}

UBGraphicsMediaItem* UBBoardController::addVideo(const QUrl& pSourceUrl, bool startPlay, const QPointF& pos, bool bUseSource)
{
    QUuid uuid = QUuid::createUuid();
    QUrl concreteUrl = pSourceUrl;

    // media file is not in document folder yet
    if (!bUseSource)
    {
        QString destFile;
        bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
                    pSourceUrl.toLocalFile(),
                    UBPersistenceManager::videoDirectory,
                    uuid,
                    destFile);
        if (!b)
        {
            UBApplication::showMessage(tr("Add file operation failed: file copying error"));
            return NULL;
        }
        concreteUrl = QUrl::fromLocalFile(destFile);
    }// else we just use source Url.


    UBGraphicsMediaItem* vi = mActiveScene->addMedia(concreteUrl, startPlay, pos);
    QDateTime now  = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    if (vi) {
        vi->setUuid(uuid);
        vi->setSourceUrl(pSourceUrl);
    }

    return vi;

}

UBGraphicsMediaItem* UBBoardController::addAudio(const QUrl& pSourceUrl, bool startPlay, const QPointF& pos, bool bUseSource)
{
    QUuid uuid = QUuid::createUuid();
    QUrl concreteUrl = pSourceUrl;

    // media file is not in document folder yet
    if (!bUseSource)
    {
        QString destFile;
        bool b = UBPersistenceManager::persistenceManager()->addFileToDocument(selectedDocument(),
            pSourceUrl.toLocalFile(),
            UBPersistenceManager::audioDirectory,
            uuid,
            destFile);
        if (!b)
        {
            UBApplication::showMessage(tr("Add file operation failed: file copying error"));
            return NULL;
        }
        concreteUrl = QUrl::fromLocalFile(destFile);
    }// else we just use source Url.

    UBGraphicsMediaItem* ai = mActiveScene->addMedia(concreteUrl, startPlay, pos);
    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));

    if (ai){
        ai->setUuid(uuid);
        ai->setSourceUrl(pSourceUrl);
    }

    return ai;

}

UBGraphicsWidgetItem *UBBoardController::addW3cWidget(const QUrl &pUrl, const QPointF &pos)
{
    UBGraphicsWidgetItem* w3cWidgetItem = 0;

    QUuid uuid = QUuid::createUuid();

    QString destPath;
    if (!UBPersistenceManager::persistenceManager()->addGraphicsWidgetToDocument(selectedDocument(), pUrl.toLocalFile(), uuid, destPath))
        return NULL;
    QUrl newUrl = QUrl::fromLocalFile(destPath);

    w3cWidgetItem = mActiveScene->addW3CWidget(newUrl, pos);

    if (w3cWidgetItem) {
        w3cWidgetItem->setUuid(uuid);
        w3cWidgetItem->setOwnFolder(newUrl);
        w3cWidgetItem->setSourceUrl(pUrl);

        QString struuid = UBStringUtils::toCanonicalUuid(uuid);
        QString snapshotPath = selectedDocument()->persistencePath() +  "/" + UBPersistenceManager::widgetDirectory + "/" + struuid + ".png";
        w3cWidgetItem->setSnapshotPath(QUrl::fromLocalFile(snapshotPath));
    }

    return w3cWidgetItem;
}

void UBBoardController::cut()
{
    //---------------------------------------------------------//

    QList<QGraphicsItem*> selectedItems;
    foreach(QGraphicsItem* gi, mActiveScene->selectedItems())
        selectedItems << gi;

    //---------------------------------------------------------//

    QList<UBItem*> selected;
    foreach(QGraphicsItem* gi, selectedItems)
    {
        gi->setSelected(false);

        UBItem* ubItem = dynamic_cast<UBItem*>(gi);
        UBGraphicsItem *ubGi =  dynamic_cast<UBGraphicsItem*>(gi);

        if (ubItem && ubGi && !mActiveScene->tools().contains(gi))
        {
            UBItem* copy = ubItem->deepCopy();
            if (copy)
            {
                selected << copy;
                ubGi->remove();
            }
        }
    }

    //---------------------------------------------------------//

    if (selected.size() > 0)
    {
        QClipboard *clipboard = QApplication::clipboard();

        UBMimeDataGraphicsItem*  mimeGi = new UBMimeDataGraphicsItem(selected);

        mimeGi->setData(UBApplication::mimeTypeUniboardPageItem, QByteArray());
        clipboard->setMimeData(mimeGi);

        QDateTime now = QDateTime::currentDateTime();
        selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
    }

    //---------------------------------------------------------//
}


void UBBoardController::copy()
{
    QList<UBItem*> selected;

    foreach(QGraphicsItem* gi, mActiveScene->selectedItems())
    {
        UBItem* ubItem = dynamic_cast<UBItem*>(gi);

        if (ubItem && !mActiveScene->tools().contains(gi))
        {
            UBItem* copy = ubItem->deepCopy();
            if (copy)
                selected << copy;
        }
    }

    // The clipboard must own its items: it can outlive the page (and the
    // scene that owns the originals). Storing raw pointers to scene items
    // used to leave dangling entries that ClearUndoStack() dereferenced
    // when reading sourceUrl(), causing a SIGSEGV.

    if (selected.size() > 0)
    {
        QClipboard *clipboard = QApplication::clipboard();

        UBMimeDataGraphicsItem*  mimeGi = new UBMimeDataGraphicsItem(selected);

        mimeGi->setData(UBApplication::mimeTypeUniboardPageItem, QByteArray());
        clipboard->setMimeData(mimeGi);

    }
}


void UBBoardController::paste()
{
    QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();

    QRect rect = mControlView->rect();
    QPoint center(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2);
    const QPointF viewRelativeCenter = mControlView->mapToScene(center);

    qreal imageScale = 1.0;
    QPointF pastePos;

    if (mimeData->hasImage())
    {
        // Fit a pasted image to the visible page so it looks the same size on
        // blank and imported-PDF pages: keep the aspect ratio, never upscale
        // beyond the natural size, and never exceed 40% of the visible width
        // or height. The image is centred in the visible area.
        QImage image;
        const QVariant imageData = mimeData->imageData();
        if (imageData.canConvert<QImage>())
            image = qvariant_cast<QImage>(imageData);
        if (image.isNull())
        {
            const QStringList formats = mimeData->formats();
            for (const QString &format : formats)
            {
                if (UBFileSystemUtils::mimeTypeFromString(format) == UBMimeType::RasterImage)
                {
                    image = QImage::fromData(mimeData->data(format));
                    break;
                }
            }
        }

        const QRectF visible =
                mControlView->mapToScene(mControlView->rect()).boundingRect();
        if (!image.isNull() && image.width() > 0 && image.height() > 0
                && visible.width() > 0.0 && visible.height() > 0.0)
        {
            const qreal maxWidth = visible.width() * 0.4;
            const qreal maxHeight = visible.height() * 0.4;
            imageScale = qMin<qreal>(1.0, qMin(maxWidth / image.width(),
                                              maxHeight / image.height()));
            if (imageScale <= 0.0)
                imageScale = 1.0;
        }

        pastePos = viewRelativeCenter;
    }
    else
    {
        qreal xPosition = ((qreal)QRandomGenerator::global()->bounded(RAND_MAX)/(qreal)RAND_MAX) * 400;
        qreal yPosition = ((qreal)QRandomGenerator::global()->bounded(RAND_MAX)/(qreal)RAND_MAX) * 200;
        QPointF randomPos(xPosition -200 , yPosition - 100);
        pastePos = viewRelativeCenter + randomPos;
    }

    processMimeData(mimeData, pastePos, imageScale);

    QDateTime now = QDateTime::currentDateTime();
    selectedDocument()->setMetaData(UBSettings::documentUpdatedAt, UBStringUtils::toUtcIsoDateTime(now));
}


bool zLevelLessThan( UBItem* s1, UBItem* s2)
{
    qreal s1Zvalue = dynamic_cast<QGraphicsItem*>(s1)->data(UBGraphicsItemData::ItemOwnZValue).toReal();
    qreal s2Zvalue = dynamic_cast<QGraphicsItem*>(s2)->data(UBGraphicsItemData::ItemOwnZValue).toReal();
    return s1Zvalue < s2Zvalue;
}

void UBBoardController::processMimeData(const QMimeData* pMimeData, const QPointF& pPos, qreal pImageScale)
{
    if (pMimeData->hasFormat(UBApplication::mimeTypeUniboardPageItem))
    {
        const UBMimeDataGraphicsItem* mimeData = qobject_cast <const UBMimeDataGraphicsItem*>(pMimeData);

        if (mimeData)
        {
            QList<UBItem*> items = mimeData->items();
            std::stable_sort(items.begin(),items.end(),zLevelLessThan);
            foreach(UBItem* item, items)
            {
                QGraphicsItem* pItem = dynamic_cast<QGraphicsItem*>(item);
                if(NULL != pItem){
                    duplicateItem(item);
                }
            }

            return;
        }
    }

    if(pMimeData->hasHtml())
    {
        QString qsHtml = pMimeData->html();
        QString url = UBApplication::urlFromHtml(qsHtml);

        if("" != url)
        {
            downloadURL(url, QString(), pPos);
            return;
        }
    }

    if (pMimeData->hasUrls())
    {
        QList<QUrl> urls = pMimeData->urls();

        int index = 0;

        const UBFeaturesMimeData *internalMimeData = qobject_cast<const UBFeaturesMimeData*>(pMimeData);
        bool internalData = false;
        if (internalMimeData) {
            internalData = true;
        }

        foreach(const QUrl url, urls){
            QPointF pos(pPos + QPointF(index * 15, index * 15));

            downloadURL(url, QString(), pos, QSize(), false,  internalData);
            index++;
        }

        return;
    }

    if (pMimeData->hasImage())
    {
        const QStringList formats = pMimeData->formats();
        QString selectedFormat;
        UBMimeType::Enum ubMimeType{UBMimeType::UNKNOWN};

        for (const QString& format : formats)
        {            
            ubMimeType = UBFileSystemUtils::mimeTypeFromString(format);

            if (ubMimeType == UBMimeType::VectorImage || ubMimeType == UBMimeType::RasterImage)
            {
                selectedFormat = format;
                break;
            }
        }

        QBuffer buffer;

        if (selectedFormat.isEmpty())
        {
            // should never happen, but just in case
            // create an image and fill the buffer with PNG data
            QImage img = qvariant_cast<QImage> (pMimeData->imageData());
            img.save(&buffer, "png");
            ubMimeType = UBMimeType::RasterImage;
        }
        else
        {
            // get data from mime data
            buffer.setData(pMimeData->data(selectedFormat));
        }

        if (ubMimeType == UBMimeType::VectorImage)
        {
            mActiveScene->addSvg({}, pPos, buffer.data());
            return;
        }
        else if (ubMimeType == UBMimeType::RasterImage)
        {
            // validate that the image is really an image, webkit does not fill properly the image mime data
            if (!buffer.data().isEmpty())
            {
                mActiveScene->addImage(buffer.data(), nullptr, pPos, pImageScale);
                return;
            }
        }
    }

    if (pMimeData->hasText())
    {
        if("" != pMimeData->text()){
            // Sometimes, it is possible to have an URL as text. we check here if it is the case
            QString qsTmp = pMimeData->text().remove(QChar('\0'));
            if(qsTmp.startsWith("http"))
                downloadURL(QUrl(qsTmp), QString(), pPos);
            else{
                if(mActiveScene->selectedItems().count() && mActiveScene->selectedItems().at(0)->type() == UBGraphicsItemType::TextItemType)
                    dynamic_cast<UBGraphicsTextItem*>(mActiveScene->selectedItems().at(0))->setHtml(pMimeData->text());
                else
                    mActiveScene->addTextHtml("", pPos)->setHtml(pMimeData->text());
            }
        }
        else{
#ifdef Q_OS_OSX
                //  With Safari, in 95% of the drops, the mime datas are hidden in Apple Web Archive pasteboard type.
                //  This is due to the way Safari is working so we have to dig into the pasteboard in order to retrieve
                //  the data.
                QString qsUrl = UBPlatformUtils::urlFromClipboard();
                if("" != qsUrl){
                    // We finally got the url of the dropped ressource! Let's import it!
                    downloadURL(qsUrl, qsUrl, pPos);
                    return;
                }
#endif
        }
    }
}


void UBBoardController::togglePodcast(bool checked)
{
    if (UBPodcastController::instance())
        UBPodcastController::instance()->toggleRecordingPalette(checked);
}

void UBBoardController::moveGraphicsWidgetToControlView(UBGraphicsWidgetItem* graphicsWidget)
{
    mActiveScene->setURStackEnable(false);
     UBGraphicsItem *toolW3C = duplicateItem(dynamic_cast<UBItem *>(graphicsWidget));
    UBGraphicsWidgetItem *copyedGraphicsWidget = NULL;

    if (toolW3C)
    {
        if (UBGraphicsWidgetItem::Type == toolW3C->type())
            copyedGraphicsWidget = static_cast<UBGraphicsWidgetItem *>(toolW3C);

        UBToolWidget *toolWidget = new UBToolWidget(copyedGraphicsWidget, mControlView);

        graphicsWidget->remove(false);
        mActiveScene->addItemToDeletion(graphicsWidget);

        mActiveScene->setURStackEnable(true);

        QPoint controlViewPos = mControlView->mapFromScene(graphicsWidget->sceneBoundingRect().center());
        toolWidget->centerOn(mControlView->mapTo(mControlContainer, controlViewPos));
        toolWidget->show();
    }
}


void UBBoardController::moveToolWidgetToScene(UBToolWidget* toolWidget)
{
    UBGraphicsWidgetItem *widgetToScene = toolWidget->toolWidget();

    widgetToScene->resetTransform();

    QPoint mainWindowCenter = toolWidget->mapTo(mMainWindow, QPoint(toolWidget->width(), toolWidget->height()) / 2);
    QPoint controlViewCenter = mControlView->mapFrom(mMainWindow, mainWindowCenter);
    QPointF scenePos = mControlView->mapToScene(controlViewCenter);

    widgetToScene->setWebActive(true);
    mActiveScene->addGraphicsWidget(widgetToScene, scenePos);

    toolWidget->remove();
}


void UBBoardController::updateBackgroundActionsState(bool isDark, UBPageBackground pageBackground)
{
    switch (pageBackground) {

        case UBPageBackground::crossed:
            if (isDark)
                mMainWindow->actionCrossedDarkBackground->setChecked(true);
            else
                mMainWindow->actionCrossedLightBackground->setChecked(true);
        break;

        case UBPageBackground::ruled:
        {
            QAction* actionRuledBackground = nullptr;
            if(UBSettings::settings()->isSeyesRuledBackground())
                if(isDark)
                    actionRuledBackground = mMainWindow->actionSeyesRuledDarkBackground;
                else
                    actionRuledBackground = mMainWindow->actionSeyesRuledLightBackground;
            else
                if(isDark)
                    actionRuledBackground = mMainWindow->actionRuledDarkBackground;
                else
                    actionRuledBackground = mMainWindow->actionRuledLightBackground;
            if(actionRuledBackground)
                actionRuledBackground->setChecked(true);
        }
        break;

        default:
            if (isDark)
                mMainWindow->actionPlainDarkBackground->setChecked(true);
            else
                mMainWindow->actionPlainLightBackground->setChecked(true);
        break;
    }
}


void UBBoardController::addItem()
{
    QString defaultPath = UBSettings::settings()->lastImportToLibraryPath->get().toString();

    QString extensions;
    foreach(QString ext, UBSettings::imageFileExtensions)
    {
        extensions += " *.";
        extensions += ext;
    }

    QString filename = QFileDialog::getOpenFileName(mControlContainer, tr("Add Item"),
                                                    defaultPath,
                                                    tr("All Supported (%1)").arg(extensions), NULL, QFileDialog::DontUseNativeDialog);

    if (filename.length() > 0)
    {
        mPaletteManager->addItem(QUrl::fromLocalFile(filename));
        QFileInfo source(filename);
        UBSettings::settings()->lastImportToLibraryPath->set(QVariant(source.absolutePath()));
    }
}

void UBBoardController::importPage()
{
    int pageCount = selectedDocument()->pageCount();
    if (UBApplication::documentController->addFileToDocument(selectedDocument()))
    {
        setActiveDocumentScene(selectedDocument(), pageCount, true);
    }
}

void UBBoardController::notifyPageChanged()
{
    emit activeSceneChanged();
}

void UBBoardController::onDownloadModalFinished()
{

}

void UBBoardController::displayMetaData(QMap<QString, QString> metadatas)
{
    emit displayMetadata(metadatas);
}

void UBBoardController::freezeW3CWidgets(bool freeze)
{
    if (mActiveSceneIndex >= 0)
    {
        QList<QGraphicsItem *> list = UBApplication::boardController->activeScene()->items();
        foreach(QGraphicsItem *item, list)
        {
            freezeW3CWidget(item, freeze);
        }
    }
}

void UBBoardController::freezeW3CWidget(QGraphicsItem *item, bool freeze)
{
    if (item->type() == UBGraphicsW3CWidgetItem::Type)
    {
        UBGraphicsWidgetItem* widget = qgraphicsitem_cast<UBGraphicsWidgetItem*>(item);
        widget->setWebActive(!freeze);
    }
}
