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

#include "UBGraphicsItemColorUndoCommand.h"

#include <QGraphicsItem>

#include "UBGraphicsPolygonItem.h"
#include "UBGraphicsStrokesGroup.h"

#include "core/memcheck.h"

UBGraphicsItemColorUndoCommand::UBGraphicsItemColorUndoCommand(
    const QList<Entry> &entries, QUndoCommand *parent)
    : UBUndoCommand(parent), mEntries(entries) {}

UBGraphicsItemColorUndoCommand::~UBGraphicsItemColorUndoCommand() {}

bool UBGraphicsItemColorUndoCommand::supports(QGraphicsItem *item) {
  if (!item)
    return false;

  return item->type() == UBGraphicsStrokesGroup::Type ||
         item->type() == UBGraphicsPolygonItem::Type;
}

UBGraphicsItemColorUndoCommand::ColorState
UBGraphicsItemColorUndoCommand::stateOf(QGraphicsItem *item) {
  ColorState state;

  if (!item)
    return state;

  if (item->type() == UBGraphicsStrokesGroup::Type) {
    UBGraphicsStrokesGroup *group = static_cast<UBGraphicsStrokesGroup *>(item);
    state.current = group->color(UBGraphicsStrokesGroup::currentColor);
    state.light = group->color(UBGraphicsStrokesGroup::colorOnLightBackground);
    state.dark = group->color(UBGraphicsStrokesGroup::colorOnDarkBackground);
  } else if (item->type() == UBGraphicsPolygonItem::Type) {
    UBGraphicsPolygonItem *polygon = static_cast<UBGraphicsPolygonItem *>(item);
    state.current = polygon->color();
    state.light = polygon->colorOnLightBackground();
    state.dark = polygon->colorOnDarkBackground();
  }

  return state;
}

void UBGraphicsItemColorUndoCommand::apply(QGraphicsItem *item,
                                           const ColorState &state) {
  if (!item)
    return;

  if (item->type() == UBGraphicsStrokesGroup::Type) {
    UBGraphicsStrokesGroup *group = static_cast<UBGraphicsStrokesGroup *>(item);
    group->setColor(state.light,
                    UBGraphicsStrokesGroup::colorOnLightBackground);
    group->setColor(state.dark, UBGraphicsStrokesGroup::colorOnDarkBackground);
    group->setColor(state.current, UBGraphicsStrokesGroup::currentColor);
  } else if (item->type() == UBGraphicsPolygonItem::Type) {
    UBGraphicsPolygonItem *polygon = static_cast<UBGraphicsPolygonItem *>(item);
    polygon->setColorOnLightBackground(state.light);
    polygon->setColorOnDarkBackground(state.dark);
    polygon->setColor(state.current);
  }
}

void UBGraphicsItemColorUndoCommand::undo() {
  foreach (const Entry &entry, mEntries)
    apply(entry.item, entry.before);
}

void UBGraphicsItemColorUndoCommand::redo() {
  foreach (const Entry &entry, mEntries)
    apply(entry.item, entry.after);
}
