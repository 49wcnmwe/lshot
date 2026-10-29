#include "ui/shape_settings_panel.h"
#include "core/ui_scale.h"
#include "ui/color_picker_button.h"
#include "ui/paint_settings_panel.h"
#include <QAbstractGraphicsShapeItem>
#include "canvas/items/drawing_items.h"
#include <QSignalBlocker>

namespace jietuba {
ShapeSettingsPanel::ShapeSettingsPanel(QWidget *parent) : BaseSettingsPanel(parent) { init_ui(); }

QString ShapeSettingsPanel::build_stylesheet() const {
    return build_settings_panel_stylesheet(true, "1px", BASE_COMBO_WIDTH, BASE_COMBO_WIDTH, true);
}

void ShapeSettingsPanel::apply_scale_sizes() {
    BaseSettingsPanel::apply_scale_sizes();
    line_style_combo->setIconSize(QSize(scaled(BASE_COMBO_WIDTH), scaled(BASE_COMBO_ICON_H)));
}

void ShapeSettingsPanel::init_ui() {
    current_size = size_default();
    auto *layout = new QHBoxLayout(this);
    line_style_combo = new PillComboBox;
    line_style_combo->set_preview_painter(&paint_line_style_preview);
    add_line_style_items(line_style_combo);
    line_style_combo->setToolTip(tr_panel("Line Style"));
    line_style_combo->setMaxVisibleItems(10);
    layout->addWidget(line_style_combo);
    line_style_combo->hide();
    build_size_and_opacity(layout);
    layout->addWidget(make_separator());
    layout->addWidget(build_color_row());
    rectangle_controls = new QWidget(this);
    auto *extra = new QHBoxLayout(rectangle_controls); extra->setContentsMargins(0,0,0,0);
    extra->addWidget(new QLabel(QString::fromUtf8("圆角")));
    corner_spin = new QDoubleSpinBox(rectangle_controls); corner_spin->setObjectName("rect_corner_radius");
    corner_spin->setRange(0, 10000); corner_spin->setDecimals(1); corner_spin->setSuffix(" px"); corner_spin->setKeyboardTracking(false);
    extra->addWidget(corner_spin);
    ratio_lock = new QCheckBox(QString::fromUtf8("比例"), rectangle_controls); extra->addWidget(ratio_lock);
    ratio_width_spin = new QDoubleSpinBox(rectangle_controls); ratio_height_spin = new QDoubleSpinBox(rectangle_controls);
    ratio_width_spin->setObjectName("rect_ratio_width"); ratio_height_spin->setObjectName("rect_ratio_height");
    for (auto *spin : {ratio_width_spin, ratio_height_spin}) { spin->setRange(.01,10000); spin->setDecimals(2); spin->setValue(1); spin->setKeyboardTracking(false); spin->setFixedWidth(scaled(70)); }
    extra->addWidget(ratio_width_spin); extra->addWidget(new QLabel(":")); extra->addWidget(ratio_height_spin);
    layout->addWidget(rectangle_controls);
    connect(corner_spin, &QDoubleSpinBox::valueChanged, this, &ShapeSettingsPanel::corner_radius_changed);
    const auto emit_ratio = [this] { emit aspect_ratio_changed(ratio_lock->isChecked() ? ratio_width_spin->value()/ratio_height_spin->value() : 0.); };
    connect(ratio_lock, &QCheckBox::toggled, this, [emit_ratio](bool) { emit_ratio(); });
    for (auto *spin : {ratio_width_spin, ratio_height_spin}) connect(spin, &QDoubleSpinBox::editingFinished, this, [this, emit_ratio] {
        const QSignalBlocker block(ratio_lock); ratio_lock->setChecked(true); emit_ratio();
    });
    layout->addStretch();
    connect(line_style_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        current_line_style = line_style_from_combo(line_style_combo);
        emit line_style_changed(current_line_style);
    });
    connect(size_spin, &StepperWidget::valueChanged, this, &ShapeSettingsPanel::on_size_changed);
    connect(opacity_spin, &StepperWidget::valueChanged, this, &ShapeSettingsPanel::on_opacity_changed);
    connect(color_picker_btn, &ColorPickerButton::color_changed, this, &ShapeSettingsPanel::on_color_picked);
    build_extra_controls(layout);
    apply_scale();
}

void ShapeSettingsPanel::set_line_style(const QString &value) {
    if (value != "solid" && value != "dashed" && value != "dashed_dense") return;
    current_line_style = value;
    const int idx = line_style_combo->findData(value);
    if (idx >= 0) {
        line_style_combo->blockSignals(true);
        line_style_combo->setCurrentIndex(idx);
        line_style_combo->blockSignals(false);
    }
}

void ShapeSettingsPanel::set_tool(const QString &tool) {
    rectangle_controls->setVisible(tool == "rect");
    line_style_combo->hide();
    adjustSize();
}
void ShapeSettingsPanel::set_rectangle_state(double radius, double aspect) {
    const QSignalBlocker b1(corner_spin), b2(ratio_width_spin), b3(ratio_height_spin), b4(ratio_lock);
    corner_spin->setValue(radius);
    ratio_lock->setChecked(aspect > 0);
    ratio_width_spin->setValue(aspect >= 1 ? aspect : 1);
    ratio_height_spin->setValue(aspect > 0 && aspect < 1 ? 1 / aspect : 1);
}
void ShapeSettingsPanel::set_state_from_item(QGraphicsItem *item) {
    if (!item) return;
    if (auto *rect = dynamic_cast<RectItem *>(item)) {
        set_tool("rect");
        { const QSignalBlocker block(corner_spin); corner_spin->setMaximum(std::max(0., std::min(rect->rect().width(), rect->rect().height())/2)); }
        set_rectangle_state(rect->get_corner_radius(), rect->aspect_ratio);
    } else set_tool("ellipse");
    blockSignals(true);
    if (auto *shape = dynamic_cast<QAbstractGraphicsShapeItem *>(item)) {
        const auto pen = shape->pen();
        set_line_style(line_style_of_pen(pen));
        size_spin->setValue(static_cast<int>(python_round(pen.widthF())));
        const auto color = pen.color();
        current_color = color;
        color_picker_btn->set_color(color);
        current_opacity = color.alpha();
        opacity_spin->setValue(opacity_to_percent(current_opacity));
    }
    blockSignals(false);
}
}
