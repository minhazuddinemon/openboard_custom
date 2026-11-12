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

#include "UBShapeGeometry.h"

#include <cmath>

#include "core/UB.h"

namespace
{

constexpr qreal kPi = 3.14159265358979323846;
constexpr qreal kTwoPi = 2.0 * kPi;

QRectF boxOf(const QRectF& dragRect)
{
    return dragRect.normalized();
}

// Constrained circle/square: side = max(|dx|, |dy|), growing from the press
// point in the drag direction. A zero delta extends right/down.
QRectF constrainedBox(const QRectF& dragRect)
{
    const qreal dx = dragRect.width();
    const qreal dy = dragRect.height();
    const qreal side = qMax(qAbs(dx), qAbs(dy));
    const qreal sx = (dx < 0.0) ? -1.0 : 1.0;
    const qreal sy = (dy < 0.0) ? -1.0 : 1.0;

    const QPointF start = dragRect.topLeft();
    return QRectF(start, start + QPointF(sx * side, sy * side)).normalized();
}

QVector<QPointF> regularPolygon(const QRectF& box, int sides, qreal startAngle)
{
    const QPointF center = box.center();
    const qreal radiusX = box.width() / 2.0;
    const qreal radiusY = box.height() / 2.0;

    QVector<QPointF> points;
    points.reserve(sides + 1);

    for (int i = 0; i <= sides; ++i)
    {
        const qreal angle = startAngle + kTwoPi * i / sides;
        points << QPointF(center.x() + radiusX * std::cos(angle),
                          center.y() + radiusY * std::sin(angle));
    }

    return points;
}

} // namespace

namespace UBShapeGeometry
{

QVector<QPointF> rectangle(const QRectF& dragRect)
{
    const QRectF box = boxOf(dragRect);

    QVector<QPointF> points;
    points << box.topLeft() << box.topRight() << box.bottomRight()
           << box.bottomLeft() << box.topLeft();
    return points;
}

QVector<QPointF> ellipse(const QRectF& dragRect)
{
    constexpr int segmentCount = 96;
    const QRectF box = boxOf(dragRect);

    QVector<QPointF> points;
    points.reserve(segmentCount + 1);

    const QPointF center = box.center();
    const qreal radiusX = box.width() / 2.0;
    const qreal radiusY = box.height() / 2.0;

    for (int i = 0; i <= segmentCount; ++i)
    {
        const qreal angle = kTwoPi * i / segmentCount;
        points << QPointF(center.x() + radiusX * std::cos(angle),
                          center.y() + radiusY * std::sin(angle));
    }

    return points;
}

QVector<QPointF> circle(const QRectF& dragRect)
{
    return ellipse(constrainedBox(dragRect));
}

QVector<QPointF> square(const QRectF& dragRect)
{
    return rectangle(constrainedBox(dragRect));
}

QVector<QPointF> triangle(const QRectF& dragRect)
{
    const QRectF box = boxOf(dragRect);

    QVector<QPointF> points;
    points << QPointF(box.center().x(), box.top())
           << box.bottomRight()
           << box.bottomLeft()
           << QPointF(box.center().x(), box.top());
    return points;
}

QVector<QPointF> diamond(const QRectF& dragRect)
{
    const QRectF box = boxOf(dragRect);
    const QPointF center = box.center();

    QVector<QPointF> points;
    points << QPointF(center.x(), box.top())
           << QPointF(box.right(), center.y())
           << QPointF(center.x(), box.bottom())
           << QPointF(box.left(), center.y())
           << QPointF(center.x(), box.top());
    return points;
}

QVector<QPointF> parallelogram(const QRectF& dragRect)
{
    return editableShapePoints(6, dragRect, initialShapeHandle(6, dragRect));
}

QVector<QPointF> pentagon(const QRectF& dragRect)
{
    return regularPolygon(boxOf(dragRect), 5, -kPi / 2.0);
}

QVector<QPointF> hexagon(const QRectF& dragRect)
{
    return regularPolygon(boxOf(dragRect), 6, 0.0);
}

QVector<QPointF> star(const QRectF& dragRect)
{
    const QRectF box = boxOf(dragRect);
    const QPointF center = box.center();
    const qreal radiusX = box.width() / 2.0;
    const qreal radiusY = box.height() / 2.0;
    const qreal innerRatio = 0.4;

    QVector<QPointF> points;
    points.reserve(11);

    for (int i = 0; i <= 10; ++i)
    {
        const qreal angle = -kPi / 2.0 + kPi * i / 5.0;
        const qreal radius = (i % 2 == 0) ? 1.0 : innerRatio;
        points << QPointF(center.x() + radiusX * radius * std::cos(angle),
                          center.y() + radiusY * radius * std::sin(angle));
    }

    return points;
}

QVector<QPointF> equilateralTriangle(const QRectF& dragRect)
{
    const qreal dx = dragRect.width();
    const qreal dy = dragRect.height();
    const QPointF dragPoint = dragRect.bottomRight();

    const qreal base = qAbs(dx);
    const qreal height = base * 0.8660254037844386;
    const qreal direction = (dy < 0.0) ? -1.0 : 1.0;

    const QPointF baseLeft(dragPoint.x() - base / 2.0, dragPoint.y());
    const QPointF baseRight(dragPoint.x() + base / 2.0, dragPoint.y());
    const QPointF apex(dragPoint.x(), dragPoint.y() - direction * height);

    QVector<QPointF> points;
    points << baseLeft << baseRight << apex << baseLeft;
    return points;
}

QVector<QPointF> rightTriangle(const QRectF& dragRect)
{
    // topLeft() is the press point (right-angle corner); bottomRight() is the
    // drag point. Negative width/height keep both legs in every direction.
    const QPointF press = dragRect.topLeft();
    const QPointF drag = dragRect.bottomRight();

    QVector<QPointF> points;
    points << press
           << QPointF(drag.x(), press.y())
           << drag
           << press;
    return points;
}

QPointF freeTriangleApex(const QPointF& baseStart, const QPointF& baseEnd)
{
    const QPointF base = baseEnd - baseStart;
    const qreal length = std::hypot(base.x(), base.y());
    if (length <= 0.0)
        return baseStart;

    // Fixed normal relative to the base direction (scene coordinates, y down),
    // so the apex rotates continuously with the base instead of flipping.
    const QPointF normal(base.y() / length, -base.x() / length);

    return (baseStart + baseEnd) / 2.0 + normal * (length * 0.8660254037844386);
}

QVector<QPointF> freeTriangle(const QPointF& baseStart, const QPointF& baseEnd,
                              const QPointF& apex)
{
    QVector<QPointF> points;
    points << baseStart << baseEnd << apex << baseStart;
    return points;
}

QVector<QPointF> freeTriangle(const QRectF& dragRect)
{
    const QPointF baseStart = dragRect.topLeft();
    const QPointF baseEnd = dragRect.bottomRight();
    return freeTriangle(baseStart, baseEnd, freeTriangleApex(baseStart, baseEnd));
}

bool isEditableKind(int kind)
{
    return kind == 6 || kind == 12;
}

QPointF initialShapeHandle(int kind, const QRectF& dragRect)
{
    if (kind == 12)
        return freeTriangleApex(dragRect.topLeft(), dragRect.bottomRight());

    if (kind == 6)
    {
        const QRectF box = boxOf(dragRect);
        const qreal length = box.width() * 0.75;
        return QPointF(box.left() + box.width() * 0.25 + length / 2.0, box.top());
    }

    return dragRect.topLeft();
}

QVector<QPointF> editableShapePoints(int kind, const QRectF& dragRect,
                                    const QPointF& handlePos)
{
    if (kind == 12)
        return freeTriangle(dragRect.topLeft(), dragRect.bottomRight(), handlePos);

    if (kind == 6)
    {
        const QRectF box = boxOf(dragRect);
        const qreal length = box.width() * 0.75;

        // The bottom edge is fixed; the top edge keeps its length and is
        // centred on the movable handle.
        QVector<QPointF> points;
        points << QPointF(box.left(), box.bottom())
               << QPointF(handlePos.x() - length / 2.0, handlePos.y())
               << QPointF(handlePos.x() + length / 2.0, handlePos.y())
               << QPointF(box.left() + length, box.bottom())
               << QPointF(box.left(), box.bottom());
        return points;
    }

    return pointsForKind(kind, dragRect);
}

namespace
{

const QVector<ShapeDefinition> sShapes = {
    {0, "Rectangle", false, &rectangle},
    {1, "Ellipse", false, &ellipse},
    {2, "Circle", true, &circle},
    {3, "Square", true, &square},
    {4, "Triangle", false, &triangle},
    {5, "Diamond", false, &diamond},
    {6, "Parallelogram", false, &parallelogram},
    {7, "Pentagon", false, &pentagon},
    {8, "Hexagon", false, &hexagon},
    {9, "Star", false, &star},
    // Reserved: the equilateral triangle is merged into the free triangle (12).
    // The slot keeps its number because settings persist kind values.
    {10, "Equilateral triangle", true, &equilateralTriangle},
    {11, "Right-angle triangle", false, &rightTriangle},
    {12, "Free triangle", false, &freeTriangle}
};

} // namespace

const QVector<ShapeDefinition>& shapes()
{
    return sShapes;
}

QVector<QPointF> pointsForKind(int kind, const QRectF& dragRect)
{
    if (kind < 0 || kind >= sShapes.size())
        kind = 0;

    return sShapes.at(kind).points(dragRect);
}

QString nameForKind(int kind)
{
    if (kind < 0 || kind >= sShapes.size())
        kind = 0;

    return QString::fromLatin1(sShapes.at(kind).name);
}

int shapeKindCount()
{
    return sShapes.size();
}

bool isConstrainedKind(int kind)
{
    if (kind < 0 || kind >= sShapes.size())
        return false;

    return sShapes.at(kind).constrainedBox;
}

} // namespace UBShapeGeometry
