#include "tools/line.h"
#include "canvas/items/stroke_item.h"
#include "canvas/scene.h"
#include <QLineF>
namespace jietuba {
bool LineTool::on_press(const QPointF &pos, Qt::MouseButton button, ToolContext &ctx) {
    if (button != Qt::LeftButton) return true;
    on_deactivate(ctx); start_ = pos;
    QPen pen(color_with_opacity(ctx.color,ctx.opacity),ctx.stroke_width,Qt::SolidLine,Qt::RoundCap);
    item_ = new StrokeItem(QPainterPath(pos), pen); item_->setData(Qt::UserRole+351,"line"); ctx.scene->addItem(item_); return true;
}
void LineTool::on_move(const QPointF &pos, ToolContext &) {
    if (!item_) return; QPainterPath path(start_); path.lineTo(pos); item_->setPath(path);
}
void LineTool::on_release(const QPointF &pos, ToolContext &ctx) {
    if (!item_) return;
    on_move(pos,ctx); auto *item = item_; item_ = nullptr;
    ctx.scene->removeItem(item);
    if (QLineF(start_,pos).length()<2) {delete item;return;}
    item->finish_path(); ctx.undo_stack->push(new AddItemCommand(ctx.scene,ItemReference(item)));
    emit ctx.scene->item_auto_select_requested(item);
}
void LineTool::on_deactivate(ToolContext &ctx) {
    if (item_) { ctx.scene->removeItem(item_); delete item_; item_=nullptr; }
    Tool::on_deactivate(ctx);
}
}
