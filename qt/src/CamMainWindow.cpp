#include "camclaw/qt/CamMainWindow.h"
#include "camclaw/qt/AgentReviewDialog.h"
#include "camclaw/qt/OperationEditDialog.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QHeaderView>
#include <QMenu>
#include <QStyle>
#include <QStringList>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <sstream>

namespace camclaw {

namespace {

std::string toStd(const QString& value)
{
    return value.toUtf8().constData();
}

QString fromStd(const std::string& value)
{
    return QString::fromUtf8(value.c_str());
}

void styleOperationButton(QPushButton* button, const QIcon& icon)
{
    button->setIcon(icon);
    button->setIconSize(QSize(22, 22));
    button->setMinimumHeight(34);
    button->setStyleSheet(
        "QPushButton {"
        "  padding: 6px 10px;"
        "  border: 1px solid #b9c2cf;"
        "  border-radius: 4px;"
        "  background: #f7f9fb;"
        "  color: #1d2733;"
        "}"
        "QPushButton:hover {"
        "  background: #eaf2fb;"
        "  border-color: #7fa8d8;"
        "}"
        "QPushButton:pressed {"
        "  background: #dcecff;"
        "}");
}

} // namespace

CamMainWindow::CamMainWindow(QWidget* parent)
    : QMainWindow(parent),
      browser_console_(repository_, this),
      human_in_loop_service_(),
      semantic_executor_(browser_console_, human_in_loop_service_),
      draft_service_(QUrl("http://127.0.0.1:8765/v1/agent/plan")),
      active_draft_service_(&draft_service_),
      browser_tree_(0),
      browser_tree_model_(0),
      viewport_(0),
      agent_create_button_(0),
      direct_create_button_(0),
      create_finishing_button_(0),
      create_drilling_button_(0)
{
    setObjectName("camMainWindow");
    setWindowTitle(QString::fromUtf8("CAMClaw Qt CAM"));
    seedRepository();
    buildUi();
    refreshBrowserTree();
}

CamMainWindow::CamMainWindow(AgentDraftService& draft_service, QWidget* parent)
    : QMainWindow(parent),
      browser_console_(repository_, this),
      human_in_loop_service_(),
      semantic_executor_(browser_console_, human_in_loop_service_),
      draft_service_(QUrl("http://127.0.0.1:8765/v1/agent/plan")),
      active_draft_service_(&draft_service),
      browser_tree_(0),
      browser_tree_model_(0),
      viewport_(0),
      agent_create_button_(0),
      direct_create_button_(0),
      create_finishing_button_(0),
      create_drilling_button_(0)
{
    setObjectName("camMainWindow");
    setWindowTitle(QString::fromUtf8("CAMClaw Qt CAM"));
    seedRepository();
    buildUi();
    refreshBrowserTree();
}

QTreeWidget* CamMainWindow::browserTree() const
{
    return browser_tree_;
}

QPushButton* CamMainWindow::agentCreateOperationButton() const
{
    return agent_create_button_;
}

QPushButton* CamMainWindow::createRoughingOperationButton() const
{
    return direct_create_button_;
}

QPushButton* CamMainWindow::createFinishingOperationButton() const
{
    return create_finishing_button_;
}

QPushButton* CamMainWindow::createDrillingOperationButton() const
{
    return create_drilling_button_;
}

CamViewport* CamMainWindow::viewport() const
{
    return viewport_;
}

void CamMainWindow::buildUi()
{
    QToolBar* ribbon = new QToolBar(QString::fromUtf8("CAM"), this);
    ribbon->setObjectName("camRibbon");
    ribbon->setMovable(false);
    ribbon->setStyleSheet(
        "QToolBar {"
        "  background: #edf2f7;"
        "  border-bottom: 1px solid #cbd5e1;"
        "  spacing: 6px;"
        "  padding: 6px;"
        "}");
    addToolBar(Qt::TopToolBarArea, ribbon);

    direct_create_button_ = new QPushButton(QString::fromUtf8("型腔铣"), this);
    direct_create_button_->setObjectName("createRoughingOperationButton");
    direct_create_button_->setToolTip(QString::fromUtf8("创建型腔铣工序"));
    create_finishing_button_ = new QPushButton(QString::fromUtf8("平面铣"), this);
    create_finishing_button_->setObjectName("createFinishingOperationButton");
    create_finishing_button_->setToolTip(QString::fromUtf8("创建平面铣工序"));
    create_drilling_button_ = new QPushButton(QString::fromUtf8("钻孔"), this);
    create_drilling_button_->setObjectName("createDrillingOperationButton");
    create_drilling_button_->setToolTip(QString::fromUtf8("创建钻孔工序"));
    agent_create_button_ = new QPushButton(QString::fromUtf8("CAMClaw"), this);
    agent_create_button_->setObjectName("agentCreateOperationButton");
    agent_create_button_->setToolTip(QString::fromUtf8("用自然语言辅助创建、编辑和计算 CAM 任务"));

    styleOperationButton(direct_create_button_, style()->standardIcon(QStyle::SP_DriveHDIcon));
    styleOperationButton(create_finishing_button_, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    styleOperationButton(create_drilling_button_, style()->standardIcon(QStyle::SP_ArrowDown));
    styleOperationButton(agent_create_button_, style()->standardIcon(QStyle::SP_ComputerIcon));

    ribbon->addWidget(direct_create_button_);
    ribbon->addWidget(create_finishing_button_);
    ribbon->addWidget(create_drilling_button_);
    ribbon->addSeparator();
    ribbon->addWidget(agent_create_button_);

    browser_tree_ = new QTreeWidget(this);
    browser_tree_->setObjectName("camBrowserTree");
    browser_tree_->setHeaderLabel(QString::fromUtf8("CAM Browser"));
    browser_tree_->header()->setStretchLastSection(true);
    browser_tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    browser_tree_model_ = new BrowserTree(*browser_tree_, repository_);
    setCentralWidget(new QWidget(this));
    centralWidget()->setStyleSheet("background: #eef2f6;");

    viewport_ = new CamViewport(centralWidget());
    showResultInViewport(QString::fromUtf8("CAM 绘图视窗"), QString::fromUtf8("选择特征后创建或计算工序"));

    QWidget* root = centralWidget();
    QHBoxLayout* layout = new QHBoxLayout(root);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    browser_tree_->setMinimumWidth(300);
    browser_tree_->setMaximumWidth(380);
    layout->addWidget(browser_tree_);
    layout->addWidget(viewport_, 1);

    connect(agent_create_button_, SIGNAL(clicked()), this, SLOT(openAgentCreateOperationDialog()));
    connect(direct_create_button_, SIGNAL(clicked()), this, SLOT(createRoughingOperationDirectly()));
    connect(create_finishing_button_, SIGNAL(clicked()), this, SLOT(createFinishingOperationDirectly()));
    connect(create_drilling_button_, SIGNAL(clicked()), this, SLOT(createDrillingOperationDirectly()));
    connect(browser_tree_, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showBrowserContextMenu(QPoint)));
    connect(browser_tree_, SIGNAL(itemSelectionChanged()), this, SLOT(updateViewportFromSelection()));
    connect(browser_tree_, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(handleBrowserItemClicked(QTreeWidgetItem*,int)));
    connect(browser_tree_, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(openCurrentOperationEditor()));
}

void CamMainWindow::seedRepository()
{
    if (!repository_.exists("feature_001")) {
        repository_.save(CamObject("feature_001", ObjectType::Feature, "型腔 A"));
    }
    if (!repository_.exists("feature_plane_001")) {
        repository_.save(CamObject("feature_plane_001", ObjectType::Feature, "平面 A"));
    }
    if (!repository_.exists("feature_holes_001")) {
        repository_.save(CamObject("feature_holes_001", ObjectType::Feature, "孔组 A"));
    }
}

void CamMainWindow::refreshBrowserTree()
{
    if (browser_tree_model_ != 0) {
        browser_tree_model_->refresh();
        if (!current_toolpath_id_.isEmpty()) {
            browser_tree_model_->setCurrentObject(current_toolpath_id_);
        } else if (!current_operation_id_.isEmpty()) {
            browser_tree_model_->setCurrentObject(current_operation_id_);
        }
    }
}

QString CamMainWindow::selectedObjectId() const
{
    if (browser_tree_model_ == 0) {
        return QString();
    }
    return browser_tree_model_->selectedObjectId();
}

ObjectType CamMainWindow::selectedObjectType() const
{
    if (browser_tree_model_ == 0) {
        return ObjectType::Unknown;
    }
    return browser_tree_model_->selectedObjectType();
}

QString CamMainWindow::objectTypeText(ObjectType type) const
{
    if (type == ObjectType::Feature) {
        return "Feature";
    }
    if (type == ObjectType::Operation) {
        return "Operation";
    }
    if (type == ObjectType::Toolpath) {
        return "Toolpath";
    }
    return "Unknown";
}

void CamMainWindow::openAgentCreateOperationDialog()
{
    QString target_id;
    QString display_name = QString::fromUtf8("未指定");
    if (selectedObjectType() != ObjectType::Unknown) {
        target_id = selectedObjectId();
        const CamObject selected_object = repository_.get(toStd(selectedObjectId()));
        display_name = selected_object.display_name.empty()
            ? selectedObjectId()
            : fromStd(selected_object.display_name);
    }
    AgentReviewDialog dialog(target_id, display_name, *active_draft_service_, semantic_executor_, &human_in_loop_service_, this);
    dialog.exec();
    if (dialog.executionSucceeded()) {
        current_operation_id_ = dialog.createdOperationId();
        current_toolpath_id_ = dialog.createdToolpathId();
        refreshBrowserTree();
        if (!current_operation_id_.isEmpty()) {
            viewport_->setOperationPreview(current_operation_id_);
        }
        if (!current_toolpath_id_.isEmpty()) {
            viewport_->setToolpathPreview(current_toolpath_id_);
        }
        syncVisibleToolpathsToViewport();
    }
}

void CamMainWindow::createRoughingOperationDirectly()
{
    createOperationDirectly("roughing");
}

void CamMainWindow::createFinishingOperationDirectly()
{
    createOperationDirectly("finishing");
}

void CamMainWindow::createDrillingOperationDirectly()
{
    createOperationDirectly("drilling");
}

bool CamMainWindow::createOperationDirectly(const QString& operation_type)
{
    BrowserOperationCreateItem item;
    item.operation_type = toStd(operation_type);
    item.count = 1;
    std::vector<BrowserOperationCreateItem> items;
    items.push_back(item);
    std::string target_object_id;
    if (selectedObjectType() == ObjectType::Feature) {
        target_object_id = toStd(selectedObjectId());
    }

    const BrowserConsoleResult result = browser_console_.createOperations(items, target_object_id, false);
    if (!result.ok) {
        showResultInViewport(QString::fromUtf8("创建工序失败"), fromStd(result.error_code + ": " + result.message));
        return false;
    }

    current_operation_id_ = fromStd(result.primary_object_id);
    current_toolpath_id_.clear();
    return true;
}

void CamMainWindow::showBrowserContextMenu(const QPoint& position)
{
    QTreeWidgetItem* item = browser_tree_->itemAt(position);
    if (item == 0) {
        return;
    }
    browser_tree_->setCurrentItem(item);
    if (selectedObjectType() != ObjectType::Operation) {
        return;
    }

    QMenu menu(this);
    QAction* edit_action = menu.addAction(QString::fromUtf8("编辑工序"));
    QAction* generate_action = menu.addAction(QString::fromUtf8("生成刀轨"));
    QAction* regenerate_action = menu.addAction(QString::fromUtf8("重新计算刀轨"));
    QAction* delete_action = menu.addAction(QString::fromUtf8("删除当前刀轨"));
    connect(edit_action, SIGNAL(triggered()), this, SLOT(openCurrentOperationEditor()));
    connect(generate_action, SIGNAL(triggered()), this, SLOT(generateToolpathForCurrentOperation()));
    connect(regenerate_action, SIGNAL(triggered()), this, SLOT(regenerateToolpathForCurrentOperation()));
    connect(delete_action, SIGNAL(triggered()), this, SLOT(deleteToolpathForCurrentOperation()));
    menu.exec(browser_tree_->viewport()->mapToGlobal(position));
}

void CamMainWindow::generateToolpathForCurrentOperation()
{
    if (selectedObjectType() != ObjectType::Operation) {
        showResultInViewport(QString::fromUtf8("请选择工序"), QString::fromUtf8("生成刀轨只能从工序节点触发"));
        return;
    }

    std::vector<std::string> operation_ids;
    operation_ids.push_back(toStd(selectedObjectId()));
    const BrowserConsoleResult result = browser_console_.generateToolpaths(operation_ids);
    if (!result.ok) {
        showResultInViewport(QString::fromUtf8("刀轨生成失败"), fromStd(result.error_code + ": " + result.message));
        return;
    }

    current_toolpath_id_ = fromStd(result.primary_object_id);
}

void CamMainWindow::regenerateToolpathForCurrentOperation()
{
    if (selectedObjectType() != ObjectType::Operation) {
        showResultInViewport(QString::fromUtf8("请选择工序"), QString::fromUtf8("重新计算刀轨只能从工序节点触发"));
        return;
    }

    std::vector<std::string> operation_ids;
    operation_ids.push_back(toStd(selectedObjectId()));
    const BrowserConsoleResult result = browser_console_.regenerateToolpaths(operation_ids);
    if (!result.ok) {
        showResultInViewport(QString::fromUtf8("刀轨重新计算失败"), fromStd(result.error_code + ": " + result.message));
        return;
    }
    current_toolpath_id_ = fromStd(result.primary_object_id);
}

void CamMainWindow::deleteToolpathForCurrentOperation()
{
    if (selectedObjectType() != ObjectType::Operation) {
        showResultInViewport(QString::fromUtf8("请选择工序"), QString::fromUtf8("删除刀轨只能从工序节点触发"));
        return;
    }

    const QString operation_id = selectedObjectId();
    const BrowserConsoleResult result = browser_console_.deleteToolpathForOperation(toStd(operation_id));
    if (!result.ok) {
        showResultInViewport(QString::fromUtf8("删除刀轨失败"), fromStd(result.error_code + ": " + result.message));
        return;
    }
    current_toolpath_id_.clear();
    current_operation_id_ = operation_id;
}

void CamMainWindow::openCurrentOperationEditor()
{
    if (selectedObjectType() != ObjectType::Operation) {
        return;
    }

    const QString operation_id = selectedObjectId();
    const CamObject operation = repository_.get(toStd(operation_id));
    OperationEditDialog dialog(operation, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    std::map<std::string, std::string> parameters = dialog.editedParameters();
    std::vector<BrowserParameterUpdate> updates;
    for (std::map<std::string, std::string>::const_iterator it = parameters.begin(); it != parameters.end(); ++it) {
        BrowserParameterUpdate update;
        update.parameter = it->first;
        update.value = it->second;
        updates.push_back(update);
    }
    std::vector<std::string> operation_ids;
    operation_ids.push_back(toStd(operation_id));
    const BrowserConsoleResult result = browser_console_.updateOperations(operation_ids, updates, false);
    if (!result.ok) {
        showResultInViewport(QString::fromUtf8("工序保存失败"), fromStd(result.error_code + ": " + result.message));
        return;
    }

    current_operation_id_ = operation_id;
}

void CamMainWindow::updateViewportFromSelection()
{
    const QString object_id = selectedObjectId();
    if (object_id.isEmpty()) {
        return;
    }
    viewport_->setSelection(selectedObjectType(), object_id);
    if (selectedObjectType() == ObjectType::Toolpath) {
        current_toolpath_id_ = object_id;
        syncVisibleToolpathsToViewport();
    }
}

void CamMainWindow::showResultInViewport(const QString& title, const QString& detail)
{
    viewport_->setStatusMessage(title, detail);
}

void CamMainWindow::handleBrowserItemClicked(QTreeWidgetItem* item, int)
{
    if (item == 0 || selectedObjectType() != ObjectType::Toolpath) {
        return;
    }

    const QString toolpath_id = selectedObjectId();
    std::vector<std::string> toolpath_ids;
    toolpath_ids.push_back(toStd(toolpath_id));
    const BrowserConsoleResult result = browser_console_.setToolpathVisibility(toolpath_ids, "toggle");
    if (!result.ok) {
        showResultInViewport(QString::fromUtf8("刀轨显示切换失败"), fromStd(result.error_code + ": " + result.message));
        return;
    }

    const CamObject toolpath = repository_.get(toStd(toolpath_id));
    const std::map<std::string, std::string>::const_iterator visible = toolpath.attributes.find("visible");
    current_toolpath_id_ = visible != toolpath.attributes.end() && visible->second == "true" ? toolpath_id : QString();
}

QVector<ToolpathViewItem> CamMainWindow::visibleToolpaths() const
{
    QVector<ToolpathViewItem> items;
    const std::vector<CamObject> toolpaths = repository_.objectsByType(ObjectType::Toolpath);
    for (std::size_t index = 0u; index < toolpaths.size(); ++index) {
        const std::map<std::string, std::string>::const_iterator visible = toolpaths[index].attributes.find("visible");
        if (visible == toolpaths[index].attributes.end() || visible->second == "true") {
            ToolpathViewItem item;
            item.operation_id = fromStd(toolpaths[index].parent_object_id);
            item.toolpath_id = fromStd(toolpaths[index].object_id);
            items.push_back(item);
        }
    }
    return items;
}

void CamMainWindow::syncVisibleToolpathsToViewport()
{
    viewport_->setVisibleToolpaths(visibleToolpaths());
}

void CamMainWindow::selectObject(const std::string& object_id)
{
    if (browser_tree_model_ != 0) {
        browser_tree_model_->setCurrentObject(fromStd(object_id));
    }
}

void CamMainWindow::showOperationPreview(const std::string& operation_id)
{
    current_operation_id_ = fromStd(operation_id);
    current_toolpath_id_.clear();
    viewport_->setOperationPreview(current_operation_id_);
}

void CamMainWindow::showToolpathPreview(const std::string& toolpath_id)
{
    current_toolpath_id_ = fromStd(toolpath_id);
    viewport_->setToolpathPreview(current_toolpath_id_);
}

void CamMainWindow::syncVisibleToolpaths()
{
    syncVisibleToolpathsToViewport();
}

void CamMainWindow::openOperationEditor(const std::string& operation_id)
{
    if (browser_tree_model_ != 0) {
        browser_tree_model_->setCurrentObject(fromStd(operation_id));
    }
    openCurrentOperationEditor();
}

} // namespace camclaw
