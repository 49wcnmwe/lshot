#include "core/aspect_geometry.h"
#include "canvas/items/drawing_items.h"
#include "canvas/items/arrow_item.h"
#include "canvas/items/selection_item.h"
#include "canvas/items/stroke_item.h"
#include "canvas/scene.h"
#include "canvas/selection_model.h"
#include "canvas/view.h"
#include "canvas/smart_edit_controller.h"
#include "tools/line.h"
#include "tools/text.h"
#include "ui/selection_info/lock_ratio.h"
#include "canvas/handle_editor.h"
#include "canvas/undo.h"
#include "core/save.h"
#include "ui/shape_settings_panel.h"
#include "ui/selection_info/rounded_corners.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>
#include <QTemporaryDir>

using namespace jietuba;
class MintCaptureTest : public QObject {
    Q_OBJECT
private slots:
    void locked_geometry() {
        for (double ratio : {4. / 3., 2., .25}) {
            for (Qt::Edges edges : {Qt::Edges(Qt::TopEdge | Qt::LeftEdge), Qt::Edges(Qt::BottomEdge | Qt::RightEdge), Qt::Edges(Qt::TopEdge), Qt::Edges(Qt::LeftEdge)}) {
                const auto rect = constrain_aspect(QRectF(-100, -40, 180, 90), ratio, edges);
                QVERIFY(std::abs(rect.width() / rect.height() - ratio) < .0001);
            }
        }
    }
    void locked_corner_tracks_vertical_drags_and_minimum() {
        const QRectF base(20,30,160,80);
        const auto grown = constrain_aspect(QRectF(20,30,160,150),2,Qt::BottomEdge|Qt::RightEdge,base);
        QCOMPARE(grown, QRectF(20,30,300,150));
        QCOMPARE(constrain_aspect(QRectF(20,30,-40,-20),2,Qt::BottomEdge|Qt::RightEdge,base),QRectF(-20,10,40,20));
        SelectionModel model;
        SelectionItem selection(&model);
        LockRatioLogic lock(&selection);
        lock.set_enabled(true,base);
        auto tiny = selection.resize_constraint(QRectF(20,30,2,1),base,SelectionItem::HANDLE_BOTTOM_RIGHT);
        model.set_rect(tiny);
        QCOMPARE(model.rect().size(),QSizeF(16,8));
        lock.uninstall();
    }
    void straight_line_and_dot() {
        CanvasScene scene(QImage(),QRectF(0,0,400,300));
        ToolContext ctx;
        ctx.scene = &scene; ctx.undo_stack = scene.undo_stack;
        ctx.color = Qt::green; ctx.stroke_width = 3;
        LineTool line;
        QVERIFY(line.on_press(QPointF(10,20),Qt::LeftButton,ctx));
        line.on_move(QPointF(80,90),ctx);
        line.on_release(QPointF(100,120),ctx);
        StrokeItem *result = nullptr;
        for (auto *item : scene.items()) if (auto *stroke = dynamic_cast<StrokeItem *>(item)) result = stroke;
        QVERIFY(result);
        QCOMPARE(result->path().elementCount(),2);
        QCOMPARE(result->path().currentPosition(),QPointF(100,120));
        QCOMPARE(result->data(Qt::UserRole+351).toString(),QString("line"));
        ctx.undo_stack->undo();
        QVERIFY(!result->scene());
        ctx.undo_stack->redo();
        QVERIFY(result->scene());
        EllipseItem dot(QRectF(0,0,50,50),QPen(Qt::green,3));
        dot.filled_dot = true;
        QVERIFY(dot.shape().contains(QPointF(25,25)));
        ctx.opacity = .5;
        TextTool text;
        QVERIFY(text.on_press(QPointF(40,40),Qt::LeftButton,ctx));
        auto *text_item = dynamic_cast<TextItem *>(scene.focusItem());
        if (!text_item) for (auto *item : scene.items()) if (auto *candidate = dynamic_cast<TextItem *>(item)) text_item = candidate;
        QVERIFY(text_item);
        QCOMPARE(text_item->opacity(),.5);
    }
    void curved_dashes_follow_shaft() {
        for (bool double_bend : {false,true}) {
            ArrowItem arrow(QPointF(20,150),QPointF(380,150),QPen(Qt::green,4),"line");
            arrow.set_control_point(QPointF(200,50));
            if (double_bend) arrow.set_bend_points(std::array<QPointF,2>{QPointF(110,100),QPointF(290,105)});
            const auto solid = arrow.path();
            arrow.line_style = "dashed"; arrow.update_geometry();
            int solid_pixels = 0, dashed_pixels = 0;
            for (int x=80;x<320;++x) for (int y=0;y<200;++y) {
                const QPointF p(x+.5,y+.5);
                solid_pixels += solid.contains(p);
                dashed_pixels += arrow.path().contains(p);
            }
            QVERIFY(solid_pixels > 100);
            QVERIFY(dashed_pixels > solid_pixels*.2);
            QVERIFY(dashed_pixels < solid_pixels*.9);
        }
    }
    void radius_and_ratio_editors() {
        ShapeSettingsPanel panel;
        panel.set_tool("rect");
        auto *radius = panel.findChild<QDoubleSpinBox *>("rect_corner_radius");
        auto *width = panel.findChild<QDoubleSpinBox *>("rect_ratio_width");
        QVERIFY(radius); QVERIFY(width);
        QSignalSpy changed(&panel, &ShapeSettingsPanel::corner_radius_changed);
        radius->setValue(9);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.front().front().toDouble(), 9.);
        QSignalSpy ratio_changed(&panel,&ShapeSettingsPanel::aspect_ratio_changed);
        QMetaObject::invokeMethod(width,"editingFinished",Qt::DirectConnection);
        QCOMPARE(ratio_changed.count(),0);
        panel.set_tool("ellipse");
        QVERIFY(radius->parentWidget()->isHidden());
    }
    void screenshot_radius_accepts_numbers() {
        QWidget owner;
        RoundedSliderPopup popup(&owner);
        auto *input = popup.findChild<QSpinBox *>("screenshot_corner_radius");
        QVERIFY(input);
        QSignalSpy changed(&popup, &RoundedSliderPopup::radius_changed);
        input->setValue(31);
        QCOMPARE(popup.value(), 31);
        QCOMPARE(changed.count(), 1);
    }
    void jpeg_flattens_transparent_corners() {
        QImage source(16, 16, QImage::Format_ARGB32_Premultiplied);
        source.fill(Qt::transparent);
        const auto encoded = SaveService::encode_image(source, "JPEG", 100);
        QVERIFY(!encoded.isEmpty());
        const auto decoded = QImage::fromData(encoded, "JPG");
        QVERIFY(!decoded.isNull());
        QVERIFY(decoded.pixelColor(0,0).red() > 250);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("rounded.jpeg");
        QVERIFY(SaveService::save_qimage_to_path(source, path, "JPEG", 300, 100));
        QVERIFY(QImage(path).pixelColor(0,0).red() > 250);
    }
    void rectangle_resize_remains_locked() {
        RectItem item(QRectF(20, 30, 160, 80), QPen(Qt::green, 3));
        item.aspect_ratio = 2.;
        LayerEditor editor;
        QVERIFY(editor.start_edit(&item));
        auto handle = editor.handles.front();
        for (const auto &candidate : editor.handles) if (candidate.handle_type == HandleType::CORNER_BR) handle = candidate;
        editor.start_drag(handle, handle.position);
        editor.drag_to(handle.position + QPointF(50, 70));
        QVERIFY(std::abs(item.rect().width() / item.rect().height() - 2.) < .0001);
    }
    void numeric_rectangle_edits_are_undoable() {
        CanvasScene scene(QImage(),QRectF(0,0,400,300));
        CanvasView view(&scene);
        scene.activate_tool("rect");
        auto *item = new RectItem(QRectF(20,30,160,80),QPen(Qt::green,3));
        scene.addItem(item);
        view.smart_edit_controller->select_item(item);
        view.apply_rectangle_parameters(25.5,std::nullopt);
        QCOMPARE(item->get_corner_radius(),25.5);
        view.apply_rectangle_parameters(std::nullopt,4./3.);
        QCOMPARE(item->rect().size(),QSizeF(160,120));
        QCOMPARE(item->aspect_ratio,4./3.);
        scene.undo_stack->undo();
        QCOMPARE(item->rect().size(),QSizeF(160,80));
        QCOMPARE(item->aspect_ratio,0.);
        scene.undo_stack->undo();
        QCOMPARE(item->get_corner_radius(),0.);
        scene.undo_stack->redo();
        QCOMPARE(item->get_corner_radius(),25.5);
        scene.undo_stack->redo();
        QCOMPARE(item->aspect_ratio,4./3.);
    }
};
QTEST_MAIN(MintCaptureTest)
#include "test_mint_capture.moc"
