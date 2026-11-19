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
#include <QVector>

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

  // The trail fades only after the pen is lifted. A negative fade start
  // means "active" (pen still down): the trail stays fully visible.
  void startFade(qint64 ms) { mFadeStartMs = ms; }
  qint64 fadeStartMs() const { return mFadeStartMs; }
  bool isFading() const { return mFadeStartMs >= 0; }

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
  void simplifyPath();

  bool mHasPoint;
  QVector<QPointF> mPoints;
  qint64 mFadeStartMs{-1};
};

#endif /* UBGRAPHICSLASERSTROKE_H_ */
