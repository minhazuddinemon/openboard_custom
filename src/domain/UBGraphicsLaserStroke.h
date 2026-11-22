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

#ifndef UBGRAPHICSLASERSTROKE_H_
#define UBGRAPHICSLASERSTROKE_H_

#include <QColor>
#include <QGraphicsObject>
#include <QPainterPath>
#include <QPointF>

/**
 * A transient, glowing stroke used by the virtual laser pointer.
 *
 * Derives from QGraphicsObject so the scene can safely defer its deletion with
 * deleteLater() instead of deleting it synchronously inside an input event.
 * It is intentionally *not* a UBItem and is never registered in the document
 * model, therefore it cannot pollute the undo stack or saved documents.
 */
class UBGraphicsLaserStroke : public QGraphicsObject {
  Q_OBJECT

public:
  UBGraphicsLaserStroke(const QColor &color, qreal outerWidth, qreal innerWidth,
                        QGraphicsItem *parent = 0);

  void appendPoint(const QPointF &point);

  // Number of points actually recorded in this trail (used by the scene to
  // bound the cost of one continuous stroke).
  int pointCount() const { return mPointCount; }

  QRectF boundingRect() const override;

protected:
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
             QWidget *widget) override;

private:
  QPainterPath mPath;
  QPointF mLastPoint;
  QColor mColor;
  qreal mOuterWidth;
  qreal mInnerWidth;
  bool mHasPoint;
  int mPointCount;
};

#endif /* UBGRAPHICSLASERSTROKE_H_ */
