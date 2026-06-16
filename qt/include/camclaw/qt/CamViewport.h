#ifndef CAMCLAW_QT_CAM_VIEWPORT_H
#define CAMCLAW_QT_CAM_VIEWPORT_H

#include "camclaw/AgentWorkflowService.h"

#include <QStringList>
#include <QWidget>
#include <QVector>

namespace camclaw {

struct ToolpathViewItem {
    QString operation_id;
    QString toolpath_id;
};

class CamViewport : public QWidget {
    Q_OBJECT

public:
    explicit CamViewport(QWidget* parent = 0);

    QString statusText() const;
    void setSelection(ObjectType object_type, const QString& object_id);
    void setOperationPreview(const QString& operation_id);
    void setToolpathPreview(const QString& toolpath_id);
    void setVisibleToolpaths(const QVector<ToolpathViewItem>& toolpaths);
    void setStatusMessage(const QString& title, const QString& detail);

protected:
    void paintEvent(QPaintEvent* event);

private:
    enum class FeatureViewKind {
        Pocket,
        Plane,
        Holes
    };

    QRectF modelRect() const;
    QRectF cavityRect(const QRectF& stock) const;
    FeatureViewKind currentFeatureKind() const;
    void drawGrid(QPainter& painter);
    void drawFeature(QPainter& painter, const QPointF& origin);
    void drawPocketFeature(QPainter& painter, const QPointF& origin);
    void drawPlaneFeature(QPainter& painter, const QPointF& origin);
    void drawHoleFeature(QPainter& painter, const QPointF& origin);
    QRectF drawStock(QPainter& painter, const QPointF& origin);
    void drawStatus(QPainter& painter, const QRectF& stock);
    void drawToolpath(QPainter& painter, const QRectF& cavity);
    void drawMillingToolpath(QPainter& painter, const QPointF& origin);
    void drawDrillingToolpath(QPainter& painter, const QPointF& origin);

    ObjectType selected_type_;
    QString selected_id_;
    QString operation_id_;
    QString toolpath_id_;
    QVector<ToolpathViewItem> visible_toolpaths_;
    QString title_;
    QString detail_;
};

} // namespace camclaw

#endif // CAMCLAW_QT_CAM_VIEWPORT_H
