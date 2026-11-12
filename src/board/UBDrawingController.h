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




#ifndef UBDRAWINGCONTROLLER_H_
#define UBDRAWINGCONTROLLER_H_

#include <QtCore>

#include "core/UB.h"

class UBAbstractDrawRuler;

class UBDrawingController : public QObject
{
    Q_OBJECT;

    private:
        UBDrawingController(QObject * parent = 0);
        virtual ~UBDrawingController();

    public:

        static UBDrawingController* drawingController();
        static void destroy();

        int stylusTool();
        int latestDrawingTool();

        bool isDrawingTool(int tool = -1);
        bool isSnappingTool() const;

        int currentToolWidthIndex();
        qreal currentToolWidth();
        int currentToolColorIndex();
        QColor currentToolColor();
        QColor toolColor(bool onDarkBackground);

        // Last used shape kind (0 = rectangle, 1 = ellipse).
        int shapeKind();
        void setShapeKind(int kind);

        // Indices of the two recent custom-color slots (toolbar slots 6 and 7).
        static constexpr int CustomColorStartIndex = 5;
        static constexpr int CustomColorCount = 2;

        // Returns the color that keeps similar contrast on the twin board theme.
        static QColor invertedColor(const QColor& color);

        // Dedicated colour for the virtual laser pointer. It is independent of
        // the pen colour and defaults to red.
        QColor laserColor() const { return mLaserColor; }
        void setLaserColor(const QColor& color);

        // Stores color in the recent-custom-color slots (index 5 becomes the
        // most recent color, the previous index 5 moves to index 6). Both the
        // dark- and light-background variants are stored.
        void setCustomColor(const QColor& color);

        void setPenColor(bool onDarkBackground, const QColor& color, int pIndex);
        void setMarkerColor(bool onDarkBackground, const QColor& color, int pIndex);
        void setMarkerAlpha(qreal alpha);

        // Sets the user-defined strong/custom width (toolbar slot 3) and
        // selects that slot immediately. Widths are persisted by UBSettings.
        void setCustomLineWidth(qreal width);
        void setCustomEraserWidth(qreal width);

        void setActiveRuler(UBAbstractDrawRuler* ruler);
        UBAbstractDrawRuler* activeRuler() const;

        void setInDesktopMode(bool mode){
            mIsDesktopMode = mode;
        }

        bool isInDesktopMode(){
            return mIsDesktopMode;
        }

    public slots:

        void setStylusTool(int tool);
        void setLassoMode();
        void setLaserMode();
        void setShapeMode();
        void setLineWidthIndex(int index);
        void setColorIndex(int index);
        void setEraserWidthIndex(int index);

    signals:
        void stylusToolChanged(int tool, int previousTool = -1);
        void colorPaletteChanged();

        void lineWidthIndexChanged(int index);
        void eraserWidthIndexChanged(int index);
        void colorIndexChanged(int index);

    private:
        QPointer<UBAbstractDrawRuler> mActiveRuler;
        UBStylusTool::Enum mStylusTool;
        UBStylusTool::Enum mLatestDrawingTool;
        bool mIsDesktopMode;
        QColor mLaserColor;

        static UBDrawingController* sDrawingController;

    private slots:

        void penToolSelected(bool checked);
        void eraserToolSelected(bool checked);
        void markerToolSelected(bool checked);
        void selectorToolSelected(bool checked);
        void playToolSelected(bool checked);
        void handToolSelected(bool checked);
        void zoomInToolSelected(bool checked);
        void zoomOutToolSelected(bool checked);
        void pointerToolSelected(bool checked);
        void lineToolSelected(bool checked);
        void textToolSelected(bool checked);
        void captureToolSelected(bool checked);
};

#endif /* UBDRAWINGCONTROLLER_H_ */
