#pragma once
#include <QRectF>
#include <Qt>
#include <algorithm>
#include <cmath>

namespace jietuba {
inline bool valid_aspect(double ratio) { return std::isfinite(ratio) && ratio > 0; }

// Keeps the opposite corner fixed, or the centre on the secondary axis for edge drags.
inline QRectF constrain_aspect(QRectF rect, double ratio, Qt::Edges moving, const QRectF &base = {}, const QSizeF &minimum = QSizeF(.01, .01)) {
    rect = rect.normalized();
    if (!valid_aspect(ratio)) return rect;
    const bool horizontal = moving.testFlag(Qt::LeftEdge) || moving.testFlag(Qt::RightEdge);
    const bool vertical = moving.testFlag(Qt::TopEdge) || moving.testFlag(Qt::BottomEdge);
    bool width_leads = horizontal;
    if (horizontal && vertical && base.isValid())
        width_leads = std::abs(rect.width() - base.width()) >= std::abs(rect.height() - base.height()) * ratio;
    const double width = std::max({width_leads ? rect.width() : rect.height() * ratio, minimum.width(), minimum.height() * ratio});
    const double height = width / ratio;
    if (horizontal && vertical) {
        const auto anchor = base.isValid() ? base : rect;
        const double x = moving.testFlag(Qt::LeftEdge) ? anchor.right() - width : anchor.left();
        const double y = moving.testFlag(Qt::TopEdge) ? anchor.bottom() - height : anchor.top();
        return QRectF(x, y, width, height);
    }
    if (horizontal) {
        if (moving.testFlag(Qt::LeftEdge)) rect.setLeft(rect.right() - width);
        else rect.setRight(rect.left() + width);
        const double cy = rect.center().y();
        rect.setTop(cy - height / 2); rect.setBottom(cy + height / 2);
    } else if (vertical) {
        if (moving.testFlag(Qt::TopEdge)) rect.setTop(rect.bottom() - height);
        else rect.setBottom(rect.top() + height);
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
