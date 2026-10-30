#include "ui/selection_info/panel.h"
#include "core/i18n.h"
#include "core/resource_manager.h"
#include "core/theme.h"
#include "core/ui_scale.h"
#include "core/ui_theme.h"
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QPainter>
#include <QSpacerItem>

namespace jietuba {
namespace {
QString panel_tr(const char *text) { return i18n_tr(text, "SelectionInfoPanel"); }
const QColor BG(245, 251, 247, 225);
const QColor BORDER_COLOR(125, 161, 139, 120);
}

InfoPanelView InfoPanelView::of(QGraphicsView *view) {
    if (!view) return {};
    return {[view] { return static_cast<QWidget *>(view->viewport()); }, [view](const QPointF &point) { return view->mapFromScene(point); }};
}

SelectionInfoPanel::SelectionInfoPanel(QWidget *parent, QGraphicsView *view) : SelectionInfoPanel(parent, InfoPanelView::of(view)) {}

SelectionInfoPanel::SelectionInfoPanel(QWidget *parent, InfoPanelView view) : QWidget(parent), view_(std::move(view)) {
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    init_ui();
    hide();
    // Connection ends with this widget.
    connect(&get_ui_scale(), &UIScaleManager::scale_changed, this, &SelectionInfoPanel::apply_scale);
}

void SelectionInfoPanel::init_ui() {
    row_layout_ = new QHBoxLayout(this);
    info_label_ = new QLabel;
    row_layout_->addWidget(info_label_);
    // Kept so the gap is rescaled with the rest.
    label_spacer_ = new QSpacerItem(scaled(5), 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    row_layout_->addItem(label_spacer_);
    // Order: rounded corners, aspect ratio, border/shadow.
    btn_rounded = make_btn(QString::fromUtf8("圆角.svg"), true, "Rounded Corners");
    btn_lock = make_btn(QString::fromUtf8("保持纵横比.svg"), true, "Lock Aspect Ratio");
    btn_border = make_btn(QString::fromUtf8("阴影描边.svg"), true, "Border / Shadow");
    for (auto *button : {btn_rounded, btn_lock, btn_border}) row_layout_->addWidget(button);
    sep_ = new QLabel;
    set_own_style(sep_, "background: rgba(255,255,255,30);");
    row_layout_->addWidget(sep_);
    // Ratio edits are a draft until Apply; unchecked means free selection.
    ratio_width = new QDoubleSpinBox(this); ratio_height = new QDoubleSpinBox(this);
    ratio_width->setObjectName("capture_ratio_width"); ratio_height->setObjectName("capture_ratio_height");
    for (auto *spin : {ratio_width, ratio_height}) {
        spin->setRange(.01, 10000); spin->setDecimals(2); spin->setValue(1);
        spin->setFixedWidth(scaled(65)); spin->setKeyboardTracking(false);
        spin->setStyleSheet("QDoubleSpinBox { color:#263b34; background:#eff7f1; border:0; border-radius:4px; }");
    }
    ratio_width->setToolTip(QString::fromUtf8("截图比例：宽，点击应用后生效并记住"));
    ratio_height->setToolTip(QString::fromUtf8("截图比例：高，点击应用后生效并记住"));
    auto *colon = new QLabel(":", this);
    row_layout_->addWidget(ratio_width); row_layout_->addWidget(colon); row_layout_->addWidget(ratio_height);
    btn_lock->setObjectName("capture_ratio_mode");
    btn_lock->setToolTip(QString::fromUtf8("点击切换固定比例 / 自由，点击应用后生效"));
    const auto update_mode = [this](bool locked) {
        btn_lock->setText(QString::fromUtf8(locked ? "固定" : "自由"));
        ratio_width->setEnabled(locked);
        ratio_height->setEnabled(locked);
    };
    connect(btn_lock, &QPushButton::toggled, this, update_mode);
    btn_lock->setChecked(true);
    update_mode(true);
    btn_apply_ratio = new QPushButton(QString::fromUtf8("应用"), this);
    btn_apply_ratio->setObjectName("capture_ratio_apply");
    btn_apply_ratio->setCursor(Qt::PointingHandCursor);
    btn_apply_ratio->setFocusPolicy(Qt::NoFocus);
    btn_apply_ratio->setToolTip(QString::fromUtf8("应用截图比例，并记住用于下次截图"));
    row_layout_->addWidget(btn_apply_ratio);
    connect(btn_apply_ratio, &QPushButton::clicked, this, [this] {
        ratio_width->interpretText();
        ratio_height->interpretText();
        emit ratio_requested(btn_lock->isChecked() ? ratio_width->value() : 0., btn_lock->isChecked() ? ratio_height->value() : 0.);
    });
    // Refresh background, rightmost.
    btn_refresh = make_btn(QString::fromUtf8("刷新背景.svg"), false, "Refresh Background");
    connect(btn_refresh, &QPushButton::pressed, this, &SelectionInfoPanel::refresh_pressed);
    connect(btn_refresh, &QPushButton::released, this, &SelectionInfoPanel::refresh_released);
    row_layout_->addWidget(btn_refresh);
    connect(btn_border, &QPushButton::toggled, this, &SelectionInfoPanel::border_toggled);
    connect(btn_rounded, &QPushButton::toggled, this, &SelectionInfoPanel::rounded_toggled);
    connect(btn_lock, &QPushButton::toggled, this, &SelectionInfoPanel::lock_ratio_toggled);
    // Hidden until the selection is confirmed.
    action_widgets_ = {btn_rounded, btn_lock, btn_border, sep_, ratio_width, colon, ratio_height, btn_apply_ratio, btn_refresh};
    for (auto *widget : action_widgets_) widget->hide();
    apply_scale();
}

QPushButton *SelectionInfoPanel::make_btn(const QString &svg, bool checkable, const char *tip) {
    auto *button = new QPushButton;
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCheckable(checkable);
    button->setToolTip(panel_tr(tip));
    button->setIcon(ResourceManager::get_icon(ResourceManager::get_icon_path(svg)));
    buttons_.append(button);
    return button;
}

void SelectionInfoPanel::apply_scale() {
    setFixedHeight(scaled(BASE_HEIGHT));
    row_layout_->setContentsMargins(scaled(10), 0, scaled(5), 0);
    row_layout_->setSpacing(scaled(7));
    set_own_style(info_label_, QString("color: #294638; font-size: %1px; background: transparent;").arg(scaled(BASE_FONT)));
    const int btn_sz = scaled(BASE_BTN);
    const int icon_sz = scaled(BASE_ICON);
    const int radius = scaled(4);
    const int padding = scaled(2);
    for (auto *button : buttons_) {
        button->setFixedSize(btn_sz, btn_sz);
        button->setIconSize(QSize(icon_sz, icon_sz));
        button->setStyleSheet(QString(R"(
                QPushButton { background: transparent; border: none;
                               border-radius: %1px; padding: %2px; }
                QPushButton:hover { background: rgba(40,120,82,20); }
                QPushButton:checked { background: rgba(40,120,82,45); }
            )").arg(radius).arg(padding));
    }
    btn_lock->setFixedWidth(scaled(66));
    btn_apply_ratio->setFixedSize(scaled(46), btn_sz);
    btn_apply_ratio->setStyleSheet(QString("QPushButton { color:white; background:#287852; border:0; border-radius:%1px; padding:0; } QPushButton:hover { background:#206643; }").arg(radius));
    for (auto *spin : {ratio_width, ratio_height}) spin->setFixedWidth(scaled(65));
    sep_->setFixedSize(1, scaled(BASE_SEP_HEIGHT));
    label_spacer_->changeSize(scaled(5), 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    row_layout_->invalidate();
    adjustSize();
    update();
    if (isVisible() && view_.viewport && last_rect_) follow_rect(*last_rect_);
}

void SelectionInfoPanel::follow_rect(const QRectF &scene_rect) {
    last_rect_ = scene_rect;
    if (scene_rect.isEmpty()) {
        hide();
        return;
    }
    auto *vp = view_.viewport();
    const int vw = vp->width(), vh = vp->height();
    const int pw = width(), ph = height();
    const int gap = scaled(BASE_MARGIN_ABOVE);
    const auto view_tl = view_.map_from_scene(scene_rect.topLeft());
    const auto view_br = view_.map_from_scene(scene_rect.bottomRight());
    const int sel_left = view_tl.x();
    const int sel_top = view_tl.y();
    const int sel_right = view_br.x();
    // Above, left-aligned, shifted left when it would overflow.
    int x = sel_left;
    int y = sel_top - ph - gap;
    if (x + pw > vw) x = vw - pw;
    x = std::max(0, x);
    if (y >= 0) {
        move(x, y);
        return;
    }
    // Left of the selection, top-aligned.
    x = sel_left - pw - gap;
    y = sel_top;
    if (x >= 0 && y + ph <= vh) {
        move(x, y);
        return;
    }
    // Right of the selection, top-aligned.
    x = sel_right + gap;
    y = sel_top;
    if (x + pw <= vw && y + ph <= vh) {
        move(x, y);
        return;
    }
    // Inside the selection's top-left corner.
    const int inset = scaled(4);
    move(std::max(0, sel_left + inset), sel_top + inset);
}

void SelectionInfoPanel::set_confirmed(bool confirmed) {
    for (auto *widget : action_widgets_) widget->setVisible(confirmed);
    adjustSize();
}

void SelectionInfoPanel::update_info_text(const QRectF &rect) {
    const auto theme_hex = get_theme().theme_color_hex();
    const int x = static_cast<int>(rect.x()), y = static_cast<int>(rect.y());
    const int w = static_cast<int>(rect.width()), h = static_cast<int>(rect.height());
    info_label_->setText(QString::fromUtf8("<span style='color:%1'>%2,%3</span>&nbsp;&nbsp;%4 × %5 px").arg(theme_hex).arg(x).arg(y).arg(w).arg(h));
    adjustSize();
}

void SelectionInfoPanel::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(BORDER_COLOR, 1));
    p.setBrush(QBrush(BG));
    // Inset so the outline is not clipped.
    const auto r = rect().adjusted(1, 1, -1, -1);
    const int radius = scaled(BASE_RADIUS);
    p.drawRoundedRect(r, radius, radius);
    p.end();
    QWidget::paintEvent(event);
}
}
