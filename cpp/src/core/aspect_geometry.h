#pragma once
#include <QRectF>
#include <Qt>
#include <algorithm>
#include <cmath>

namespace jietuba {
inline bool valid_aspect(double ratio) { return std::isfinite(ratio) && ratio > 0; }

// Keeps the opposite corner fixed, or the centre on the secondary axis for edge drags.
inline QRectF constrain_aspect(QRectF rect, double ratio, Qt::Edges moving) {
    rect = rect.normalized();
    if (!valid_aspect(ratio)) return rect;
    const bool horizontal = moving.testFlag(Qt::LeftEdge) || moving.testFlag(Qt::RightEdge);
    const bool vertical = moving.testFlag(Qt::TopEdge) || moving.testFlag(Qt::BottomEdge);
    if (horizontal) {
        const double height = std::max(.01, rect.width()) / ratio;
        if (vertical) {
            if (moving.testFlag(Qt::TopEdge)) rect.setTop(rect.bottom() - height);
            else rect.setBottom(rect.top() + height);
        } else {
            const double cy = rect.center().y();
            rect.setTop(cy - height / 2); rect.setBottom(cy + height / 2);
        }
    } else if (vertical) {
        const double width = std::max(.01, rect.height()) * ratio;
        const double cx = rect.center().x();
        rect.setLeft(cx - width / 2); rect.setRight(cx + width / 2);
    }
    return rect;
}

inline QRectF aspect_drag_rect(const QPointF &start, const QPointF &end, double ratio) {
    if (!valid_aspect(ratio)) return QRectF(start, end).normalized();
    const auto delta = end - start;
    const double width = std::max(std::abs(delta.x()), std::abs(delta.y()) * ratio);
    const double height = width / ratio;
    return QRectF(start, start + QPointF(delta.x() < 0 ? -width : width, delta.y() < 0 ? -height : height)).normalized();
}
}
