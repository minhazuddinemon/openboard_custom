/*
 * Copyright (C) 2015-2022 Département de l'Instruction Publique (DIP-SEM)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "UBGraphicsLaserStroke.h"

#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

namespace {
// Keep the laser trail above regular content but below the selection frame.
constexpr qreal kLaserZValue = 1.0e7;

// Alpha of the wide outer glow (~35 % so the halo stays soft).
constexpr int kGlowAlpha = 90;

// Minimum distance between two recorded points (scene units).
constexpr qreal kMinPointDistance = 0.5;

// Upper bound on recorded points. When exceeded, older points are
// simplified away (every other interior point), which bounds the render
// cost of very long strokes without ever breaking the line.
constexpr int kMaxPoints = 512;
} // namespace

UBGraphicsLaserStroke::UBGraphicsLaserStroke(const QColor &color,
                                             qreal outerWidth, qreal innerWidth,
                                             QGraphicsItem *parent)
    : QGraphicsObject(parent), mColor(color),
      mOuterWidth(qMax<qreal>(outerWidth, 1.0)),
      mInnerWidth(qMax<qreal>(innerWidth, 1.0)), mHasPoint(false) {
  setAcceptedMouseButtons(Qt::NoButton);
  setZValue(kLaserZValue);
  setOpacity(1.0);
}

QRectF UBGraphicsLaserStroke::boundingRect() const {
  if (mPath.isEmpty())
    return QRectF();

  // Account for the widest pen pass (round caps extend by half the width).
  const qreal margin = mOuterWidth / 2.0 + 2.0;
  return mPath.boundingRect().adjusted(-margin, -margin, margin, margin);
}

void UBGraphicsLaserStroke::appendPoint(const QPointF &point) {
  if (mHasPoint && QLineF(mLastPoint, point).length() < kMinPointDistance)
    return;

  prepareGeometryChange();

  if (!mHasPoint) {
    mPath = QPainterPath(point);
    mHasPoint = true;
  } else {
    mPath.lineTo(point);
  }

  mLastPoint = point;
  mPoints.append(point);

  if (mPoints.size() > kMaxPoints)
    simplifyPath();

  update();
}

void UBGraphicsLaserStroke::simplifyPath() {
  if (mPoints.size() < 8)
    return;

  // Keep the endpoints and every other interior point. Consecutive
  // retained points are still connected, so the line never gets a gap;
  // older portions simply become coarser the longer the stroke grows.
  QVector<QPointF> reduced;
  reduced.reserve(mPoints.size() / 2 + 2);
  reduced.append(mPoints.first());
  for (int i = 1; i + 1 < mPoints.size(); i += 2)
    reduced.append(mPoints.at(i));
  reduced.append(mPoints.last());

  mPoints = reduced;

  QPainterPath path(mPoints.first());
  for (int i = 1; i < mPoints.size(); ++i)
    path.lineTo(mPoints.at(i));
  mPath = path;
  mLastPoint = mPoints.last();
}

void UBGraphicsLaserStroke::paint(QPainter *painter,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  if (mPath.isEmpty())
    return;

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  // The laser is a line, never a filled shape.
  painter->setBrush(Qt::NoBrush);
  painter->setCompositionMode(QPainter::CompositionMode_SourceOver);

  // Outer glow: wide, semi-transparent pass.
  QColor glowColor = mColor;
  glowColor.setAlpha(kGlowAlpha);
  QPen glowPen(glowColor);
  glowPen.setWidthF(mOuterWidth);
  glowPen.setStyle(Qt::SolidLine);
  glowPen.setCapStyle(Qt::RoundCap);
  glowPen.setJoinStyle(Qt::RoundJoin);
  painter->setPen(glowPen);
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(mPath);

  // Inner core: thin, fully opaque pass.
  QColor coreColor(mColor.red(), mColor.green(), mColor.blue(), 255);
  QPen corePen(coreColor);
  corePen.setWidthF(mInnerWidth);
  corePen.setStyle(Qt::SolidLine);
  corePen.setCapStyle(Qt::RoundCap);
  corePen.setJoinStyle(Qt::RoundJoin);
  painter->setPen(corePen);
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(mPath);

  painter->restore();
}
