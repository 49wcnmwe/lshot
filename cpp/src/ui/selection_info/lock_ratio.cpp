#include "ui/selection_info/lock_ratio.h"
#include "canvas/items/selection_item.h"
#include "core/logger.h"
#include "core/aspect_geometry.h"

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
        installed_ = false;
    }
}
}
