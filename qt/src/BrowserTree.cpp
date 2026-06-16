#include "camclaw/qt/BrowserTree.h"

#include <QBrush>
#include <QFont>
#include <QHeaderView>
#include <QStyle>

namespace camclaw {

namespace {

QString fromStd(const std::string& value)
{
    return QString::fromUtf8(value.c_str());
}

std::string toStd(const QString& value)
{
    return value.toUtf8().constData();
}

} // namespace

BrowserTree::BrowserTree(QTreeWidget& widget, Repository& repository)
    : widget_(widget),
      repository_(repository)
{
    styleWidget();
}

int BrowserTree::objectTypeRole()
{
    return Qt::UserRole + 1;
}

void BrowserTree::refresh()
{
    widget_.clear();
    QTreeWidgetItem* project = addGroupItem(0, QString::fromUtf8("CAMClaw Project"), widget_.style()->standardIcon(QStyle::SP_DirHomeIcon));
    project->setExpanded(true);

    QTreeWidgetItem* geometry = addGroupItem(project, QString::fromUtf8("几何 / 特征"), widget_.style()->standardIcon(QStyle::SP_DirIcon));
    geometry->setExpanded(true);
    const std::vector<CamObject> features = repository_.objectsByType(ObjectType::Feature);
    for (std::size_t index = 0; index < features.size(); ++index) {
        addBusinessItem(geometry, features[index]);
    }

    QTreeWidgetItem* operations = addGroupItem(project, QString::fromUtf8("工序"), widget_.style()->standardIcon(QStyle::SP_FileDialogListView));
    operations->setExpanded(true);
    const std::vector<CamObject> operation_objects = repository_.objectsByType(ObjectType::Operation);
    for (std::size_t index = 0; index < operation_objects.size(); ++index) {
        addBusinessItem(operations, operation_objects[index]);
    }

    QTreeWidgetItem* toolpaths = addGroupItem(project, QString::fromUtf8("刀轨"), widget_.style()->standardIcon(QStyle::SP_DialogApplyButton));
    toolpaths->setExpanded(true);
    const std::vector<CamObject> toolpath_objects = repository_.objectsByType(ObjectType::Toolpath);
    for (std::size_t index = 0; index < toolpath_objects.size(); ++index) {
        addBusinessItem(toolpaths, toolpath_objects[index]);
    }
}

QString BrowserTree::selectedObjectId() const
{
    QTreeWidgetItem* item = widget_.currentItem();
    if (item == 0) {
        return QString();
    }
    return item->data(0, Qt::UserRole).toString();
}

ObjectType BrowserTree::selectedObjectType() const
{
    QTreeWidgetItem* item = widget_.currentItem();
    if (item == 0) {
        return ObjectType::Unknown;
    }
    return static_cast<ObjectType>(item->data(0, objectTypeRole()).toInt());
}

void BrowserTree::setCurrentObject(const QString& object_id)
{
    if (object_id.isEmpty()) {
        return;
    }

    QList<QTreeWidgetItem*> items = widget_.findItems(object_id, Qt::MatchContains | Qt::MatchRecursive);
    if (!items.isEmpty()) {
        widget_.setCurrentItem(items.first());
    }
}

QTreeWidgetItem* BrowserTree::addGroupItem(QTreeWidgetItem* parent, const QString& text, const QIcon& icon)
{
    QTreeWidgetItem* item = parent == 0
        ? new QTreeWidgetItem(&widget_, QStringList(text))
        : new QTreeWidgetItem(parent, QStringList(text));
    item->setIcon(0, icon);
    item->setData(0, objectTypeRole(), static_cast<int>(ObjectType::Unknown));
    item->setForeground(0, QBrush(QColor("#334155")));
    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    item->setSizeHint(0, QSize(240, 30));
    return item;
}

void BrowserTree::addBusinessItem(QTreeWidgetItem* parent, const CamObject& object)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parent, QStringList(itemText(object)));
    item->setIcon(0, iconForObject(object));
    item->setData(0, Qt::UserRole, fromStd(object.object_id));
    item->setData(0, objectTypeRole(), static_cast<int>(object.object_type));
    item->setForeground(0, QBrush(QColor("#1f2937")));
    item->setSizeHint(0, QSize(240, 28));
}

void BrowserTree::styleWidget()
{
    widget_.setHeaderHidden(false);
    widget_.setIndentation(18);
    widget_.setRootIsDecorated(true);
    widget_.setAlternatingRowColors(false);
    widget_.header()->setStretchLastSection(true);
    widget_.setStyleSheet(
        "QTreeWidget {"
        "  background: #f8fafc;"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 4px;"
        "  color: #1f2937;"
        "  outline: 0;"
        "}"
        "QHeaderView::section {"
        "  background: #e7edf4;"
        "  border: 0;"
        "  border-bottom: 1px solid #cbd5e1;"
        "  padding: 7px 8px;"
        "  color: #334155;"
        "  font-weight: 600;"
        "}"
        "QTreeWidget::item {"
        "  padding: 4px 6px;"
        "  border-radius: 3px;"
        "}"
        "QTreeWidget::item:hover {"
        "  background: #eef6ff;"
        "}"
        "QTreeWidget::item:selected {"
        "  background: #d9ebff;"
        "  color: #0f172a;"
        "}");
}

QIcon BrowserTree::iconForObject(const CamObject& object) const
{
    if (object.object_type == ObjectType::Feature) {
        return widget_.style()->standardIcon(QStyle::SP_FileDialogContentsView);
    }
    if (object.object_type == ObjectType::Operation) {
        const std::map<std::string, std::string>::const_iterator type = object.attributes.find("operation_type");
        if (type != object.attributes.end() && type->second == "drilling") {
            return widget_.style()->standardIcon(QStyle::SP_ArrowDown);
        }
        return widget_.style()->standardIcon(QStyle::SP_ArrowRight);
    }
    if (object.object_type == ObjectType::Toolpath) {
        return widget_.style()->standardIcon(QStyle::SP_DialogApplyButton);
    }
    return widget_.style()->standardIcon(QStyle::SP_FileIcon);
}

QString BrowserTree::itemText(const CamObject& object) const
{
    return fromStd(object.display_name) + QString::fromUtf8(" / ") + fromStd(object.object_id);
}

} // namespace camclaw
