#include "core/aspect_geometry.h"
#include "canvas/items/drawing_items.h"
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
};
QTEST_MAIN(MintCaptureTest)
#include "test_mint_capture.moc"
