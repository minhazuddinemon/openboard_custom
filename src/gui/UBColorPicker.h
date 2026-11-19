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




#ifndef UBCOLORPICKER_H_
#define UBCOLORPICKER_H_

#include <QtGui>
#include <QFrame>

class UBColorPicker : public QFrame
{

    Q_OBJECT;

    public:
        UBColorPicker(QWidget* parent);
        UBColorPicker(QWidget* parent, const QList<QColor>& colors, int pSelectedColorIndex = 0);
        virtual ~UBColorPicker();
        QList<QColor> getColors() const
        {
            return mColors;
        }

        void setColors(const QList<QColor>& pColors)
        {
            mColors = pColors;

            // Keep the selection valid when the list changes (it can be
            // empty, shorter or longer than before).
            if (mColors.isEmpty())
                mSelectedColorIndex = 0;
            else if (mSelectedColorIndex < 0 || mSelectedColorIndex >= mColors.size())
                mSelectedColorIndex = 0;

            repaint();
        }

        int selectedColorIndex() const
        {
            return mSelectedColorIndex;
        }

        void setSelectedColorIndex(int pSelectedColorIndex)
        {
            // QList::indexOf() returns -1 when the color is not in the list;
            // never store an out-of-range index.
            if (mColors.isEmpty())
                mSelectedColorIndex = 0;
            else
                mSelectedColorIndex = qBound(0, pSelectedColorIndex, mColors.size() - 1);

            repaint();
        }

    signals:
        void colorSelected(const QColor& color);

    protected:

        virtual void paintEvent ( QPaintEvent * event );
        virtual void mousePressEvent ( QMouseEvent * event );


    private:
        QList<QColor> mColors;
        int mSelectedColorIndex;
};

#endif /* UBCOLORPICKER_H_ */
