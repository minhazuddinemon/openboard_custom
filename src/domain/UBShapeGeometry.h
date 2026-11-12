/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * This file is part of OpenBoard.
 *
 * OpenBoard is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * OpenBoard is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenBoard. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UBSHAPEGEOMETRY_H_
#define UBSHAPEGEOMETRY_H_

#include <QtCore>
#include <QRectF>
#include <QVector>

namespace UBShapeGeometry
{

// Every function takes the raw signed drag rectangle (press point as topLeft(),
// drag point as bottomRight(), width()/height() may be negative) and returns a
// closed point list (last point equals the first).
struct ShapeDefinition
{
    int kind;
    const char* name;
    bool constrainedBox;
    QVector<QPointF> (*points)(const QRectF& dragRect);
};

QVector<QPointF> rectangle(const QRectF& dragRect);
QVector<QPointF> ellipse(const QRectF& dragRect);
QVector<QPointF> circle(const QRectF& dragRect);
QVector<QPointF> square(const QRectF& dragRect);
QVector<QPointF> triangle(const QRectF& dragRect);
QVector<QPointF> diamond(const QRectF& dragRect);
QVector<QPointF> parallelogram(const QRectF& dragRect);
QVector<QPointF> pentagon(const QRectF& dragRect);
QVector<QPointF> hexagon(const QRectF& dragRect);
QVector<QPointF> star(const QRectF& dragRect);
QVector<QPointF> equilateralTriangle(const QRectF& dragRect);
QVector<QPointF> rightTriangle(const QRectF& dragRect);
QVector<QPointF> freeTriangle(const QRectF& dragRect);
QVector<QPointF> freeTriangle(const QPointF& baseStart, const QPointF& baseEnd,
                              const QPointF& apex);
QPointF freeTriangleApex(const QPointF& baseStart, const QPointF& baseEnd);

const QVector<ShapeDefinition>& shapes();
QVector<QPointF> pointsForKind(int kind, const QRectF& dragRect);
QString nameForKind(int kind);
int shapeKindCount();
bool isConstrainedKind(int kind);

} // namespace UBShapeGeometry

#endif /* UBSHAPEGEOMETRY_H_ */
