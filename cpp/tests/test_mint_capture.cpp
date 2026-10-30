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
#include "ui/selection_info/controller.h"
#include "ui/selection_info/panel.h"
#include "ui/mask_overlay.h"
#include "core/export.h"
#include "canvas/handle_editor.h"
#include "canvas/undo.h"
#include "core/save.h"
#include "ui/shape_settings_panel.h"
#include "ui/toolbar_layout.h"
#include "settings/tool_settings.h"
#include <QSettings>
#include "ui/selection_info/rounded_corners.h"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSignalSpy>
#include <QTest>
#include <QUndoStack>
#include <QTemporaryDir>

using namespace jietuba;
namespace {
// A real capture scene without desktop capture or global shortcut registration.
struct CaptureRatioSession {
    QWidget owner;
    CanvasScene scene{QImage(), QRectF(0, 0, 800, 600)};
    CanvasView view{&scene, &owner};
    ExportService exporter{&scene};
    MaskOverlayWidget mask{&owner, scene.selection_model};
    SelectionInfoPanel panel{&owner, &view};
    SelectionInfoController controller;
    explicit CaptureRatioSession(ToolSettingsManager &config)
        : controller(&panel, scene.selection_model, scene.selection_item, &exporter, &mask, &owner, &config) {
        owner.resize(800, 600);
        view.resize(800, 600);
    }
    void select() { scene.preset_selection(QRectF(20, 30, 160, 80)); }
    QRectF resize() const {
        return scene.selection_item->resize_constraint(QRectF(20, 30, 240, 90), scene.selection_model->rect(), SelectionItem::HANDLE_BOTTOM_RIGHT);
    }
};
}
class MintCaptureTest : public QObject {
    Q_OBJECT
private slots:
    void capture_ratio_starts_square_and_applies_explicitly() {
        QTemporaryDir dir;
        QSettings storage(dir.filePath("capture.ini"), QSettings::IniFormat);
        ToolSettingsManager config(storage);
        CaptureRatioSession session(config);
        session.select();
        auto *apply = session.panel.findChild<QPushButton *>("capture_ratio_apply");
        QVERIFY2(apply, "The capture ratio needs an explicit Apply button");
        QVERIFY(session.panel.btn_lock->isChecked());
        QCOMPARE(session.panel.ratio_width->value(), 1.);
        QCOMPARE(session.panel.ratio_height->value(), 1.);
        QCOMPARE(session.resize().size(), QSizeF(240, 240));
        session.panel.ratio_width->setValue(4);
        session.panel.ratio_height->setValue(3);
        QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 160, 80));
        QCOMPARE(session.resize().size(), QSizeF(240, 240));
        apply->click();
        QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 160, 120));
        QCOMPARE(session.resize().size(), QSizeF(240, 180));
    }
    void capture_ratio_free_requires_apply_and_survives_restart() {
        QTemporaryDir dir;
        const auto path = dir.filePath("capture.ini");
        {
            QSettings storage(path, QSettings::IniFormat);
            ToolSettingsManager config(storage);
            CaptureRatioSession session(config);
            session.select();
            auto *apply = session.panel.findChild<QPushButton *>("capture_ratio_apply");
            QVERIFY(apply);
            session.panel.btn_lock->setChecked(false);
            QVERIFY(!session.panel.ratio_width->isEnabled());
            QCOMPARE(session.resize().size(), QSizeF(240, 240));
            apply->click();
            QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 160, 80));
            QCOMPARE(session.resize().size(), QSizeF(240, 90));
        }
        QSettings storage(path, QSettings::IniFormat);
        ToolSettingsManager config(storage);
        CaptureRatioSession next(config);
        next.select();
        QVERIFY(!next.panel.btn_lock->isChecked());
        QCOMPARE(next.resize().size(), QSizeF(240, 90));
    }
    void capture_ratio_remembers_applied_values_only() {
        QTemporaryDir dir;
        const auto path = dir.filePath("capture.ini");
        {
            QSettings storage(path, QSettings::IniFormat);
            ToolSettingsManager config(storage);
            CaptureRatioSession session(config);
            session.select();
            auto *apply = session.panel.findChild<QPushButton *>("capture_ratio_apply");
            QVERIFY(apply);
            session.panel.ratio_width->setValue(16);
            session.panel.ratio_height->setValue(9);
            apply->click();
            session.panel.ratio_width->setValue(7); // Unapplied edits must not leak into the next capture.
            session.panel.btn_lock->setChecked(false);
        }
        QSettings storage(path, QSettings::IniFormat);
        ToolSettingsManager config(storage);
        CaptureRatioSession next(config);
        next.select();
        QVERIFY(next.panel.btn_lock->isChecked());
        QCOMPARE(next.panel.ratio_width->value(), 16.);
        QCOMPARE(next.panel.ratio_height->value(), 9.);
        const auto resized = next.resize();
        QVERIFY(std::abs(resized.width() / resized.height() - 16. / 9.) < .0001);
    }
    void capture_ratio_constrains_initial_drag() {
        QTemporaryDir dir;
        QSettings storage(dir.filePath("capture.ini"), QSettings::IniFormat);
        ToolSettingsManager config(storage);
        CaptureRatioSession session(config);
        session.view.selection_drag->begin(QPointF(20, 30));
        session.view.selection_drag->perform(QPointF(180, 110));
        QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 160, 160));
        session.view.selection_drag->perform(QPointF(780, 590));
        QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 570, 570));
        session.view.selection_drag->end();
        auto *apply = session.panel.findChild<QPushButton *>("capture_ratio_apply");
        QVERIFY(apply);
        session.panel.btn_lock->setChecked(false);
        apply->click();
        session.view.selection_drag->begin(QPointF(20, 30));
        session.view.selection_drag->perform(QPointF(180, 110));
        QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 160, 80));
        session.view.selection_drag->end();
    }
    void capture_ratio_applies_to_click_selection() {
        QTemporaryDir dir;
        QSettings storage(dir.filePath("capture.ini"), QSettings::IniFormat);
        ToolSettingsManager config(storage);
        CaptureRatioSession session(config);
        auto hovered = QRectF(20, 30, 160, 80);
        session.scene.selection_model->set_rect(hovered);
        session.view.selection_drag->begin(QPointF(60, 60));
        session.view.selection_drag->end();
        QCOMPARE(session.scene.selection_model->rect(), QRectF(20, 30, 160, 160));
    }
    void capture_ratio_keeps_minimum_near_screen_edge() {
        QTemporaryDir dir;
        QSettings storage(dir.filePath("capture.ini"), QSettings::IniFormat);
        ToolSettingsManager config(storage);
        CaptureRatioSession session(config);
        auto *apply = session.panel.findChild<QPushButton *>("capture_ratio_apply");
        QVERIFY(apply);
        session.panel.ratio_width->setValue(4);
        session.panel.ratio_height->setValue(3);
        apply->click();
        session.view.selection_drag->begin(QPointF(798, 598));
        session.view.selection_drag->perform(QPointF(820, 620));
        const auto rect = session.scene.selection_model->rect();
        QVERIFY(std::abs(rect.width() / rect.height() - 4. / 3.) < .0001);
        QVERIFY(rect.width() >= 8 && rect.height() >= 8);
        QVERIFY(session.scene.sceneRect().contains(rect));
        session.view.selection_drag->end();
    }
    void capture_ratio_applies_near_screen_edge() {
        QTemporaryDir dir;
        QSettings storage(dir.filePath("capture.ini"), QSettings::IniFormat);
        ToolSettingsManager config(storage);
        config.set_app_setting("screenshot_aspect_locked", false);
        CaptureRatioSession session(config);
        session.scene.preset_selection(QRectF(790, 590, 10, 10));
        auto *apply = session.panel.findChild<QPushButton *>("capture_ratio_apply");
        QVERIFY(apply);
        session.panel.ratio_width->setValue(16);
        session.panel.ratio_height->setValue(9);
        session.panel.btn_lock->setChecked(true);
        apply->click();
        const auto rect = session.scene.selection_model->rect();
        QVERIFY(std::abs(rect.width() / rect.height() - 16. / 9.) < .0001);
        QVERIFY(rect.width() >= 8 && rect.height() >= 8);
        QVERIFY(session.scene.sceneRect().contains(rect));
        QVERIFY(config.get_app_setting("screenshot_aspect_locked").toBool());
        QCOMPARE(config.get_app_setting("screenshot_aspect_width").toDouble(), 16.);
        QCOMPARE(config.get_app_setting("screenshot_aspect_height").toDouble(), 9.);
    }
    void custom_defaults_and_visible_tools() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QSettings storage(dir.filePath("defaults.ini"),QSettings::IniFormat);
        ToolSettingsManager config(storage);
        QCOMPARE(config.get_hotkey(),QString("ctrl+shift+a"));
        QCOMPARE(config.get_screenshot_format(),QString("JPG"));
        QVERIFY(!config.get_screenshot_save_enabled());
        QCOMPARE(config.get_stroke_width("rect"),3.);
        QCOMPARE(toolbar_layout::default_order(),QStringList({"long_screenshot","save","screenshot_translate","text_recognize","scan_code","gif",
            "text","mosaic","rect","ellipse","line","arrow","watermark","pen","highlighter","spotlight","number","eraser","undo","redo","cancel","pin","confirm"}));
        QCOMPARE(toolbar_layout::SCREENSHOT.default_more,QSet<QString>({"screenshot_translate","text_recognize","scan_code","gif","pen","highlighter","spotlight","number","eraser","pin"}));
        QStringList visible;
        for (const auto &value : toolbar_layout::default_layout()) {
            const auto entry=value.toArray();
            if (entry[1].toString()=="show") visible.append(entry[0].toString());
        }
        QCOMPARE(visible,QStringList({"long_screenshot","save","text","mosaic","rect","ellipse","line","arrow","watermark","undo","redo","cancel","confirm"}));
        QVERIFY(toolbar_layout::PIN.order.contains("line"));
    }
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
