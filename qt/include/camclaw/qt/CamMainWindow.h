#ifndef CAMCLAW_QT_CAM_MAIN_WINDOW_H
#define CAMCLAW_QT_CAM_MAIN_WINDOW_H

#include "camclaw/AgentWorkflowService.h"
#include "camclaw/BrowserConsole.h"
#include "camclaw/HumanInLoopService.h"
#include "camclaw/SemanticIntentPlan.h"
#include "camclaw/qt/AgentDraftService.h"
#include "camclaw/qt/BrowserTree.h"
#include "camclaw/qt/CamViewport.h"

#include <QMainWindow>
#include <QPushButton>
#include <QTreeWidget>

namespace camclaw {

class CamMainWindow : public QMainWindow, public BrowserConsoleUi {
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
    bool createOperationDirectly(const QString& operation_type);
    QString selectedObjectId() const;
    ObjectType selectedObjectType() const;
    QString objectTypeText(ObjectType type) const;
    void showResultInViewport(const QString& title, const QString& detail);
    void syncVisibleToolpathsToViewport();
    QVector<ToolpathViewItem> visibleToolpaths() const;
    void refreshBrowserTree();
    void selectObject(const std::string& object_id);
    void showOperationPreview(const std::string& operation_id);
    void showToolpathPreview(const std::string& toolpath_id);
    void syncVisibleToolpaths();
    void openOperationEditor(const std::string& operation_id);

    Repository repository_;
    BrowserConsole browser_console_;
    HumanInLoopService human_in_loop_service_;
    SemanticIntentExecutor semantic_executor_;
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
