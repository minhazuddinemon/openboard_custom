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
 * the Free Software Foundation, version 3,
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

#ifndef UBGRAPHICSITEMCOLORUNDOCOMMAND_H
#define UBGRAPHICSITEMCOLORUNDOCOMMAND_H

#include <QColor>
#include <QList>

#include "UBUndoCommand.h"

class QGraphicsItem;

/**
 * Undo command for changing the color of stroke-based items.
 *
 * Each entry stores the complete color state (the painted color plus the
 * light/dark background twins) of a single item before and after the change.
 * The command is intentionally limited to UBGraphicsStrokesGroup and
 * UBGraphicsPolygonItem, which are the only vector/stroke items exposed by
 * OpenBoard.
 */
class UBGraphicsItemColorUndoCommand : public UBUndoCommand {
public:
  struct ColorState {
    QColor current;
    QColor light;
    QColor dark;

    bool operator==(const ColorState &other) const {
      return current == other.current && light == other.light &&
             dark == other.dark;
    }

    bool operator!=(const ColorState &other) const { return !(*this == other); }
  };

  struct Entry {
    QGraphicsItem *item;
    ColorState before;
    ColorState after;
  };

  explicit UBGraphicsItemColorUndoCommand(const QList<Entry> &entries,
                                          QUndoCommand *parent = 0);
  virtual ~UBGraphicsItemColorUndoCommand();

  // Reads the current color state of a supported item. Returns an invalid
  // ColorState (all invalid colors) for unsupported items.
  static ColorState stateOf(QGraphicsItem *item);

  // Applies a stored state to a supported item. No-op for other item types.
  static void apply(QGraphicsItem *item, const ColorState &state);

  // Returns true if the item is handled by this undo command.
  static bool supports(QGraphicsItem *item);

protected:
  virtual void undo();
  virtual void redo();

private:
  QList<Entry> mEntries;
};

#endif // UBGRAPHICSITEMCOLORUNDOCOMMAND_H
