#pragma once
#include "tools/base.h"
namespace jietuba {
class StrokeItem;
class LineTool : public Tool {
public:
    QString id() const override { return "line"; }
    bool on_press(const QPointF &, Qt::MouseButton, ToolContext &) override;
    void on_move(const QPointF &, ToolContext &) override;
    void on_release(const QPointF &, ToolContext &) override;
    void on_deactivate(ToolContext &) override;
private:
    QPointF start_; StrokeItem *item_ = nullptr;
};
}
