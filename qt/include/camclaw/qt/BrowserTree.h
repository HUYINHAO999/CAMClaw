#ifndef CAMCLAW_QT_BROWSER_TREE_H
#define CAMCLAW_QT_BROWSER_TREE_H

#include "camclaw/AgentWorkflowService.h"

#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>

namespace camclaw {

class BrowserTree {
public:
    BrowserTree(QTreeWidget& widget, Repository& repository);

    void refresh();
    QString selectedObjectId() const;
    ObjectType selectedObjectType() const;
    void setCurrentObject(const QString& object_id);

    static int objectTypeRole();

private:
    QTreeWidgetItem* addGroupItem(QTreeWidgetItem* parent, const QString& text, const QIcon& icon);
    void addBusinessItem(QTreeWidgetItem* parent, const CamObject& object);
    void styleWidget();
    QIcon iconForObject(const CamObject& object) const;
    QString itemText(const CamObject& object) const;

    QTreeWidget& widget_;
    Repository& repository_;
};

} // namespace camclaw

#endif // CAMCLAW_QT_BROWSER_TREE_H
