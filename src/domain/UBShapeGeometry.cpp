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

using UBShapeGeometry::StrokePiece;

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

constexpr int kRoundedRectangleKind = 13;
constexpr int kRoundedSquareKind = 14;
constexpr int kArrowKind = 16;
constexpr int kDoubleArrowKind = 17;

// Arrow head: proportional to the stroke width, never below a sensible
// minimum, and never longer than a fraction of the shaft. Toolbar icons have
// no stroke width and use a fraction of the shaft instead.
constexpr qreal kArrowHeadMinSize = 8.0;
constexpr qreal kArrowHeadWidthFactor = 6.0;
constexpr qreal kArrowHeadIconFraction = 0.35;
constexpr qreal kArrowBarbRatio = 0.5773502691896258; // tan(30 degrees)
constexpr qreal kArrowHeadMaxLineFraction = 0.4;

// Maximum corner radius in scene units. Tune this to make the corners rounder
// or sharper.
constexpr qreal kRoundedCornerMaxRadius = 32.0;

// Excalidraw-style corner: radius follows the short side, capped at a fixed
// scene-unit maximum.
qreal roundedCornerRadius(qreal shorterSide)
{
    if (shorterSide <= 0.0)
        return 0.0;

    const qreal radius = qMin(shorterSide * 0.25, kRoundedCornerMaxRadius);
    return qBound(0.0, radius, shorterSide / 2.0);
}

QPointF quadraticBezierPoint(const QPointF& start, const QPointF& control,
                             const QPointF& end, qreal t)
{
    const qreal oneMinusT = 1.0 - t;
    return start * (oneMinusT * oneMinusT)
            + control * (2.0 * oneMinusT * t)
            + end * (t * t);
}

QVector<QPointF> roundedRectangleOutline(const QRectF& box, qreal radius)
{
    const qreal limit = qMin(box.width(), box.height()) / 2.0;
    radius = qBound(0.0, radius, limit);

    if (radius <= 0.0)
    {
        QVector<QPointF> square;
        square << box.topLeft() << box.topRight() << box.bottomRight()
               << box.bottomLeft() << box.topLeft();
        return square;
    }

    constexpr int segmentsPerCorner = 10;

    // Traversed clockwise. Each curve runs between two straight edges and uses
    // the sharp box corner as its quadratic control point (Excalidraw style).
    struct Corner
    {
        QPointF start;
        QPointF control;
        QPointF end;
    };

    const Corner corners[4] = {
        { QPointF(box.right() - radius, box.top()), box.topRight(),
          QPointF(box.right(), box.top() + radius) },
        { QPointF(box.right(), box.bottom() - radius), box.bottomRight(),
          QPointF(box.right() - radius, box.bottom()) },
        { QPointF(box.left() + radius, box.bottom()), box.bottomLeft(),
          QPointF(box.left(), box.bottom() - radius) },
        { QPointF(box.left(), box.top() + radius), box.topLeft(),
          QPointF(box.left() + radius, box.top()) }
    };

    QVector<QPointF> points;
    points.reserve(4 * (segmentsPerCorner + 1) + 1);

    for (const Corner& corner : corners)
    {
        for (int i = 0; i <= segmentsPerCorner; ++i)
        {
            const qreal t = qreal(i) / segmentsPerCorner;
            points << quadraticBezierPoint(corner.start, corner.control,
                                           corner.end, t);
        }
    }

    points << points.first();
    return points;
}

QVector<QPointF> roundedRectanglePoints(const QRectF& dragRect)
{
    const QRectF box = boxOf(dragRect);
    const qreal shorter = qMin(box.width(), box.height());
    return roundedRectangleOutline(box, roundedCornerRadius(shorter));
}

QVector<QPointF> roundedSquarePoints(const QRectF& dragRect)
{
    const QRectF box = constrainedBox(dragRect);
    const qreal shorter = qMin(box.width(), box.height());
    return roundedRectangleOutline(box, roundedCornerRadius(shorter));
}

QVector<QPointF> openLinePoints(const QRectF& dragRect)
{
    QVector<QPointF> points;
    points << dragRect.topLeft() << dragRect.bottomRight();
    return points;
}

qreal arrowHeadSize(const QPointF& start, const QPointF& tip, qreal strokeWidth)
{
    const QPointF shaft = tip - start;
    const qreal length = std::hypot(shaft.x(), shaft.y());
    if (length <= 0.0)
        return 0.0;

    const qreal head = (strokeWidth > 0.0)
            ? qMax(strokeWidth * kArrowHeadWidthFactor, kArrowHeadMinSize)
            : length * kArrowHeadIconFraction;

    return qMin(head, length * kArrowHeadMaxLineFraction);
}

QVector<QPointF> arrowPoints(const QRectF& dragRect, qreal strokeWidth)
{
    const QPointF start = dragRect.topLeft();
    const QPointF tip = dragRect.bottomRight();

    // Shaft first, then the head as out-and-back segments in the same stroke.
    QVector<QPointF> points;
    points << start << tip;

    const qreal head = arrowHeadSize(start, tip, strokeWidth);
    if (head > 0.0)
    {
        const QPointF shaft = tip - start;
        const qreal length = std::hypot(shaft.x(), shaft.y());
        const QPointF direction = shaft / length;
        const QPointF normal(-direction.y(), direction.x());
        const QPointF back = tip - direction * head;
        const QPointF barbOffset = normal * (head * kArrowBarbRatio);

        points << back + barbOffset << tip << back - barbOffset;
    }

    return points;
}

QVector<QPointF> doubleArrowPoints(const QRectF& dragRect, qreal strokeWidth)
{
    const QPointF start = dragRect.topLeft();
    const QPointF tip = dragRect.bottomRight();

    QVector<QPointF> points;
    points << start;

    const qreal head = arrowHeadSize(start, tip, strokeWidth);
    if (head <= 0.0)
    {
        points << tip;
        return points;
    }

    const QPointF shaft = tip - start;
    const qreal length = std::hypot(shaft.x(), shaft.y());
    const QPointF direction = shaft / length;
    const QPointF normal(-direction.y(), direction.x());
    const QPointF barbOffset = normal * (head * kArrowBarbRatio);

    const QPointF startBack = start + direction * head;
    const QPointF tipBack = tip - direction * head;

    points << startBack + barbOffset << start << startBack - barbOffset
           << start << tip
           << tipBack + barbOffset << tip << tipBack - barbOffset;

    return points;
}

constexpr qreal kDashLengthFactor = 4.0;
constexpr qreal kDashGapFactor = 3.0;
constexpr qreal kDotLengthFactor = 1.0;
constexpr qreal kDotGapFactor = 2.0;
constexpr qreal kCornerArmFactor = 1.5;

constexpr qreal kMinDashLength = 4.0;
constexpr qreal kMinDashGap = 3.0;
constexpr qreal kMinDotLength = 1.5;
constexpr qreal kMinDotGap = 2.5;
constexpr qreal kMinCornerArm = 2.0;

// Upper bound on the number of pieces generated for one shape. If a shape
// would exceed it, the dash/gap lengths are enlarged.
constexpr int kMaxStrokePieces = 380;

constexpr qreal kCornerTurnDegrees = 30.0;
// The rounded-box outline samples each curved corner. The junction between a
// straight side and the first/last sampled curve segment turns only ~3 deg,
// while the interior curve samples turn ~7-11 deg. The epsilon must sit
// between the two, otherwise the straight sides are misclassified as curved
// and the whole outline is emitted solid.
constexpr qreal kCurveTurnEpsilon = 4.5;

qreal distanceBetween(const QPointF& a, const QPointF& b)
{
    return std::hypot(b.x() - a.x(), b.y() - a.y());
}

QVector<qreal> cumulativeLengths(const QVector<QPointF>& points)
{
    QVector<qreal> cumulative;
    cumulative.reserve(points.size());
    cumulative << 0.0;
    for (int i = 1; i < points.size(); ++i)
        cumulative << cumulative.last() + distanceBetween(points.at(i - 1), points.at(i));
    return cumulative;
}

QPointF pointAtDistance(const QVector<QPointF>& points,
                        const QVector<qreal>& cumulative, qreal distance)
{
    if (points.isEmpty())
        return QPointF();
    if (points.size() == 1)
        return points.first();

    distance = qBound<qreal>(0.0, distance, cumulative.last());

    int index = 1;
    while (index < cumulative.size() - 1 && cumulative.at(index) < distance)
        ++index;

    const qreal segmentStart = cumulative.at(index - 1);
    const qreal segmentLength = cumulative.at(index) - segmentStart;
    if (segmentLength <= 0.0)
        return points.at(index);

    const qreal t = (distance - segmentStart) / segmentLength;
    const QPointF a = points.at(index - 1);
    const QPointF b = points.at(index);
    return a + (b - a) * t;
}

void emitSolidEdges(const QVector<QPointF>& points, QVector<StrokePiece>& pieces)
{
    for (int i = 0; i + 1 < points.size(); ++i)
        pieces << StrokePiece{QLineF(points.at(i), points.at(i + 1)), false};
}

// Fits a symmetric dash/dot pattern to an open run: it starts and ends with
// a dash (or dot) and the gap is adjusted slightly so no dash is cut off.
void emitFittedRun(const QVector<QPointF>& points, qreal dash, qreal gap,
                   bool dots, QVector<StrokePiece>& pieces)
{
    if (points.size() < 2)
        return;

    const QVector<qreal> cumulative = cumulativeLengths(points);
    const qreal total = cumulative.last();

    if (total < 2.0 * dash + gap)
    {
        emitSolidEdges(points, pieces);
        return;
    }

    const int count = int(std::floor((total + gap) / (dash + gap)));
    const qreal fittedGap = (count > 1) ? (total - count * dash) / (count - 1) : 0.0;
    if (count < 2 || fittedGap <= 0.0)
    {
        emitSolidEdges(points, pieces);
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const qreal start = i * (dash + fittedGap);
        pieces << StrokePiece{QLineF(pointAtDistance(points, cumulative, start),
                                     pointAtDistance(points, cumulative, start + dash)),
                              dots};
    }
}

// Continuous pattern around a closed loop; spacing is fitted so it closes
// exactly without a partial dash.
void emitFittedLoop(const QVector<QPointF>& points, qreal dash, qreal gap,
                    bool dots, QVector<StrokePiece>& pieces)
{
    if (points.size() < 2)
        return;

    const QVector<qreal> cumulative = cumulativeLengths(points);
    const qreal total = cumulative.last();

    if (total < 2.0 * (dash + gap))
    {
        emitSolidEdges(points, pieces);
        return;
    }

    const int count = int(std::floor(total / (dash + gap)));
    const qreal fittedGap = (count > 0) ? (total - count * dash) / count : 0.0;
    if (count < 2 || fittedGap <= 0.0)
    {
        emitSolidEdges(points, pieces);
        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const qreal start = i * (dash + fittedGap);
        pieces << StrokePiece{QLineF(pointAtDistance(points, cumulative, start),
                                     pointAtDistance(points, cumulative, start + dash)),
                              dots};
    }
}

QVector<QPointF> uniqueVertices(const QVector<QPointF>& outline, bool* closed)
{
    QVector<QPointF> vertices = outline;
    *closed = (outline.size() > 1
               && distanceBetween(outline.first(), outline.last()) < 0.001);
    if (*closed)
        vertices.removeLast();
    return vertices;
}

void computeEdges(const QVector<QPointF>& vertices, bool closed,
                  QVector<qreal>& lengths, QVector<QPointF>& directions)
{
    const int count = vertices.size();
    lengths = QVector<qreal>(count, 0.0);
    directions = QVector<QPointF>(count);

    for (int i = 0; i < count; ++i)
    {
        const int next = (i + 1) % count;
        if (!closed && i + 1 >= count)
            continue;

        const QPointF a = vertices.at(i);
        const QPointF b = vertices.at(next);
        const qreal length = distanceBetween(a, b);
        lengths[i] = length;
        directions[i] = (length > 0.0) ? (b - a) / length : QPointF();
    }
}

qreal turnDegrees(const QPointF& previousDirection, const QPointF& nextDirection)
{
    const qreal dot = qBound<qreal>(-1.0,
                                    QPointF::dotProduct(previousDirection,
                                                        nextDirection), 1.0);
    return std::acos(dot) * 180.0 / kPi;
}

// Closed polygon with sharp corners: a solid L wraps each corner, the rest of
// each side is fitted with the pattern.
void emitCornerPolygon(const QVector<QPointF>& outline, qreal armLength,
                       qreal dash, qreal gap, bool dots,
                       QVector<StrokePiece>& pieces)
{
    bool closed = false;
    const QVector<QPointF> vertices = uniqueVertices(outline, &closed);
    if (vertices.size() < 3)
    {
        emitSolidEdges(outline, pieces);
        return;
    }

    const int count = vertices.size();
    QVector<qreal> edgeLength;
    QVector<QPointF> edgeDirection;
    computeEdges(vertices, true, edgeLength, edgeDirection);

    QVector<bool> corner(count, false);
    QVector<qreal> armIn(count, 0.0);
    QVector<qreal> armOut(count, 0.0);

    for (int i = 0; i < count; ++i)
    {
        const int previous = (i - 1 + count) % count;
        const qreal turn = turnDegrees(edgeDirection.at(previous),
                                       edgeDirection.at(i));
        if (turn > kCornerTurnDegrees && edgeLength.at(previous) > 0.0
                && edgeLength.at(i) > 0.0)
        {
            corner[i] = true;
            armIn[i] = qMin(armLength, 0.45 * edgeLength.at(previous));
            armOut[i] = qMin(armLength, 0.45 * edgeLength.at(i));
        }
    }

    for (int i = 0; i < count; ++i)
    {
        if (!corner.at(i))
            continue;

        const int previous = (i - 1 + count) % count;
        const QPointF vertex = vertices.at(i);
        if (armIn.at(i) > 0.0)
            pieces << StrokePiece{
                    QLineF(vertex - edgeDirection.at(previous) * armIn.at(i),
                           vertex), false};
        if (armOut.at(i) > 0.0)
            pieces << StrokePiece{
                    QLineF(vertex, vertex + edgeDirection.at(i) * armOut.at(i)),
                    false};
    }

    for (int i = 0; i < count; ++i)
    {
        const int next = (i + 1) % count;
        const qreal trimStart = armOut.at(i);
        const qreal trimEnd = armIn.at(next);
        const qreal span = edgeLength.at(i) - trimStart - trimEnd;
        if (span <= 0.0)
            continue;

        QVector<QPointF> run;
        run << vertices.at(i) + edgeDirection.at(i) * trimStart
            << vertices.at(next) - edgeDirection.at(i) * trimEnd;
        emitFittedRun(run, dash, gap, dots, pieces);
    }
}

// Rounded rectangle/square: the sampled curved corners are solid, the straight
// sides are fitted with the pattern.
void emitRoundedBox(const QVector<QPointF>& outline, qreal dash, qreal gap,
                    bool dots, QVector<StrokePiece>& pieces)
{
    bool closed = false;
    const QVector<QPointF> vertices = uniqueVertices(outline, &closed);
    if (vertices.size() < 3)
    {
        emitSolidEdges(outline, pieces);
        return;
    }

    const int count = vertices.size();
    QVector<qreal> edgeLength;
    QVector<QPointF> edgeDirection;
    computeEdges(vertices, true, edgeLength, edgeDirection);

    QVector<bool> curvedEdge(count, false);
    for (int i = 0; i < count; ++i)
    {
        const int previous = (i - 1 + count) % count;
        if (turnDegrees(edgeDirection.at(previous), edgeDirection.at(i))
                > kCurveTurnEpsilon)
        {
            curvedEdge[i] = true;
            curvedEdge[previous] = true;
        }
    }

    int index = 0;
    while (index < count)
    {
        if (curvedEdge.at(index))
        {
            while (index < count && curvedEdge.at(index))
            {
                pieces << StrokePiece{QLineF(vertices.at(index),
                                             vertices.at((index + 1) % count)),
                                      false};
                ++index;
            }
        }
        else
        {
            const int start = index;
            QVector<QPointF> run;
            run << vertices.at(start);
            while (index < count && !curvedEdge.at(index))
            {
                run << vertices.at((index + 1) % count);
                ++index;
            }
            emitFittedRun(run, dash, gap, dots, pieces);
        }
    }
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

QVector<QPointF> roundedRectangle(const QRectF& dragRect)
{
    return roundedRectanglePoints(dragRect);
}

QVector<QPointF> roundedSquare(const QRectF& dragRect)
{
    return roundedSquarePoints(dragRect);
}

QVector<QPointF> lineShape(const QRectF& dragRect)
{
    return openLinePoints(dragRect);
}

QVector<QPointF> arrowShape(const QRectF& dragRect)
{
    return arrowPoints(dragRect, 0.0);
}

QVector<QPointF> doubleArrowShape(const QRectF& dragRect)
{
    return doubleArrowPoints(dragRect, 0.0);
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
    {12, "Free triangle", false, &freeTriangle},
    {13, "Rounded rectangle", false, &roundedRectangle},
    {14, "Rounded square", true, &roundedSquare},
    {15, "Line", false, &lineShape},
    {16, "Arrow", false, &arrowShape},
    {17, "Double arrow", false, &doubleArrowShape}
};

} // namespace

const QVector<ShapeDefinition>& shapes()
{
    return sShapes;
}

QVector<QPointF> pointsForKind(int kind, const QRectF& dragRect, qreal strokeWidth)
{
    if (kind == kRoundedRectangleKind)
        return roundedRectanglePoints(dragRect);

    if (kind == kRoundedSquareKind)
        return roundedSquarePoints(dragRect);

    if (kind == UBShapeLineKind)
        return openLinePoints(dragRect);

    if (kind == kArrowKind)
        return arrowPoints(dragRect, strokeWidth);

    if (kind == kDoubleArrowKind)
        return doubleArrowPoints(dragRect, strokeWidth);

    if (kind < 0 || kind >= sShapes.size())
        kind = 0;

    return sShapes.at(kind).points(dragRect);
}

QVector<StrokePiece> styledSegments(int kind, const QVector<QPointF>& outline,
                                    qreal strokeWidth, int style)
{
    QVector<StrokePiece> pieces;

    if (style != UBShapeStrokeStyleDashed && style != UBShapeStrokeStyleDotted)
    {
        emitSolidEdges(outline, pieces);
        return pieces;
    }

    if (outline.size() < 2)
        return pieces;

    const bool dotted = (style == UBShapeStrokeStyleDotted);
    qreal dash = dotted
            ? qMax(strokeWidth * kDotLengthFactor, kMinDotLength)
            : qMax(strokeWidth * kDashLengthFactor, kMinDashLength);
    qreal gap = dotted
            ? qMax(strokeWidth * kDotGapFactor, kMinDotGap)
            : qMax(strokeWidth * kDashGapFactor, kMinDashGap);

    // Bound the piece count on very large shapes by enlarging the pattern.
    qreal outlineLength = 0.0;
    for (int i = 1; i < outline.size(); ++i)
        outlineLength += distanceBetween(outline.at(i - 1), outline.at(i));
    const int estimate = int(outlineLength / (dash + gap));
    if (estimate > kMaxStrokePieces)
    {
        const qreal scale = qreal(estimate) / kMaxStrokePieces;
        dash *= scale;
        gap *= scale;
    }

    if (kind == 1 || kind == 2)
    {
        emitFittedLoop(outline, dash, gap, dotted, pieces);
        return pieces;
    }

    // Rounded rectangles and squares are closed outlines with no sharp
    // corners: use one continuous pattern along the whole path (straight
    // sides and curved corners alike), like the ellipse.
    if (kind == kRoundedRectangleKind || kind == kRoundedSquareKind)
    {
        emitFittedLoop(outline, dash, gap, dotted, pieces);
        return pieces;
    }

    if (kind == UBShapeLineKind)
    {
        emitFittedRun(outline, dash, gap, dotted, pieces);
        return pieces;
    }

    if (kind == kArrowKind || kind == kDoubleArrowKind)
    {
        const int shaftEdge = (kind == kArrowKind) ? 0 : 4;
        for (int i = 0; i + 1 < outline.size(); ++i)
        {
            if (i == shaftEdge)
            {
                QVector<QPointF> shaft;
                shaft << outline.at(i) << outline.at(i + 1);
                emitFittedRun(shaft, dash, gap, dotted, pieces);
            }
            else
            {
                pieces << StrokePiece{QLineF(outline.at(i), outline.at(i + 1)), false};
            }
        }
        return pieces;
    }

    const qreal armLength = qMax(strokeWidth * kCornerArmFactor, kMinCornerArm);
    emitCornerPolygon(outline, armLength, dash, gap, dotted, pieces);
    return pieces;
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
