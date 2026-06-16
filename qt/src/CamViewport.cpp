#include "camclaw/qt/CamViewport.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QLinearGradient>
#include <QPolygonF>
#include <QVector>
#include <QtGlobal>

namespace camclaw {

namespace {

QPointF projectPoint(const QPointF& origin, qreal x, qreal y, qreal z)
{
    return origin + QPointF((x - y) * 2.25, (x + y) * 0.72 - z * 2.8);
}

QPolygonF face4(const QPointF& a, const QPointF& b, const QPointF& c, const QPointF& d)
{
    QPolygonF face;
    face << a << b << c << d;
    return face;
}

uint stableSeed(const QString& value)
{
    uint seed = 2166136261u;
    const QByteArray bytes = value.toUtf8();
    for (int index = 0; index < bytes.size(); ++index) {
        seed ^= static_cast<unsigned char>(bytes[index]);
        seed *= 16777619u;
    }
    return seed;
}

qreal seededRange(uint seed, int shift, qreal min_value, qreal max_value)
{
    const uint bucket = (seed >> shift) & 0xffu;
    const qreal t = static_cast<qreal>(bucket) / 255.0;
    return min_value + (max_value - min_value) * t;
}

} // namespace

CamViewport::CamViewport(QWidget* parent)
    : QWidget(parent),
      selected_type_(ObjectType::Unknown)
{
    setObjectName("mockViewport");
    setMinimumSize(720, 520);
    setAutoFillBackground(false);
    title_ = QString::fromUtf8("CAM 绘图视窗");
    detail_ = QString::fromUtf8("型腔 A / feature_001");
}

QString CamViewport::statusText() const
{
    QString text = title_;
    if (!detail_.isEmpty()) {
        text += "\n" + detail_;
    }
    if (!operation_id_.isEmpty()) {
        text += "\n" + operation_id_;
    }
    if (!toolpath_id_.isEmpty()) {
        text += "\n" + toolpath_id_;
    }
    return text;
}

QStringList CamViewport::visibleToolpathIds() const
{
    QStringList ids;
    for (int index = 0; index < visible_toolpaths_.size(); ++index) {
        ids.push_back(visible_toolpaths_[index].toolpath_id);
    }
    return ids;
}

void CamViewport::setSelection(ObjectType object_type, const QString& object_id)
{
    selected_type_ = object_type;
    selected_id_ = object_id;
    if (object_type == ObjectType::Feature) {
        if (object_id.contains("plane")) {
            title_ = QString::fromUtf8("已选择平面特征");
        } else if (object_id.contains("holes")) {
            title_ = QString::fromUtf8("已选择孔组特征");
        } else {
            title_ = QString::fromUtf8("已选择型腔特征");
        }
        detail_ = object_id;
        operation_id_.clear();
        toolpath_id_.clear();
    } else if (object_type == ObjectType::Operation) {
        if (object_id.contains("drilling")) {
            title_ = QString::fromUtf8("钻孔工序预览");
        } else if (object_id.contains("finishing")) {
            title_ = QString::fromUtf8("平面铣工序预览");
        } else {
            title_ = QString::fromUtf8("型腔铣工序预览");
        }
        detail_ = object_id;
    } else if (object_type == ObjectType::Toolpath) {
        title_ = QString::fromUtf8("刀轨预览");
        detail_ = object_id;
    }
    update();
}

void CamViewport::setOperationPreview(const QString& operation_id)
{
    operation_id_ = operation_id;
    if (operation_id.contains("drilling")) {
        title_ = QString::fromUtf8("钻孔工序已创建");
        detail_ = QString::fromUtf8("孔组将按钻孔参数生成下刀路径");
    } else if (operation_id.contains("finishing")) {
        title_ = QString::fromUtf8("平面铣工序已创建");
        detail_ = QString::fromUtf8("平面区域将按参数生成加工路径");
    } else {
        title_ = QString::fromUtf8("型腔铣工序已创建");
        detail_ = QString::fromUtf8("型腔将按参数生成铣削路径");
    }
    update();
}

void CamViewport::setToolpathPreview(const QString& toolpath_id)
{
    toolpath_id_ = toolpath_id;
    if (toolpath_id.contains("drilling") || operation_id_.contains("drilling")) {
        title_ = QString::fromUtf8("钻孔刀轨已生成");
        detail_ = QString::fromUtf8("显示孔位快速移动和 Z 向下刀路径");
    } else if (toolpath_id.contains("finishing") || operation_id_.contains("finishing")) {
        title_ = QString::fromUtf8("平面铣刀轨已生成");
        detail_ = QString::fromUtf8("显示平面往复走刀线");
    } else {
        title_ = QString::fromUtf8("型腔铣刀轨已生成");
        detail_ = QString::fromUtf8("显示型腔铣削走刀线");
    }
    update();
}

void CamViewport::setVisibleToolpaths(const QVector<ToolpathViewItem>& toolpaths)
{
    visible_toolpaths_ = toolpaths;
    if (visible_toolpaths_.isEmpty()) {
        toolpath_id_.clear();
        title_ = QString::fromUtf8("刀轨已隐藏");
        detail_ = QString::fromUtf8("当前没有可见刀轨");
    } else {
        operation_id_ = visible_toolpaths_.last().operation_id;
        toolpath_id_ = visible_toolpaths_.last().toolpath_id;
        setToolpathPreview(toolpath_id_);
        return;
    }
    update();
}

void CamViewport::setStatusMessage(const QString& title, const QString& detail)
{
    title_ = title;
    detail_ = detail;
    update();
}

void CamViewport::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0.0, QColor(239, 242, 244));
    background.setColorAt(0.62, QColor(190, 194, 196));
    background.setColorAt(1.0, QColor(156, 160, 161));
    painter.fillRect(rect(), background);

    drawGrid(painter);

    const QPointF origin(width() * 0.55, height() * 0.54);
    const QRectF stock_bounds = drawStock(painter, origin);
    drawFeature(painter, origin);

    if (!operation_id_.isEmpty()) {
        painter.setPen(QPen(QColor(222, 153, 45), 3, Qt::DashLine));
        painter.setBrush(QColor(242, 193, 89, 45));
        painter.drawPolygon(face4(projectPoint(origin, -70, -45, 39), projectPoint(origin, 70, -45, 39), projectPoint(origin, 70, 45, 39), projectPoint(origin, -70, 45, 39)));
    }

    if (!visible_toolpaths_.isEmpty()) {
        const QString active_toolpath = toolpath_id_;
        const QString active_operation = operation_id_;
        for (int index = 0; index < visible_toolpaths_.size(); ++index) {
            operation_id_ = visible_toolpaths_[index].operation_id;
            toolpath_id_ = visible_toolpaths_[index].toolpath_id;
            drawToolpath(painter, QRectF(origin.x() - 1, origin.y() - 1, 2, 2));
        }
        operation_id_ = active_operation;
        toolpath_id_ = active_toolpath;
    }

    painter.setPen(QPen(QColor(55, 66, 76), 1));
    painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(stock_bounds.topLeft() + QPointF(-34, -18), QString::fromUtf8("毛坯 120 x 80 x 30"));

    drawStatus(painter, stock_bounds);
}

QRectF CamViewport::modelRect() const
{
    const qreal margin_x = width() * 0.18;
    const qreal margin_y = height() * 0.16;
    return QRectF(margin_x, margin_y, width() - margin_x * 2.0, height() - margin_y * 2.0);
}

QRectF CamViewport::cavityRect(const QRectF& stock) const
{
    return stock.adjusted(stock.width() * 0.18, stock.height() * 0.22, -stock.width() * 0.18, -stock.height() * 0.20);
}

CamViewport::FeatureViewKind CamViewport::currentFeatureKind() const
{
    if (operation_id_.contains("drilling")) {
        return FeatureViewKind::Holes;
    }
    if (operation_id_.contains("finishing")) {
        return FeatureViewKind::Plane;
    }
    if (operation_id_.contains("pocket") || operation_id_.contains("roughing")) {
        return FeatureViewKind::Pocket;
    }
    const QString id = selected_id_.isEmpty() ? detail_ : selected_id_;
    if (id.contains("plane")) {
        return FeatureViewKind::Plane;
    }
    if (id.contains("holes")) {
        return FeatureViewKind::Holes;
    }
    return FeatureViewKind::Pocket;
}

void CamViewport::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(QColor(180, 184, 187, 85), 1));
    const QPointF base(width() * 0.18, height() * 0.80);
    for (int index = -8; index <= 8; ++index) {
        painter.drawLine(base + QPointF(index * 32, 0), base + QPointF(index * 32 + 360, -116));
        painter.drawLine(base + QPointF(index * 32, 0), base + QPointF(index * 32 - 360, -116));
    }

    const QPointF triad(width() * 0.08, height() * 0.82);
    painter.setPen(QPen(Qt::red, 3));
    painter.drawLine(triad, triad + QPointF(62, 22));
    painter.drawText(triad + QPointF(70, 28), "X");
    painter.setPen(QPen(Qt::green, 3));
    painter.drawLine(triad, triad + QPointF(44, -24));
    painter.drawText(triad + QPointF(50, -28), "Y");
    painter.setPen(QPen(Qt::blue, 3));
    painter.drawLine(triad, triad + QPointF(0, -92));
    painter.drawText(triad + QPointF(-6, -104), "Z");
}

QRectF CamViewport::drawStock(QPainter& painter, const QPointF& origin)
{
    const QPointF p000 = projectPoint(origin, -110, -70, 0);
    const QPointF p100 = projectPoint(origin, 110, -70, 0);
    const QPointF p110 = projectPoint(origin, 110, 70, 0);
    const QPointF p010 = projectPoint(origin, -110, 70, 0);
    const QPointF p001 = projectPoint(origin, -110, -70, 34);
    const QPointF p101 = projectPoint(origin, 110, -70, 34);
    const QPointF p111 = projectPoint(origin, 110, 70, 34);
    const QPointF p011 = projectPoint(origin, -110, 70, 34);

    painter.setPen(QPen(QColor(170, 112, 37), 1));
    painter.setBrush(QColor(215, 197, 168, 68));
    painter.drawPolygon(face4(projectPoint(origin, -85, -85, 0), projectPoint(origin, 85, -85, 0), projectPoint(origin, 85, 85, 0), projectPoint(origin, -85, 85, 0)));
    painter.drawPolygon(face4(projectPoint(origin, 0, -105, -18), projectPoint(origin, 0, 105, -18), projectPoint(origin, 0, 105, 74), projectPoint(origin, 0, -105, 74)));
    painter.drawPolygon(face4(projectPoint(origin, -105, 0, -18), projectPoint(origin, 105, 0, -18), projectPoint(origin, 105, 0, 74), projectPoint(origin, -105, 0, 74)));

    painter.setPen(QPen(QColor(78, 85, 91), 2));
    painter.setBrush(QColor(196, 204, 209, 230));
    painter.drawPolygon(face4(p001, p101, p111, p011));
    painter.setBrush(QColor(171, 181, 188, 210));
    painter.drawPolygon(face4(p000, p100, p101, p001));
    painter.setBrush(QColor(151, 162, 170, 210));
    painter.drawPolygon(face4(p100, p110, p111, p101));

    return QRectF(
        qMin(qMin(p000.x(), p100.x()), qMin(p110.x(), p010.x())),
        qMin(qMin(p001.y(), p101.y()), qMin(p111.y(), p011.y())),
        420,
        240);
}

void CamViewport::drawFeature(QPainter& painter, const QPointF& origin)
{
    if (currentFeatureKind() == FeatureViewKind::Plane) {
        drawPlaneFeature(painter, origin);
    } else if (currentFeatureKind() == FeatureViewKind::Holes) {
        drawHoleFeature(painter, origin);
    } else {
        drawPocketFeature(painter, origin);
    }
}

void CamViewport::drawPocketFeature(QPainter& painter, const QPointF& origin)
{
    const QPointF c0 = projectPoint(origin, -62, -38, 36);
    const QPointF c1 = projectPoint(origin, 62, -38, 36);
    const QPointF c2 = projectPoint(origin, 62, 38, 36);
    const QPointF c3 = projectPoint(origin, -62, 38, 36);
    painter.setPen(QPen(QColor(20, 94, 142), selected_type_ == ObjectType::Feature ? 4 : 2));
    painter.setBrush(QColor(24, 128, 184, 24));
    painter.drawPolygon(face4(c0, c1, c2, c3));
    painter.setPen(QPen(QColor(20, 94, 142), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(face4(projectPoint(origin, -55, -32, 36), projectPoint(origin, 55, -32, 36), projectPoint(origin, 55, 32, 36), projectPoint(origin, -55, 32, 36)));
    painter.drawText(c3 + QPointF(-18, 28), QString::fromUtf8("型腔 A / feature_001"));
}

void CamViewport::drawPlaneFeature(QPainter& painter, const QPointF& origin)
{
    const QPointF a = projectPoint(origin, -92, -56, 37);
    const QPointF b = projectPoint(origin, 92, -56, 37);
    const QPointF c = projectPoint(origin, 92, 56, 37);
    const QPointF d = projectPoint(origin, -92, 56, 37);
    painter.setPen(QPen(QColor(38, 117, 70), selected_type_ == ObjectType::Feature ? 4 : 2));
    painter.setBrush(QColor(86, 190, 117, 42));
    painter.drawPolygon(face4(a, b, c, d));
    painter.setPen(QPen(QColor(38, 117, 70), 1, Qt::DashLine));
    for (int index = 0; index < 7; ++index) {
        const qreal y = -48 + index * 16;
        painter.drawLine(projectPoint(origin, -88, y, 40), projectPoint(origin, 88, y, 40));
    }
    painter.drawText(d + QPointF(-16, 30), QString::fromUtf8("平面 A / feature_plane_001"));
}

void CamViewport::drawHoleFeature(QPainter& painter, const QPointF& origin)
{
    painter.setPen(QPen(QColor(112, 73, 165), selected_type_ == ObjectType::Feature ? 3 : 2));
    painter.setBrush(QColor(134, 93, 190, 50));
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            const qreal x = -66 + col * 44;
            const qreal y = -34 + row * 34;
            const QPointF center = projectPoint(origin, x, y, 39);
            painter.drawEllipse(center, 9, 5);
            painter.setPen(QPen(QColor(92, 58, 142), 1));
            painter.drawLine(center + QPointF(0, -5), center + QPointF(0, 18));
            painter.setPen(QPen(QColor(112, 73, 165), selected_type_ == ObjectType::Feature ? 3 : 2));
        }
    }
    painter.drawText(projectPoint(origin, -78, 52, 39) + QPointF(-16, 34), QString::fromUtf8("孔组 A / feature_holes_001"));
}

void CamViewport::drawToolpath(QPainter& painter, const QRectF& cavity)
{
    Q_UNUSED(cavity);
    const QPointF origin(width() * 0.55, height() * 0.54);
    if (currentFeatureKind() == FeatureViewKind::Holes || toolpath_id_.contains("drilling") || operation_id_.contains("drilling")) {
        drawDrillingToolpath(painter, origin);
        return;
    }

    drawMillingToolpath(painter, origin);
}

void CamViewport::drawMillingToolpath(QPainter& painter, const QPointF& origin)
{
    const uint seed = stableSeed(toolpath_id_ + "|" + operation_id_);
    const qreal offset_x = seededRange(seed, 0, -34.0, 34.0);
    const qreal offset_y = seededRange(seed, 8, -22.0, 22.0);
    const qreal span_x = seededRange(seed, 16, 34.0, 68.0);
    const qreal span_y = seededRange(seed, 20, 20.0, 44.0);
    const qreal z = seededRange(seed, 24, 40.0, 52.0);
    const int passes = 6 + static_cast<int>((seed >> 4) % 8u);
    const bool vertical = ((seed >> 12) & 1u) != 0u;
    const QColor path_color = QColor(
        28 + static_cast<int>((seed >> 2) & 0x3fu),
        118 + static_cast<int>((seed >> 9) & 0x5fu),
        70 + static_cast<int>((seed >> 17) & 0x5fu));

    painter.setPen(QPen(path_color, 2));
    for (int index = 0; index < passes; ++index) {
        const qreal t = passes == 1 ? 0.0 : static_cast<qreal>(index) / static_cast<qreal>(passes - 1);
        const qreal lane = -span_y + t * span_y * 2.0;
        const QPointF a = vertical
            ? projectPoint(origin, offset_x + lane, offset_y - span_x, z)
            : projectPoint(origin, offset_x - span_x, offset_y + lane, z);
        const QPointF b = vertical
            ? projectPoint(origin, offset_x + lane, offset_y + span_x, z)
            : projectPoint(origin, offset_x + span_x, offset_y + lane, z);
        painter.drawLine(index % 2 == 0 ? a : b, index % 2 == 0 ? b : a);
    }
    painter.setPen(QPen(path_color.darker(120), 2, Qt::DotLine));
    painter.drawPolygon(face4(
        projectPoint(origin, offset_x - span_x - 4, offset_y - span_y - 4, z),
        projectPoint(origin, offset_x + span_x + 4, offset_y - span_y - 4, z),
        projectPoint(origin, offset_x + span_x + 4, offset_y + span_y + 4, z),
        projectPoint(origin, offset_x - span_x - 4, offset_y + span_y + 4, z)));

    painter.setPen(QPen(path_color.darker(145), 1));
    for (int index = 0; index < 3; ++index) {
        const qreal px = offset_x + seededRange(seed, index * 5, -span_x, span_x);
        const qreal py = offset_y + seededRange(seed, index * 7 + 3, -span_y, span_y);
        painter.drawEllipse(projectPoint(origin, px, py, z + 2), 3.0, 3.0);
    }
}

void CamViewport::drawDrillingToolpath(QPainter& painter, const QPointF& origin)
{
    const uint seed = stableSeed(toolpath_id_ + "|" + operation_id_);
    const QColor rapid_color(236, 142, 38);
    const QColor feed_color(42, 117, 210);
    const qreal safe_z = 64.0;
    const qreal top_z = 43.0;
    const qreal bottom_z = 20.0 - seededRange(seed, 0, 0.0, 8.0);

    QVector<QPointF> hole_points;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            const qreal jitter_x = seededRange(seed, row * 8 + col, -3.0, 3.0);
            const qreal jitter_y = seededRange(seed, row * 7 + col + 2, -2.0, 2.0);
            hole_points << QPointF(-66 + col * 44 + jitter_x, -34 + row * 34 + jitter_y);
        }
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(rapid_color, 2, Qt::DashLine));
    for (int index = 0; index + 1 < hole_points.size(); ++index) {
        const QPointF a = hole_points[index];
        const QPointF b = hole_points[index + 1];
        painter.drawLine(projectPoint(origin, a.x(), a.y(), safe_z), projectPoint(origin, b.x(), b.y(), safe_z));
    }

    for (int index = 0; index < hole_points.size(); ++index) {
        const QPointF p = hole_points[index];
        const QPointF safe = projectPoint(origin, p.x(), p.y(), safe_z);
        const QPointF top = projectPoint(origin, p.x(), p.y(), top_z);
        const QPointF bottom = projectPoint(origin, p.x(), p.y(), bottom_z);

        painter.setPen(QPen(rapid_color, 1, Qt::DashLine));
        painter.drawLine(safe, top);
        painter.setPen(QPen(feed_color, 3));
        painter.drawLine(top, bottom);
        painter.setBrush(QColor(42, 117, 210, 45));
        painter.drawEllipse(top + QPointF(-7, -4), 14, 8);
        painter.setBrush(Qt::NoBrush);
    }

    painter.setPen(QPen(feed_color.darker(125), 1));
    painter.setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    painter.drawText(projectPoint(origin, -82, -48, safe_z) + QPointF(-10, -12), QString::fromUtf8("Drill cycles"));
}

void CamViewport::drawStatus(QPainter& painter, const QRectF& stock)
{
    QRectF panel(24, 22, 280, 92);
    painter.setPen(QPen(QColor(122, 136, 148), 1));
    painter.setBrush(QColor(255, 255, 255, 220));
    painter.drawRoundedRect(panel, 4, 4);

    painter.setPen(QColor(25, 38, 50));
    painter.setFont(QFont("Segoe UI", 11, QFont::DemiBold));
    painter.drawText(panel.adjusted(12, 10, -12, -12), Qt::AlignLeft | Qt::AlignTop, title_);

    painter.setFont(QFont("Segoe UI", 9));
    QString body = detail_;
    if (!operation_id_.isEmpty()) {
        body += "\nOP: " + operation_id_;
    }
    if (!toolpath_id_.isEmpty()) {
        body += "\nTP: " + toolpath_id_;
    }
    painter.drawText(panel.adjusted(12, 36, -12, -8), Qt::AlignLeft | Qt::AlignTop, body);

    painter.setPen(QPen(QColor(60, 71, 82), 1));
    painter.drawLine(stock.topRight() + QPointF(44, 8), stock.topRight() + QPointF(44, 178));
    painter.drawText(stock.topRight() + QPointF(56, 24), QString::fromUtf8("Z+"));
    painter.drawText(stock.topRight() + QPointF(56, 178), QString::fromUtf8("Z-"));
}

} // namespace camclaw
