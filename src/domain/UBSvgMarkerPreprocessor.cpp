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

#include "UBSvgMarkerPreprocessor.h"

#include <QDomDocument>
#include <QDomElement>
#include <QDomNamedNodeMap>
#include <QDomNodeList>
#include <QHash>
#include <QPointF>
#include <QStringList>
#include <QTransform>
#include <QVector>

#include <cmath>

namespace {

constexpr qreal Pi = 3.14159265358979323846;

// ---------------------------------------------------------------------------
// Small parsing helpers
// ---------------------------------------------------------------------------

qreal parseNumber(const QString &text, bool *ok = nullptr) {
  const QString trimmed = text.trimmed();

  int length = 0;
  while (length < trimmed.size()) {
    const QChar c = trimmed.at(length);
    if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
        c == 'e' || c == 'E') {
      ++length;
    } else {
      break;
    }
  }

  bool good = false;
  const qreal value = trimmed.left(length).toDouble(&good);
  if (ok)
    *ok = good;
  return good ? value : 0.0;
}

qreal attributeNumber(const QDomElement &element, const QString &name,
                      qreal fallback) {
  if (!element.hasAttribute(name))
    return fallback;

  bool ok = false;
  const qreal value = parseNumber(element.attribute(name), &ok);
  return ok ? value : fallback;
}

bool hasOwnStrokeWidth(const QDomElement &element, qreal *width) {
  if (element.hasAttribute("stroke-width")) {
    *width = attributeNumber(element, "stroke-width", -1.0);
    return *width > 0.0;
  }

  const QString style = element.attribute("style");
  const int index = style.indexOf("stroke-width");
  if (index >= 0) {
    const int colon = style.indexOf(':', index);
    if (colon < 0)
      return false;
    int semi = style.indexOf(';', colon);
    if (semi < 0)
      semi = style.size();
    bool ok = false;
    const qreal value =
        parseNumber(style.mid(colon + 1, semi - colon - 1), &ok);
    if (ok && value > 0.0) {
      *width = value;
      return true;
    }
  }

  return false;
}

qreal inheritedStrokeWidth(const QDomElement &element) {
  qreal width = 0.0;
  for (QDomElement current = element; !current.isNull();
       current = current.parentNode().toElement()) {
    if (hasOwnStrokeWidth(current, &width))
      return width;
  }
  return 1.0;
}

QString markerIdFromValue(QString value) {
  value = value.trimmed();
  if (value.startsWith("url(", Qt::CaseInsensitive)) {
    const int end = value.indexOf(')');
    if (end > 0)
      value = value.mid(4, end - 4).trimmed();
  }
  if (value.startsWith('#'))
    value = value.mid(1);
  value.remove('\'');
  value.remove('"');
  return value.trimmed();
}

qreal angleOfVector(const QPointF &vector) {
  if (vector.isNull())
    return 0.0;
  return std::atan2(vector.y(), vector.x()) * 180.0 / Pi;
}

QString matrixString(const QTransform &transform) {
  return QString("matrix(%1,%2,%3,%4,%5,%6)")
      .arg(transform.m11(), 0, 'g', 12)
      .arg(transform.m12(), 0, 'g', 12)
      .arg(transform.m21(), 0, 'g', 12)
      .arg(transform.m22(), 0, 'g', 12)
      .arg(transform.dx(), 0, 'g', 12)
      .arg(transform.dy(), 0, 'g', 12);
}

QVector<qreal> scanNumbers(const QString &text) {
  QVector<qreal> numbers;

  int i = 0;
  while (i < text.size()) {
    const QChar c = text.at(i);
    if (c.isSpace() || c == ',') {
      ++i;
      continue;
    }

    const int start = i;
    if (c == '+' || c == '-')
      ++i;

    bool dot = false;
    bool exponent = false;
    while (i < text.size()) {
      const QChar ch = text.at(i);
      if (ch >= '0' && ch <= '9') {
        ++i;
      } else if (ch == '.' && !dot && !exponent) {
        dot = true;
        ++i;
      } else if ((ch == 'e' || ch == 'E') && !exponent) {
        exponent = true;
        ++i;
        if (i < text.size() && (text.at(i) == '+' || text.at(i) == '-'))
          ++i;
      } else {
        break;
      }
    }

    if (i == start) {
      ++i; // not a number; skip the character
      continue;
    }

    bool ok = false;
    const qreal value = text.mid(start, i - start).toDouble(&ok);
    if (ok)
      numbers.append(value);
  }

  return numbers;
}

// ---------------------------------------------------------------------------
// Path geometry: we only need the anchor points (command end points) and the
// subpath boundaries. Segment directions are derived from consecutive anchors
// (exact for straight segments, a good approximation for curves).
// ---------------------------------------------------------------------------

struct PathToken {
  bool isCommand;
  QChar command;
  qreal number;
};

int pathArgumentCount(QChar command) {
  switch (command.toLatin1()) {
  case 'M':
  case 'L':
  case 'T':
    return 2;
  case 'H':
  case 'V':
    return 1;
  case 'C':
    return 6;
  case 'S':
  case 'Q':
    return 4;
  case 'A':
    return 7;
  case 'Z':
    return 0;
  default:
    return -1;
  }
}

QVector<PathToken> tokenizePath(const QString &data) {
  QVector<PathToken> tokens;

  int i = 0;
  while (i < data.size()) {
    const QChar c = data.at(i);
    if (c.isSpace() || c == ',') {
      ++i;
      continue;
    }

    if (c.isLetter()) {
      PathToken token;
      token.isCommand = true;
      token.command = c;
      token.number = 0.0;
      tokens.append(token);
      ++i;
      continue;
    }

    const int start = i;
    if (c == '+' || c == '-')
      ++i;

    bool dot = false;
    bool exponent = false;
    while (i < data.size()) {
      const QChar ch = data.at(i);
      if (ch >= '0' && ch <= '9') {
        ++i;
      } else if (ch == '.' && !dot && !exponent) {
        dot = true;
        ++i;
      } else if ((ch == 'e' || ch == 'E') && !exponent) {
        exponent = true;
        ++i;
        if (i < data.size() && (data.at(i) == '+' || data.at(i) == '-'))
          ++i;
      } else {
        break;
      }
    }

    bool ok = false;
    const qreal value = data.mid(start, i - start).toDouble(&ok);
    if (ok) {
      PathToken token;
      token.isCommand = false;
      token.command = QChar();
      token.number = value;
      tokens.append(token);
    } else if (i == start) {
      ++i;
    }
  }

  return tokens;
}

void parsePath(const QString &data, QVector<QPointF> &anchors,
               QVector<bool> &breaks) {
  const QVector<PathToken> tokens = tokenizePath(data);

  QPointF current(0.0, 0.0);
  QPointF subpathStart(0.0, 0.0);

  int index = 0;
  while (index < tokens.size()) {
    if (!tokens.at(index).isCommand) {
      ++index;
      continue;
    }

    QChar command = tokens.at(index).command;
    ++index;

    const bool relative = command.isLower();
    QChar upper = command.toUpper();
    int argumentCount = pathArgumentCount(upper);

    if (upper == 'Z') {
      current = subpathStart;
      anchors.append(current);
      breaks.append(false);
      continue;
    }

    if (argumentCount <= 0)
      continue;

    bool firstSet = true;
    while (index + argumentCount <= tokens.size()) {
      bool valid = true;
      for (int k = 0; k < argumentCount; ++k) {
        if (tokens.at(index + k).isCommand) {
          valid = false;
          break;
        }
      }
      if (!valid)
        break;

      QVector<qreal> args;
      for (int k = 0; k < argumentCount; ++k)
        args.append(tokens.at(index + k).number);
      index += argumentCount;

      QPointF point = current;
      switch (upper.toLatin1()) {
      case 'M':
        point = relative ? current + QPointF(args.at(0), args.at(1))
                         : QPointF(args.at(0), args.at(1));
        anchors.append(point);
        breaks.append(true);
        current = point;
        subpathStart = point;
        // Implicitly repeated moveto coordinates are linetos.
        upper = QChar('L');
        break;
      case 'L':
        point = relative ? current + QPointF(args.at(0), args.at(1))
                         : QPointF(args.at(0), args.at(1));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'H':
        point = QPointF(relative ? current.x() + args.at(0) : args.at(0),
                        current.y());
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'V':
        point = QPointF(current.x(),
                        relative ? current.y() + args.at(0) : args.at(0));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'C':
        point = relative ? current + QPointF(args.at(4), args.at(5))
                         : QPointF(args.at(4), args.at(5));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'S':
        point = relative ? current + QPointF(args.at(2), args.at(3))
                         : QPointF(args.at(2), args.at(3));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'Q':
        point = relative ? current + QPointF(args.at(2), args.at(3))
                         : QPointF(args.at(2), args.at(3));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'T':
        point = relative ? current + QPointF(args.at(0), args.at(1))
                         : QPointF(args.at(0), args.at(1));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      case 'A':
        point = relative ? current + QPointF(args.at(5), args.at(6))
                         : QPointF(args.at(5), args.at(6));
        anchors.append(point);
        breaks.append(false);
        current = point;
        break;
      default:
        break;
      }

      firstSet = false;
    }

    Q_UNUSED(firstSet)
  }
}

// ---------------------------------------------------------------------------
// Marker transform
// ---------------------------------------------------------------------------

QTransform markerTransform(const QDomElement &marker, const QPointF &vertex,
                           qreal orientationDegrees, qreal strokeWidth) {
  qreal unitScale = 1.0;
  if (marker.attribute("markerUnits", "strokeWidth") != "userSpaceOnUse")
    unitScale = strokeWidth > 0.0 ? strokeWidth : 1.0;

  const qreal markerWidth = attributeNumber(marker, "markerWidth", 3.0);
  const qreal markerHeight = attributeNumber(marker, "markerHeight", 3.0);
  const qreal viewportWidth = markerWidth * unitScale;
  const qreal viewportHeight = markerHeight * unitScale;

  const qreal refX = attributeNumber(marker, "refX", 0.0);
  const qreal refY = attributeNumber(marker, "refY", 0.0);

  QTransform transform;
  transform.translate(vertex.x(), vertex.y());
  transform.rotate(orientationDegrees);

  const QVector<qreal> viewBoxNumbers =
      scanNumbers(marker.attribute("viewBox"));
  if (viewBoxNumbers.size() >= 4 && viewBoxNumbers.at(2) > 0.0 &&
      viewBoxNumbers.at(3) > 0.0) {
    const qreal vx = viewBoxNumbers.at(0);
    const qreal vy = viewBoxNumbers.at(1);
    const qreal vw = viewBoxNumbers.at(2);
    const qreal vh = viewBoxNumbers.at(3);

    const qreal scale = qMin(viewportWidth / vw, viewportHeight / vh);
    const qreal tx = (viewportWidth - vw * scale) / 2.0 - vx * scale;
    const qreal ty = (viewportHeight - vh * scale) / 2.0 - vy * scale;

    const qreal refViewportX = refX * scale + tx;
    const qreal refViewportY = refY * scale + ty;

    transform.translate(-refViewportX, -refViewportY);
    transform.translate(tx, ty);
    transform.scale(scale, scale);
  } else {
    transform.translate(-refX * unitScale, -refY * unitScale);
    transform.scale(unitScale, unitScale);
  }

  return transform;
}

qreal markerOrientation(const QDomElement &marker, qreal autoAngle,
                        bool isStart) {
  const QString orient = marker.attribute("orient", "0").trimmed();
  if (orient == "auto")
    return autoAngle;
  if (orient == "auto-start-reverse")
    return isStart ? autoAngle + 180.0 : autoAngle;

  bool ok = false;
  const qreal value = parseNumber(orient, &ok);
  return ok ? value : 0.0;
}

// The expanded marker is inserted next to the shape, which may sit inside a
// group that sets stroke, stroke-width, stroke-linejoin, fill, opacity, ... .
// Marker content must not inherit those; per SVG it inherits from the <marker>
// element and its ancestors (normally <defs>/<svg>). Pin the inherited values
// on the generated group so the insertion context cannot leak in (for example,
// a round line-join must not round the corners of a filled arrowhead).
QString styleProperty(const QString& style, const QString& name)
{
    int index = 0;
    while ((index = style.indexOf(name, index)) >= 0)
    {
        const int after = index + name.size();
        const bool boundaryBefore = index == 0
                || style.at(index - 1) == ';' || style.at(index - 1).isSpace();
        int colon = after;
        while (colon < style.size() && style.at(colon).isSpace())
            ++colon;

        if (boundaryBefore && colon < style.size() && style.at(colon) == ':')
        {
            int end = style.indexOf(';', colon + 1);
            if (end < 0)
                end = style.size();
            return style.mid(colon + 1, end - colon - 1).trimmed();
        }
        index = after;
    }
    return QString();
}

QString inheritedProperty(const QDomElement& start, const QString& name,
                          const QString& fallback)
{
    for (QDomElement current = start; !current.isNull();
         current = current.parentNode().toElement())
    {
        if (current.hasAttribute(name))
            return current.attribute(name);

        const QString value = styleProperty(current.attribute("style"), name);
        if (!value.isEmpty())
            return value;
    }
    return fallback;
}

QDomElement makeMarkerGroup(QDomDocument &document, const QDomElement &marker,
                            const QTransform &transform) {
  QDomElement group = document.createElement("g");
  group.setAttribute("transform", matrixString(transform));

  // Block inheritance from wherever the marker is inserted, using the values
  // the marker content is meant to inherit from <marker>/<defs>/<svg>.
  struct InheritedProperty {
    const char* name;
    const char* value;
  };
  static const InheritedProperty inherited[] = {
      {"fill", "black"},          {"fill-opacity", "1"},
      {"fill-rule", "nonzero"},   {"stroke", "none"},
      {"stroke-width", "1"},      {"stroke-opacity", "1"},
      {"stroke-linecap", "butt"}, {"stroke-linejoin", "miter"},
      {"stroke-miterlimit", "4"}, {"stroke-dasharray", "none"},
      {"stroke-dashoffset", "0"}, {"opacity", "1"},
      {"color", "black"},         {"visibility", "visible"}};
  for (const InheritedProperty& property : inherited) {
    group.setAttribute(property.name,
                       inheritedProperty(marker, property.name, property.value));
  }

  // Carry the marker element's presentation attributes (fill, stroke, ...)
  // onto the group, but not the geometry-specific marker attributes.
  const QStringList skipped = {
      "id",          "viewBox",     "refX",
      "refY",        "markerWidth", "markerHeight",
      "markerUnits", "orient",      "preserveAspectRatio"};

  const QDomNamedNodeMap attributes = marker.attributes();
  for (int i = 0; i < attributes.count(); ++i) {
    const QDomAttr attribute = attributes.item(i).toAttr();
    if (attribute.isNull() || skipped.contains(attribute.name()))
      continue;
    group.setAttribute(attribute.name(), attribute.value());
  }

  for (QDomNode node = marker.firstChild(); !node.isNull();
       node = node.nextSibling()) {
    if (node.isElement() || node.isText() || node.isCDATASection())
      group.appendChild(node.cloneNode(true));
  }

  return group;
}

// ---------------------------------------------------------------------------
// Per-shape vertex extraction
// ---------------------------------------------------------------------------

bool shapeVertices(const QDomElement &element, QVector<QPointF> &anchors,
                   QVector<bool> &breaks) {
  const QString tag = element.tagName();

  if (tag == "line") {
    anchors.append(QPointF(attributeNumber(element, "x1", 0.0),
                           attributeNumber(element, "y1", 0.0)));
    anchors.append(QPointF(attributeNumber(element, "x2", 0.0),
                           attributeNumber(element, "y2", 0.0)));
    breaks.append(true);
    breaks.append(false);
    return true;
  }

  if (tag == "polyline" || tag == "polygon") {
    const QVector<qreal> numbers = scanNumbers(element.attribute("points"));
    for (int i = 0; i + 1 < numbers.size(); i += 2) {
      anchors.append(QPointF(numbers.at(i), numbers.at(i + 1)));
      breaks.append(anchors.size() == 1);
    }
    return !anchors.isEmpty();
  }

  if (tag == "path") {
    parsePath(element.attribute("d"), anchors, breaks);
    return !anchors.isEmpty();
  }

  return false;
}

} // namespace

namespace UBSvgMarkerPreprocessor {

QByteArray process(const QByteArray &svgData) {
  // Cheap guard: SVGs without markers are returned untouched, with no XML
  // parsing at all.
  if (!svgData.contains("<marker") || !svgData.contains("marker-"))
    return svgData;

  QDomDocument document;
  if (!document.setContent(svgData, false))
    return svgData;

  QDomElement root = document.documentElement();
  if (root.isNull() || root.tagName() != "svg")
    return svgData;

  // Index the marker definitions by id.
  QHash<QString, QDomElement> markers;
  const QDomNodeList markerNodes = document.elementsByTagName("marker");
  for (int i = 0; i < markerNodes.count(); ++i) {
    const QDomElement marker = markerNodes.item(i).toElement();
    if (!marker.isNull() && marker.hasAttribute("id"))
      markers.insert(marker.attribute("id"), marker);
  }
  if (markers.isEmpty())
    return svgData;

  // Collect the target elements before modifying the document.
  QVector<QDomElement> targets;
  const QStringList supported = {"line", "polyline", "polygon", "path"};
  for (const QString &tag : supported) {
    const QDomNodeList nodes = document.elementsByTagName(tag);
    for (int i = 0; i < nodes.count(); ++i) {
      const QDomElement element = nodes.item(i).toElement();
      if (element.isNull())
        continue;
      if (element.hasAttribute("marker-start") ||
          element.hasAttribute("marker-mid") ||
          element.hasAttribute("marker-end")) {
        targets.append(element);
      }
    }
  }

  bool changed = false;

  for (QDomElement &shape : targets) {
    QVector<QPointF> anchors;
    QVector<bool> breaks;
    if (!shapeVertices(shape, anchors, breaks) || anchors.isEmpty())
      continue;

    const qreal strokeWidth = inheritedStrokeWidth(shape);

    // Segment direction ending at anchor i (valid when !breaks[i] and
    // the segment has non-zero length).
    auto segmentDirection = [&](int i) -> QPointF {
      if (i <= 0 || i >= anchors.size() || breaks.at(i))
        return QPointF();
      return anchors.at(i) - anchors.at(i - 1);
    };

    QVector<QDomElement> groups;

    auto addMarker = [&](const QString &attribute, const QPointF &vertex,
                         qreal autoAngle, bool isStart) {
      if (!shape.hasAttribute(attribute))
        return;
      const QString id = markerIdFromValue(shape.attribute(attribute));
      if (id.isEmpty() || !markers.contains(id))
        return;

      const QDomElement marker = markers.value(id);
      const qreal orientation = markerOrientation(marker, autoAngle, isStart);
      const QTransform transform =
          markerTransform(marker, vertex, orientation, strokeWidth);
      groups.append(makeMarkerGroup(document, marker, transform));
    };

    // marker-start
    if (shape.hasAttribute("marker-start")) {
      QPointF startDirection;
      for (int i = 1; i < anchors.size(); ++i) {
        const QPointF direction = segmentDirection(i);
        if (!direction.isNull()) {
          startDirection = direction;
          break;
        }
      }
      addMarker("marker-start", anchors.first(), angleOfVector(startDirection),
                true);
    }

    // marker-mid: one marker per interior vertex, oriented along the
    // bisector of the incoming and outgoing segments.
    if (shape.hasAttribute("marker-mid")) {
      for (int i = 1; i + 1 < anchors.size(); ++i) {
        const QPointF incoming = segmentDirection(i);
        const QPointF outgoing = segmentDirection(i + 1);
        if (incoming.isNull() || outgoing.isNull())
          continue;

        QPointF bisector = incoming / std::hypot(incoming.x(), incoming.y()) +
                           outgoing / std::hypot(outgoing.x(), outgoing.y());
        if (bisector.isNull())
          bisector = incoming;

        addMarker("marker-mid", anchors.at(i), angleOfVector(bisector), false);
      }
    }

    // marker-end
    if (shape.hasAttribute("marker-end")) {
      QPointF endDirection;
      for (int i = anchors.size() - 1; i >= 1; --i) {
        const QPointF direction = segmentDirection(i);
        if (!direction.isNull()) {
          endDirection = direction;
          break;
        }
      }
      addMarker("marker-end", anchors.last(), angleOfVector(endDirection),
                false);
    }

    if (groups.isEmpty())
      continue;

    const QString shapeTransform = shape.attribute("transform").trimmed();

    QDomNode parent = shape.parentNode();
    QDomNode reference = shape;
    for (const QDomElement &group : groups) {
      QDomElement insertElement = group;
      if (!shapeTransform.isEmpty()) {
        // Keep the marker in the shape's local coordinate system.
        QDomElement wrapper = document.createElement("g");
        wrapper.setAttribute("transform", shapeTransform);
        wrapper.appendChild(group);
        insertElement = wrapper;
      }
      reference = parent.insertAfter(insertElement, reference);
    }

    shape.removeAttribute("marker-start");
    shape.removeAttribute("marker-mid");
    shape.removeAttribute("marker-end");
    changed = true;
  }

  if (!changed)
    return svgData;

  return document.toByteArray(-1);
}

} // namespace UBSvgMarkerPreprocessor
