#include "camclaw/qt/CamMainWindow.h"
#include "camclaw/qt/AgentReviewDialog.h"
#include "camclaw/qt/AgentDraftService.h"
#include "camclaw/qt/OperationEditDialog.h"

#include <QDialogButtonBox>
#include <QListWidget>
#include <QtTest/QtTest>

class FakeAgentDraftService : public camclaw::AgentDraftService {
public:
    FakeAgentDraftService()
        : call_count(0)
    {
    }

    camclaw::AgentDraftServiceResult createDraft(const camclaw::AgentDraftRequest& request)
    {
        ++call_count;
        last_request = request;
        camclaw::AgentDraftServiceResult result;
        result.ok = true;
        result.draft = camclaw::SemanticPlanDraft(request.trace_id.toStdString());
        if (request.user_request.contains(QString::fromUtf8("隐藏型腔铣"))
            && request.user_request.contains(QString::fromUtf8("显示钻孔"))) {
            camclaw::SemanticIntent intent;
            intent.id = "i1";
            intent.kind = camclaw::SemanticIntentKind::SetToolpathVisibility;
            camclaw::VisibilityIntentAction hide_pocket;
            hide_pocket.visibility = "hide";
            hide_pocket.target.kind = camclaw::SemanticTargetKind::Query;
            hide_pocket.target.object_type = "toolpath";
            hide_pocket.target.scope = "matching";
            hide_pocket.target.filters["operation_type"] = "pocket";
            intent.visibility_actions.push_back(hide_pocket);
            camclaw::VisibilityIntentAction show_drilling;
            show_drilling.visibility = "show";
            show_drilling.target.kind = camclaw::SemanticTargetKind::Query;
            show_drilling.target.object_type = "toolpath";
            show_drilling.target.scope = "matching";
            show_drilling.target.filters["operation_type"] = "drilling";
            intent.visibility_actions.push_back(show_drilling);
            result.draft.addIntent(intent);
            return result;
        }
        if (request.user_request.contains(QString::fromUtf8("隐藏所有刀轨")) || request.user_request.contains(QString::fromUtf8("显示所有刀轨"))) {
            camclaw::SemanticIntent intent;
            intent.id = "i1";
            intent.kind = camclaw::SemanticIntentKind::SetToolpathVisibility;
            camclaw::VisibilityIntentAction action;
            action.visibility = request.user_request.contains(QString::fromUtf8("隐藏")) ? "hide" : "show";
            action.target.kind = camclaw::SemanticTargetKind::Query;
            action.target.object_type = "toolpath";
            action.target.scope = "all";
            intent.visibility_actions.push_back(action);
            result.draft.addIntent(intent);
            return result;
        }
        if (request.user_request.contains(QString::fromUtf8("打开树上的型腔铣工序"))
            || request.user_request.contains(QString::fromUtf8("步进改小"))) {
            camclaw::SemanticIntent edit;
            edit.id = "i1";
            edit.kind = camclaw::SemanticIntentKind::EditOperation;
            edit.target.kind = camclaw::SemanticTargetKind::Query;
            edit.target.object_type = "operation";
            edit.target.scope = "matching";
            edit.target.filters["operation_type"] = "pocket";
            camclaw::ParameterUpdateIntent update;
            update.parameter = "stepover";
            update.value = "0.8";
            edit.updates.push_back(update);
            camclaw::ParameterUpdateIntent tool_update;
            tool_update.parameter = "tool";
            tool_update.expression = "larger";
            edit.updates.push_back(tool_update);
            result.draft.addIntent(edit);
            camclaw::SemanticIntent regenerate;
            regenerate.id = "i2";
            regenerate.kind = camclaw::SemanticIntentKind::RegenerateToolpath;
            regenerate.target.kind = camclaw::SemanticTargetKind::Ref;
            regenerate.target.ref = "previous.primary_object_ids";
            result.draft.addIntent(regenerate);
            return result;
        }
        if (request.user_request.contains(QString::fromUtf8("5个钻孔"))
            && request.user_request.contains(QString::fromUtf8("3个型腔铣"))
            && request.user_request.contains(QString::fromUtf8("4个平面铣"))) {
            camclaw::SemanticIntent intent;
            intent.id = "i1";
            intent.kind = camclaw::SemanticIntentKind::CreateOperations;
            camclaw::OperationCreateItem drilling;
            drilling.operation_type = "drilling";
            drilling.count = 5;
            intent.create_items.push_back(drilling);
            camclaw::OperationCreateItem pocket;
            pocket.operation_type = "pocket";
            pocket.count = 3;
            intent.create_items.push_back(pocket);
            camclaw::OperationCreateItem finishing;
            finishing.operation_type = "finishing";
            finishing.count = 4;
            intent.create_items.push_back(finishing);
            result.draft.addIntent(intent);
            return result;
        }
        if (request.user_request.contains(QString::fromUtf8("20个钻孔"))) {
            camclaw::SemanticIntent intent;
            intent.id = "i1";
            intent.kind = camclaw::SemanticIntentKind::CreateOperations;
            camclaw::OperationCreateItem drilling;
            drilling.operation_type = "drilling";
            drilling.count = 20;
            intent.create_items.push_back(drilling);
            result.draft.addIntent(intent);
            return result;
        }
        std::string operation_type;
        if (request.target_object_id.contains("holes") || request.target_display_name.contains(QString::fromUtf8("孔"))) {
            operation_type = "drilling";
        } else if (request.target_object_id.contains("plane") || request.target_display_name.contains(QString::fromUtf8("平面"))) {
            operation_type = "finishing";
        } else {
            operation_type = "pocket";
        }
        const bool auto_generate = request.user_request.contains(QString::fromUtf8("加工"));
        if (auto_generate && !request.target_object_id.isEmpty()) {
            camclaw::SemanticIntent intent;
            intent.id = "i1";
            intent.kind = camclaw::SemanticIntentKind::MachineFeature;
            intent.target.kind = camclaw::SemanticTargetKind::Objects;
            intent.target.object_ids.push_back(request.target_object_id.toStdString());
            intent.auto_generate_toolpath = true;
            result.draft.addIntent(intent);
        } else {
            camclaw::SemanticIntent intent;
            intent.id = "i1";
            intent.kind = camclaw::SemanticIntentKind::CreateOperations;
            camclaw::OperationCreateItem item;
            item.operation_type = operation_type;
            item.count = request.user_request.contains("10") ? 10 : 1;
            intent.create_items.push_back(item);
            result.draft.addIntent(intent);
        }
        return result;
    }

    int call_count;
    camclaw::AgentDraftRequest last_request;
};

class FailingAgentDraftService : public camclaw::AgentDraftService {
public:
    camclaw::AgentDraftServiceResult createDraft(const camclaw::AgentDraftRequest& request)
    {
        last_request = request;
        camclaw::AgentDraftServiceResult result;
        result.ok = false;
        result.error_code = "llm_service_unavailable";
        result.message = "LLM service connection failed after retries.";
        return result;
    }

    camclaw::AgentDraftRequest last_request;
};

class CamMainWindowTest : public QObject {
    Q_OBJECT

private slots:
    void agentDialogConfirmCreatesOperationAndToolpath();
    void manualCreateOperationThenContextActionCreatesToolpath();
    void agentDialogRejectAndRegenerateUsesSmallerTool();
    void agentDialogSimulateFailureDoesNotCreateOperation();
    void agentDialogCallsDraftServiceBeforeShowingPlan();
    void manualOperationButtonsCreateDifferentOperations();
    void manualCreateOperationDoesNotRequireSelectedGeometry();
    void selectingDifferentFeaturesUpdatesViewportContext();
    void operationEditorCanUpdateParameterAndRegenerateToolpath();
    void operationContextCanDeleteToolpath();
    void agentDialogShowsFriendlyLlmConnectionFailure();
    void agentDialogUsesSelectedFeatureDisplayName();
    void drillingOperationGeneratesDrillingToolpathPreview();
    void agentPromptMachinesPocketHolesAndPlaneWithMatchingToolpaths();
    void clickingToolpathNodeTogglesVisibility();
    void agentCanHideAndShowAllToolpaths();
    void agentCanBatchCreateOperationsWithoutToolpaths();
    void agentCanCreateMixedOperationBatchesWithoutToolpaths();
    void agentCanMixHideAndShowToolpathsByOperationType();
    void agentDialogShowsHumanInLoopForAmbiguousOperationTarget();
};

static QTreeWidgetItem* find_item(QTreeWidget* tree, const QString& text_part)
{
    QList<QTreeWidgetItem*> items = tree->findItems(text_part, Qt::MatchContains | Qt::MatchRecursive);
    if (items.isEmpty()) {
        return 0;
    }
    return items.first();
}

void CamMainWindowTest::agentDialogConfirmCreatesOperationAndToolpath()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QVERIFY(dialog->draftStepTextForTest().contains(QString::fromUtf8("请输入需求")));
        QPushButton* confirm_before_generate = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm_before_generate != 0);
        QVERIFY(!confirm_before_generate->isEnabled());

        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("给当前型腔创建粗加工工序"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);

        QVERIFY(dialog->draftStepTextForTest().contains("Intent: machine_feature"));
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    QVERIFY(find_item(window.browserTree(), "op_pocket_feature_001") != 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_pocket_feature_001") != 0);
    QVERIFY(window.viewport() != 0);
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_pocket_feature_001"));
}

void CamMainWindowTest::manualCreateOperationThenContextActionCreatesToolpath()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);

    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);

    window.generateToolpathForCurrentOperation();

    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") != 0);
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_roughing_feature_001"));
}

void CamMainWindowTest::agentDialogRejectAndRegenerateUsesSmallerTool()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);

        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("给当前型腔创建粗加工工序"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);

        QPlainTextEdit* reason = dialog->findChild<QPlainTextEdit*>("rejectionReasonEdit");
        QVERIFY(reason != 0);
        reason->setPlainText(QString::fromUtf8("刀具太大，换小刀"));

        QPushButton* reject = dialog->findChild<QPushButton*>("rejectButton");
        QVERIFY(reject != 0);
        QTest::mouseClick(reject, Qt::LeftButton);
        QCOMPARE(dialog->resultStatusText(), QString::fromUtf8("已拒绝"));

        QPushButton* regenerate = dialog->findChild<QPushButton*>("regenerateButton");
        QVERIFY(regenerate != 0);
        QTest::mouseClick(regenerate, Qt::LeftButton);
        QVERIFY(dialog->draftStepTextForTest().contains("Intent: machine_feature"));

        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    QVERIFY(find_item(window.browserTree(), "op_pocket_feature_001") != 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_pocket_feature_001") != 0);
}

void CamMainWindowTest::agentDialogSimulateFailureDoesNotCreateOperation()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("给当前型腔创建粗加工工序"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);

        QPushButton* simulate_failure = dialog->findChild<QPushButton*>("simulateFailureButton");
        QVERIFY(simulate_failure != 0);
        QTest::mouseClick(simulate_failure, Qt::LeftButton);
        QCOMPARE(dialog->resultStatusText(), QString::fromUtf8("执行失败"));
        QVERIFY(dialog->resultBodyText().contains("invalid_argument"));
        dialog->accept();
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    QVERIFY(find_item(window.browserTree(), "op_roughing_feature_001") == 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") == 0);
}

void CamMainWindowTest::agentDialogCallsDraftServiceBeforeShowingPlan()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::SemanticIntentExecutor executor(browser_console, human_in_loop);
    FakeAgentDraftService draft_service;

    camclaw::AgentReviewDialog dialog(
        "feature_001",
        QString::fromUtf8("型腔 A"),
        draft_service,
        executor);

    QVERIFY(dialog.draftStepTextForTest().contains(QString::fromUtf8("请输入需求")));
    QCOMPARE(draft_service.call_count, 0);

    QLineEdit* prompt = dialog.findChild<QLineEdit*>("agentPromptEdit");
    QVERIFY(prompt != 0);
    prompt->setText(QString::fromUtf8("给当前型腔加工"));
    QPushButton* generate = dialog.findChild<QPushButton*>("generateDraftButton");
    QVERIFY(generate != 0);
    QTest::mouseClick(generate, Qt::LeftButton);

    QCOMPARE(draft_service.call_count, 1);
    QCOMPARE(draft_service.last_request.user_request, QString::fromUtf8("给当前型腔加工"));
    QVERIFY(dialog.draftStepTextForTest().contains("Intent: machine_feature"));
}

void CamMainWindowTest::manualOperationButtonsCreateDifferentOperations()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);

    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);
    QVERIFY(find_item(window.browserTree(), QString::fromUtf8("型腔铣")) != 0);
    QVERIFY(find_item(window.browserTree(), "op_roughing_feature_001") != 0);

    QTest::mouseClick(window.createFinishingOperationButton(), Qt::LeftButton);
    QVERIFY(find_item(window.browserTree(), QString::fromUtf8("平面铣")) != 0);
    QVERIFY(find_item(window.browserTree(), "op_finishing") != 0);

    QTest::mouseClick(window.createDrillingOperationButton(), Qt::LeftButton);
    QVERIFY(find_item(window.browserTree(), QString::fromUtf8("钻孔")) != 0);
    QVERIFY(find_item(window.browserTree(), "op_drilling") != 0);
}

void CamMainWindowTest::manualCreateOperationDoesNotRequireSelectedGeometry()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.browserTree()->clearSelection();
    window.browserTree()->setCurrentItem(0);

    QTest::mouseClick(window.createFinishingOperationButton(), Qt::LeftButton);

    QVERIFY(find_item(window.browserTree(), QString::fromUtf8("平面铣")) != 0);
    QVERIFY(find_item(window.browserTree(), "op_finishing") != 0);
}

void CamMainWindowTest::selectingDifferentFeaturesUpdatesViewportContext()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* plane = find_item(window.browserTree(), "feature_plane_001");
    QVERIFY(plane != 0);
    window.browserTree()->setCurrentItem(plane);
    QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8("平面")));
    QVERIFY(window.viewport()->statusText().contains("feature_plane_001"));

    QTreeWidgetItem* holes = find_item(window.browserTree(), "feature_holes_001");
    QVERIFY(holes != 0);
    window.browserTree()->setCurrentItem(holes);
    QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8("孔组")));
    QVERIFY(window.viewport()->statusText().contains("feature_holes_001"));
}

void CamMainWindowTest::operationEditorCanUpdateParameterAndRegenerateToolpath()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);

    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.generateToolpathForCurrentOperation();
    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") != 0);

    operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);

    QTimer::singleShot(0, [&window]() {
        camclaw::OperationEditDialog* dialog = window.findChild<camclaw::OperationEditDialog*>("operationEditDialog");
        QVERIFY(dialog != 0);
        QLineEdit* stepover = dialog->findChild<QLineEdit*>("operationParam_stepover");
        QVERIFY(stepover != 0);
        stepover->setText("0.8");
        QDialogButtonBox* buttons = dialog->findChild<QDialogButtonBox*>("operationEditButtons");
        QVERIFY(buttons != 0);
        QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
    });

    window.openCurrentOperationEditor();
    QVERIFY(window.viewport()->statusText().contains("op_roughing_feature_001"));

    operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.regenerateToolpathForCurrentOperation();

    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") != 0);
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_roughing_feature_001"));
}

void CamMainWindowTest::operationContextCanDeleteToolpath()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);

    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.generateToolpathForCurrentOperation();
    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") != 0);

    operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.deleteToolpathForCurrentOperation();

    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") == 0);
    QVERIFY(find_item(window.browserTree(), "op_roughing_feature_001") != 0);
}

void CamMainWindowTest::agentDialogShowsFriendlyLlmConnectionFailure()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::SemanticIntentExecutor executor(browser_console, human_in_loop);
    FailingAgentDraftService draft_service;

    camclaw::AgentReviewDialog dialog(
        "feature_001",
        QString::fromUtf8("型腔 A"),
        draft_service,
        executor);

    QLineEdit* prompt = dialog.findChild<QLineEdit*>("agentPromptEdit");
    QVERIFY(prompt != 0);
    prompt->setText(QString::fromUtf8("帮我给这个零件生成刀轨"));
    QPushButton* generate = dialog.findChild<QPushButton*>("generateDraftButton");
    QVERIFY(generate != 0);
    QTest::mouseClick(generate, Qt::LeftButton);

    QCOMPARE(dialog.resultStatusText(), QString::fromUtf8("草案生成失败"));
    QVERIFY(dialog.resultBodyText().contains("llm_service_unavailable"));
    QVERIFY(dialog.resultBodyText().contains(QString::fromUtf8("暂时连不上大模型服务")));
    QVERIFY(dialog.resultBodyText().contains("CAMCLAW_LLM_BASE_URL"));
}

void CamMainWindowTest::agentDialogUsesSelectedFeatureDisplayName()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* holes = find_item(window.browserTree(), "feature_holes_001");
    QVERIFY(holes != 0);
    window.browserTree()->setCurrentItem(holes);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLabel* target = dialog->findChild<QLabel*>("targetLabel");
        QVERIFY(target != 0);
        QVERIFY(target->text().contains(QString::fromUtf8("孔组 A")));
        QVERIFY(!target->text().contains(QString::fromUtf8("型腔 A / feature_holes_001")));
        dialog->accept();
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);
}

void CamMainWindowTest::drillingOperationGeneratesDrillingToolpathPreview()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* holes = find_item(window.browserTree(), "feature_holes_001");
    QVERIFY(holes != 0);
    window.browserTree()->setCurrentItem(holes);

    QTest::mouseClick(window.createDrillingOperationButton(), Qt::LeftButton);
    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_drilling_feature_holes_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.generateToolpathForCurrentOperation();

    QVERIFY(find_item(window.browserTree(), QString::fromUtf8("钻孔刀轨")) != 0);
    QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8("钻孔刀轨已生成")));
    QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8("Z 向下刀路径")));
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_drilling_feature_holes_001"));
}

void CamMainWindowTest::agentPromptMachinesPocketHolesAndPlaneWithMatchingToolpaths()
{
    struct CaseData {
        const char* feature_id;
        const char* operation_id;
        const char* toolpath_id;
        const char* viewport_text;
    };

    const CaseData cases[] = {
        { "feature_001", "op_pocket_feature_001", "toolpath_op_pocket_feature_001", "型腔铣刀轨已生成" },
        { "feature_holes_001", "op_drilling_feature_holes_001", "toolpath_op_drilling_feature_holes_001", "钻孔刀轨已生成" },
        { "feature_plane_001", "op_finishing_feature_plane_001", "toolpath_op_finishing_feature_plane_001", "平面铣刀轨已生成" },
    };

    for (std::size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        FakeAgentDraftService draft_service;
        camclaw::CamMainWindow window(draft_service);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QTreeWidgetItem* feature = find_item(window.browserTree(), cases[index].feature_id);
        QVERIFY(feature != 0);
        window.browserTree()->setCurrentItem(feature);

        QTimer::singleShot(0, [&window]() {
            camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
            QVERIFY(dialog != 0);
            QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
            QVERIFY(prompt != 0);
            prompt->setText(QString::fromUtf8("帮我加工这个零件"));
            QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
            QVERIFY(generate != 0);
            QTest::mouseClick(generate, Qt::LeftButton);
            QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
            QVERIFY(confirm != 0);
            QTest::mouseClick(confirm, Qt::LeftButton);
        });

        QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

        QVERIFY(find_item(window.browserTree(), cases[index].operation_id) != 0);
        QVERIFY(find_item(window.browserTree(), cases[index].toolpath_id) != 0);
        QVERIFY(window.viewport()->statusText().contains(cases[index].toolpath_id));
        QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8(cases[index].viewport_text)));
    }
}

void CamMainWindowTest::clickingToolpathNodeTogglesVisibility()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);
    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.generateToolpathForCurrentOperation();
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_roughing_feature_001"));

    QTreeWidgetItem* toolpath = find_item(window.browserTree(), "toolpath_op_roughing_feature_001");
    QVERIFY(toolpath != 0);
    QRect item_rect = window.browserTree()->visualItemRect(toolpath);
    QTest::mouseClick(window.browserTree()->viewport(), Qt::LeftButton, Qt::NoModifier, item_rect.center());
    QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8("刀轨已隐藏")));

    toolpath = find_item(window.browserTree(), "toolpath_op_roughing_feature_001");
    QVERIFY(toolpath != 0);
    item_rect = window.browserTree()->visualItemRect(toolpath);
    QTest::mouseClick(window.browserTree()->viewport(), Qt::LeftButton, Qt::NoModifier, item_rect.center());
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_roughing_feature_001"));
}

void CamMainWindowTest::agentCanHideAndShowAllToolpaths()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(feature != 0);
    window.browserTree()->setCurrentItem(feature);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);
    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);
    window.generateToolpathForCurrentOperation();
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_roughing_feature_001"));

    QTreeWidgetItem* toolpath = find_item(window.browserTree(), "toolpath_op_roughing_feature_001");
    QVERIFY(toolpath != 0);
    window.browserTree()->setCurrentItem(toolpath);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("帮我隐藏所有刀轨"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });
    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);
    QVERIFY(window.viewport()->statusText().contains(QString::fromUtf8("刀轨已隐藏")));

    toolpath = find_item(window.browserTree(), "toolpath_op_roughing_feature_001");
    QVERIFY(toolpath != 0);
    window.browserTree()->setCurrentItem(toolpath);
    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("帮我显示所有刀轨"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });
    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);
    QVERIFY(window.viewport()->statusText().contains("toolpath_op_roughing_feature_001"));
}

void CamMainWindowTest::agentCanBatchCreateOperationsWithoutToolpaths()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("帮我创建20个钻孔工序"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    for (int index = 1; index <= 20; ++index) {
        const QString suffix = index == 1 ? QString() : QString("_%1").arg(index);
        QVERIFY(find_item(window.browserTree(), QString("op_drilling%1").arg(suffix)) != 0);
    }
    QVERIFY(find_item(window.browserTree(), "toolpath_op_drilling") == 0);
}

void CamMainWindowTest::agentCanCreateMixedOperationBatchesWithoutToolpaths()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("帮我创建5个钻孔工序，3个型腔铣工序，4个平面铣工序"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    QVERIFY(find_item(window.browserTree(), "op_drilling_5") != 0);
    QVERIFY(find_item(window.browserTree(), "op_pocket_3") != 0);
    QVERIFY(find_item(window.browserTree(), "op_finishing_4") != 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_drilling") == 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_pocket") == 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_finishing") == 0);
}

void CamMainWindowTest::agentCanMixHideAndShowToolpathsByOperationType()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QTreeWidgetItem* pocket_feature = find_item(window.browserTree(), "feature_001");
    QVERIFY(pocket_feature != 0);
    window.browserTree()->setCurrentItem(pocket_feature);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);
    QTreeWidgetItem* pocket_operation = find_item(window.browserTree(), "op_roughing_feature_001");
    QVERIFY(pocket_operation != 0);
    window.browserTree()->setCurrentItem(pocket_operation);
    window.generateToolpathForCurrentOperation();

    QTreeWidgetItem* holes = find_item(window.browserTree(), "feature_holes_001");
    QVERIFY(holes != 0);
    window.browserTree()->setCurrentItem(holes);
    QTest::mouseClick(window.createDrillingOperationButton(), Qt::LeftButton);
    QTreeWidgetItem* drilling_operation = find_item(window.browserTree(), "op_drilling_feature_holes_001");
    QVERIFY(drilling_operation != 0);
    window.browserTree()->setCurrentItem(drilling_operation);
    window.generateToolpathForCurrentOperation();
    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing_feature_001") != 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_drilling_feature_holes_001") != 0);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("帮我隐藏型腔铣的刀轨，显示钻孔的刀轨"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);
        QVERIFY(dialog->draftStepTextForTest().contains("Intent: set_toolpath_visibility"));
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);
        QTest::mouseClick(confirm, Qt::LeftButton);
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    const QStringList visible_toolpaths = window.viewport()->visibleToolpathIds();
    QVERIFY(visible_toolpaths.contains("toolpath_op_drilling_feature_holes_001"));
    QVERIFY(!visible_toolpaths.contains("toolpath_op_roughing_feature_001"));
}

void CamMainWindowTest::agentDialogShowsHumanInLoopForAmbiguousOperationTarget()
{
    FakeAgentDraftService draft_service;
    camclaw::CamMainWindow window(draft_service);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.browserTree()->clearSelection();
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);
    QTest::mouseClick(window.createRoughingOperationButton(), Qt::LeftButton);
    QVERIFY(find_item(window.browserTree(), "op_roughing") != 0);
    QVERIFY(find_item(window.browserTree(), "op_roughing_2") != 0);

    QTreeWidgetItem* operation = find_item(window.browserTree(), "op_roughing");
    QVERIFY(operation != 0);
    window.browserTree()->setCurrentItem(operation);

    QTimer::singleShot(0, [&window]() {
        camclaw::AgentReviewDialog* dialog = window.findChild<camclaw::AgentReviewDialog*>("agentReviewDialog");
        QVERIFY(dialog != 0);
        QLineEdit* prompt = dialog->findChild<QLineEdit*>("agentPromptEdit");
        QVERIFY(prompt != 0);
        prompt->setText(QString::fromUtf8("帮我打开树上的型腔铣工序，把步进改小一些，然后重新计算"));
        QPushButton* generate = dialog->findChild<QPushButton*>("generateDraftButton");
        QVERIFY(generate != 0);
        QTest::mouseClick(generate, Qt::LeftButton);
        QPushButton* confirm = dialog->findChild<QPushButton*>("confirmButton");
        QVERIFY(confirm != 0);

        QTest::mouseClick(confirm, Qt::LeftButton);
    });

    QTest::mouseClick(window.agentCreateOperationButton(), Qt::LeftButton);

    QVERIFY(find_item(window.browserTree(), "op_roughing") != 0);
    QVERIFY(find_item(window.browserTree(), "toolpath_op_roughing") != 0);
}

QTEST_MAIN(CamMainWindowTest)

#include "test_cam_main_window.moc"
