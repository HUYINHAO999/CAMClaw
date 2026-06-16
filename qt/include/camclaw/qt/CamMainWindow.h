#ifndef CAMCLAW_QT_CAM_MAIN_WINDOW_H
#define CAMCLAW_QT_CAM_MAIN_WINDOW_H

#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/PlanDraftFactory.h"
#include "camclaw/SkillRuntime.h"
#include "camclaw/qt/AgentDraftService.h"
#include "camclaw/qt/BrowserTree.h"
#include "camclaw/qt/CamViewport.h"

#include <QMainWindow>
#include <QPushButton>
#include <QTreeWidget>

namespace camclaw {

class CamMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit CamMainWindow(QWidget* parent = 0);
    explicit CamMainWindow(AgentDraftService& draft_service, QWidget* parent = 0);

    QTreeWidget* browserTree() const;
    QPushButton* agentCreateOperationButton() const;
    QPushButton* createRoughingOperationButton() const;
    QPushButton* createFinishingOperationButton() const;
    QPushButton* createDrillingOperationButton() const;
    CamViewport* viewport() const;

public slots:
    void openAgentCreateOperationDialog();
    void createRoughingOperationDirectly();
    void createFinishingOperationDirectly();
    void createDrillingOperationDirectly();
    void generateToolpathForCurrentOperation();
    void regenerateToolpathForCurrentOperation();
    void deleteToolpathForCurrentOperation();
    void openCurrentOperationEditor();

private slots:
    void showBrowserContextMenu(const QPoint& position);
    void updateViewportFromSelection();
    void handleBrowserItemClicked(QTreeWidgetItem* item, int column);

private:
    void buildUi();
    void seedRepository();
    void refreshBrowserTree();
    bool createOperationDirectly(const QString& operation_type);
    QString selectedObjectId() const;
    ObjectType selectedObjectType() const;
    QString objectTypeText(ObjectType type) const;
    AgentPlanDraft createDefaultRoughingDraft(const std::string& trace_id, const std::string& target_object_id) const;
    bool executeDraftAndRefresh(AgentPlanDraft draft);
    void showResultInViewport(const QString& title, const QString& detail);
    void syncVisibleToolpathsToViewport();
    QVector<ToolpathViewItem> visibleToolpaths() const;

    Repository repository_;
    BrowserConsole browser_console_;
    ActionGateway gateway_;
    SkillRuntime skill_runtime_;
    AgentPlanExecutor executor_;
    PlanDraftFactory draft_factory_;
    HttpAgentDraftService draft_service_;
    AgentDraftService* active_draft_service_;

    QTreeWidget* browser_tree_;
    BrowserTree* browser_tree_model_;
    CamViewport* viewport_;
    QPushButton* agent_create_button_;
    QPushButton* direct_create_button_;
    QPushButton* create_finishing_button_;
    QPushButton* create_drilling_button_;
    QString current_operation_id_;
    QString current_toolpath_id_;
};

} // namespace camclaw

#endif // CAMCLAW_QT_CAM_MAIN_WINDOW_H
