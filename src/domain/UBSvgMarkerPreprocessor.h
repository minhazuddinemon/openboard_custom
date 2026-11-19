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

#ifndef UBSVGMARKERPREPROCESSOR_H_
#define UBSVGMARKERPREPROCESSOR_H_

#include <QByteArray>

namespace UBSvgMarkerPreprocessor {

// Qt's QSvgRenderer (a subset of SVG) does not support SVG markers
// (marker-start / marker-mid / marker-end), so arrowheads defined with
// <marker> are silently dropped. This expands the markers used by line,
// polyline, polygon and path elements into explicit geometry (a <g> per
// marker placement) with the correct position, orientation and scale.
//
// The input is returned unchanged when it does not define any marker, so
// SVGs without markers pay only for a cheap byte scan.
QByteArray process(const QByteArray &svgData);

} // namespace UBSvgMarkerPreprocessor

#endif /* UBSVGMARKERPREPROCESSOR_H_ */
