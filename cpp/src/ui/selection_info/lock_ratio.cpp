#include "ui/selection_info/lock_ratio.h"
#include "canvas/items/selection_item.h"
#include "core/logger.h"
#include "core/aspect_geometry.h"
#include <QGraphicsScene>

namespace jietuba {
LockRatioLogic::LockRatioLogic(SelectionItem *selection_item) : item_(selection_item) { install_hook(); }

void LockRatioLogic::set_enabled(bool enabled, const std::optional<QRectF> &current_rect) {
    enabled_ = enabled;
    if (enabled && current_rect && current_rect->height() > 0) {
        ratio_ = current_rect->width() / current_rect->height();
        log_debug(T("锁定纵横比: ON  ratio={ratio:.4f}", {{"ratio", *ratio_}}), "LockRatio");
    } else {
        ratio_.reset();
        if (!enabled) log_debug(T("锁定纵横比: OFF"), "LockRatio");
    }
}

void LockRatioLogic::install_hook() {
    item_->creation_constraint = [this](const QPointF &start, const QPointF &end) {
        if (!enabled_ || !ratio_ || !valid_aspect(*ratio_)) return QRectF(start, end).normalized();
        auto rect = aspect_drag_rect(start, end, *ratio_);
        double width = std::max({rect.width(), 8., 8. * *ratio_});
        double height = width / *ratio_;
        if (const auto *scene = item_->scene()) {
            const auto bounds = scene->sceneRect();
            const double available_width = end.x() < start.x() ? start.x() - bounds.left() : bounds.right() - start.x();
            const double available_height = end.y() < start.y() ? start.y() - bounds.top() : bounds.bottom() - start.y();
            const double factor = std::clamp(std::min(available_width / width, available_height / height), 0., 1.);
            width *= factor; height *= factor;
        }
        return QRectF(start, start + QPointF(end.x() < start.x() ? -width : width, end.y() < start.y() ? -height : height)).normalized();
    };
    item_->resize_constraint = [this](const QRectF &rect, const QRectF &base, int handle) {
        if (enabled_ && ratio_ && *ratio_ > 0) return constrain(rect, base, handle);
        return rect;
    };
    installed_ = true;
}

QRectF LockRatioLogic::constrain(const QRectF &rect, const QRectF &base, int handle) const {
    Qt::Edges edges;
    if (handle == SelectionItem::HANDLE_LEFT || handle == SelectionItem::HANDLE_TOP_LEFT || handle == SelectionItem::HANDLE_BOTTOM_LEFT) edges |= Qt::LeftEdge;
    if (handle == SelectionItem::HANDLE_RIGHT || handle == SelectionItem::HANDLE_TOP_RIGHT || handle == SelectionItem::HANDLE_BOTTOM_RIGHT) edges |= Qt::RightEdge;
    if (handle == SelectionItem::HANDLE_TOP || handle == SelectionItem::HANDLE_TOP_LEFT || handle == SelectionItem::HANDLE_TOP_RIGHT) edges |= Qt::TopEdge;
    if (handle == SelectionItem::HANDLE_BOTTOM || handle == SelectionItem::HANDLE_BOTTOM_LEFT || handle == SelectionItem::HANDLE_BOTTOM_RIGHT) edges |= Qt::BottomEdge;
    return constrain_aspect(rect, *ratio_, edges, base, QSizeF(8, 8));

}

void LockRatioLogic::uninstall() {
    if (installed_) {
        item_->resize_constraint = nullptr;
        item_->creation_constraint = nullptr;
        installed_ = false;
    }
}
}
