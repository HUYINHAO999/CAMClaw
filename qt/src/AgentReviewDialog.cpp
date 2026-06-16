#include "camclaw/qt/AgentReviewDialog.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QApplication>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <map>
#include <sstream>

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

QString userFacingDraftErrorMessage(const QString& error_code, const QString& detail)
{
    if (error_code == "llm_service_unavailable" || error_code == "backend_unavailable") {
        return QString::fromUtf8("CAMClaw 暂时连不上大模型服务。请检查后端是否启动、CAMCLAW_LLM_BASE_URL、网络/TLS/代理配置。\n\n详细信息: ")
            + detail;
    }
    if (error_code == "planner_failed") {
        return QString::fromUtf8("CAMClaw 规划器运行失败。通常是后端异常或上游模型接口返回异常。\n\n详细信息: ")
            + detail;
    }
    return detail;
}

} // namespace

AgentReviewDialog::AgentReviewDialog(
    const QString& target_object_id,
    const QString& target_display_name,
    AgentDraftService& draft_service,
    SemanticIntentExecutor& executor,
    HumanInLoopService* human_in_loop_service,
    QWidget* parent)
    : QDialog(parent),
      target_object_id_(target_object_id),
      target_display_name_(target_display_name),
      draft_service_(draft_service),
      draft_("trace_qt_empty"),
      executor_(executor),
      human_in_loop_service_(human_in_loop_service),
      has_result_(false),
      execution_succeeded_(false),
      created_toolpath_id_(),
      trace_id_label_(0),
      draft_status_label_(0),
      target_label_(0),
      prompt_edit_(0),
      step_label_(0),
      result_status_label_(0),
      result_body_edit_(0),
      trace_edit_(0),
      tabs_(0),
      rejection_reason_edit_(0),
      confirm_button_(0),
      generate_button_(0),
      reject_button_(0),
      simulate_failure_button_(0),
      regenerate_button_(0),
      close_button_(0)
{
    setObjectName("agentReviewDialog");
    setWindowTitle(QString::fromUtf8("CAMClaw 草案审核"));
    buildUi();
    render();
}

QString AgentReviewDialog::parameterValue(const QString& name) const
{
    if (!parameter_edits_.contains(name)) {
        return QString();
    }
    return parameter_edits_[name]->text();
}

QString AgentReviewDialog::resultStatusText() const
{
    return result_status_label_->text();
}

QString AgentReviewDialog::resultBodyText() const
{
    return result_body_edit_->toPlainText();
}

QString AgentReviewDialog::traceText() const
{
    return trace_edit_->toPlainText();
}

QString AgentReviewDialog::draftStepTextForTest() const
{
    return step_label_->text();
}

bool AgentReviewDialog::executionSucceeded() const
{
    return execution_succeeded_;
}

QString AgentReviewDialog::createdOperationId() const
{
    return created_operation_id_;
}

QString AgentReviewDialog::createdToolpathId() const
{
    return created_toolpath_id_;
}

void AgentReviewDialog::buildUi()
{
    setModal(true);
    setMinimumSize(500, 340);
    setStyleSheet(
        "QDialog { background: #f6f8fb; color: #172033; }"
        "QLabel { color: #26354d; }"
        "QGroupBox {"
        "  border: 1px solid #d8dee8;"
        "  border-radius: 6px;"
        "  margin-top: 8px;"
        "  padding: 8px;"
        "  background: #ffffff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 4px;"
        "  color: #344054;"
        "}"
        "QLineEdit, QPlainTextEdit {"
        "  border: 1px solid #cbd5e1;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  background: #ffffff;"
        "  selection-background-color: #b9ddff;"
        "}"
        "QLineEdit:focus, QPlainTextEdit:focus {"
        "  border: 1px solid #0f6cbd;"
        "}"
        "QPushButton {"
        "  min-height: 26px;"
        "  padding: 4px 10px;"
        "  border: 1px solid #b8c3d1;"
        "  border-radius: 4px;"
        "  background: #ffffff;"
        "}"
        "QPushButton:hover { background: #eef6ff; border-color: #80b8ed; }"
        "QPushButton:disabled { color: #98a2b3; background: #f2f4f7; }"
        "QPushButton#confirmButton, QPushButton#generateDraftButton {"
        "  background: #0f6cbd;"
        "  border-color: #0f6cbd;"
        "  color: #ffffff;"
        "}"
        "QPushButton#confirmButton:hover, QPushButton#generateDraftButton:hover {"
        "  background: #0b5cab;"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid #d8dee8;"
        "  border-radius: 6px;"
        "  background: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  padding: 5px 10px;"
        "  margin-right: 3px;"
        "  border: 1px solid #d8dee8;"
        "  border-bottom: 0;"
        "  border-top-left-radius: 5px;"
        "  border-top-right-radius: 5px;"
        "  background: #eef2f7;"
        "}"
        "QTabBar::tab:selected { background: #ffffff; color: #0f6cbd; }");

    trace_id_label_ = new QLabel(this);
    trace_id_label_->setObjectName("traceIdLabel");
    draft_status_label_ = new QLabel(this);
    draft_status_label_->setObjectName("draftStatusLabel");
    target_label_ = new QLabel(QString::fromUtf8("目标对象: ") + target_display_name_ + " / " + target_object_id_, this);
    target_label_->setObjectName("targetLabel");
    target_label_->setWordWrap(true);

    prompt_edit_ = new QLineEdit(this);
    prompt_edit_->setObjectName("agentPromptEdit");
    prompt_edit_->setPlaceholderText(QString::fromUtf8("例如：帮我打开树上的型腔铣工序，把刀具改大一些，再进行计算"));
    generate_button_ = new QPushButton(QString::fromUtf8("生成草案"), this);
    generate_button_->setObjectName("generateDraftButton");

    step_label_ = new QLabel(this);
    step_label_->setObjectName("stepLabel");
    step_label_->setWordWrap(true);
    step_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    QWidget* draft_page = new QWidget(this);
    QVBoxLayout* draft_layout = new QVBoxLayout(draft_page);
    draft_layout->setContentsMargins(8, 8, 8, 8);
    draft_layout->addWidget(step_label_);
    draft_layout->addStretch();

    QWidget* parameter_page = new QWidget(this);
    QGroupBox* parameter_group = new QGroupBox(QString::fromUtf8("参数"), parameter_page);
    QFormLayout* parameter_layout = new QFormLayout(parameter_group);
    parameter_layout->setLabelAlignment(Qt::AlignRight);
    parameter_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    const QString names[] = {
        "operation_type",
        "tool_id",
        "stepover",
        "stepdown",
        "tolerance",
        "visibility",
        "scope",
        "toolpath_ids",
    };
    const QString labels[] = {
        QString::fromUtf8("工序类型"),
        QString::fromUtf8("刀具 ID"),
        QString::fromUtf8("步距"),
        QString::fromUtf8("切深"),
        QString::fromUtf8("容差"),
        QString::fromUtf8("显示动作"),
        QString::fromUtf8("作用范围"),
        QString::fromUtf8("刀轨 ID"),
    };
    for (int index = 0; index < 8; ++index) {
        QLineEdit* edit = new QLineEdit(parameter_group);
        edit->setObjectName("parameter_" + names[index]);
        parameter_edits_[names[index]] = edit;
        parameter_layout->addRow(labels[index], edit);
        connect(edit, SIGNAL(editingFinished()), this, SLOT(editParameter()));
    }
    QVBoxLayout* parameter_page_layout = new QVBoxLayout(parameter_page);
    parameter_page_layout->setContentsMargins(8, 8, 8, 8);
    parameter_page_layout->addWidget(parameter_group);
    parameter_page_layout->addStretch();

    result_status_label_ = new QLabel(this);
    result_status_label_->setObjectName("resultStatusLabel");
    result_body_edit_ = new QPlainTextEdit(this);
    result_body_edit_->setObjectName("resultBodyEdit");
    result_body_edit_->setReadOnly(true);
    result_body_edit_->setMinimumHeight(116);

    QWidget* result_page = new QWidget(this);
    QGroupBox* result_group = new QGroupBox(QString::fromUtf8("结果"), result_page);
    QVBoxLayout* result_layout = new QVBoxLayout(result_group);
    result_layout->addWidget(result_status_label_);
    result_layout->addWidget(result_body_edit_);
    QVBoxLayout* result_page_layout = new QVBoxLayout(result_page);
    result_page_layout->setContentsMargins(8, 8, 8, 8);
    result_page_layout->addWidget(result_group);

    trace_edit_ = new QPlainTextEdit(this);
    trace_edit_->setObjectName("traceEdit");
    trace_edit_->setReadOnly(true);
    trace_edit_->setMinimumHeight(116);
    QWidget* trace_page = new QWidget(this);
    QGroupBox* trace_group = new QGroupBox(QString::fromUtf8("Trace"), trace_page);
    QVBoxLayout* trace_layout = new QVBoxLayout(trace_group);
    trace_layout->addWidget(trace_edit_);
    QVBoxLayout* trace_page_layout = new QVBoxLayout(trace_page);
    trace_page_layout->setContentsMargins(8, 8, 8, 8);
    trace_page_layout->addWidget(trace_group);

    rejection_reason_edit_ = new QPlainTextEdit(this);
    rejection_reason_edit_->setObjectName("rejectionReasonEdit");
    rejection_reason_edit_->setPlaceholderText(QString::fromUtf8("例如：刀具太大，换小刀"));
    rejection_reason_edit_->setMaximumHeight(42);

    confirm_button_ = new QPushButton(QString::fromUtf8("确认执行"), this);
    confirm_button_->setObjectName("confirmButton");
    reject_button_ = new QPushButton(QString::fromUtf8("拒绝草案"), this);
    reject_button_->setObjectName("rejectButton");
    simulate_failure_button_ = new QPushButton(QString::fromUtf8("模拟失败"), this);
    simulate_failure_button_->setObjectName("simulateFailureButton");
    regenerate_button_ = new QPushButton(QString::fromUtf8("重新生成"), this);
    regenerate_button_->setObjectName("regenerateButton");
    close_button_ = new QPushButton(QString::fromUtf8("关闭"), this);
    close_button_->setObjectName("closeButton");

    QHBoxLayout* header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    QLabel* title_label = new QLabel(QString::fromUtf8("CAMClaw"), this);
    QFont title_font = title_label->font();
    title_font.setPointSize(title_font.pointSize() + 1);
    title_font.setBold(true);
    title_label->setFont(title_font);
    header_layout->addWidget(title_label);
    header_layout->addSpacing(12);
    header_layout->addWidget(draft_status_label_);
    header_layout->addStretch();
    header_layout->addWidget(trace_id_label_);

    QHBoxLayout* prompt_layout = new QHBoxLayout();
    prompt_layout->addWidget(new QLabel(QString::fromUtf8("需求"), this));
    prompt_layout->addWidget(prompt_edit_, 1);
    prompt_layout->addWidget(generate_button_);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName("agentReviewTabs");
    tabs_->addTab(draft_page, QString::fromUtf8("草案"));
    tabs_->addTab(parameter_page, QString::fromUtf8("参数"));
    tabs_->addTab(result_page, QString::fromUtf8("结果"));
    tabs_->addTab(trace_page, QString::fromUtf8("Trace"));

    QHBoxLayout* rejection_layout = new QHBoxLayout();
    rejection_layout->addWidget(new QLabel(QString::fromUtf8("拒绝原因"), this));
    rejection_layout->addWidget(rejection_reason_edit_, 1);

    QHBoxLayout* action_layout = new QHBoxLayout();
    action_layout->addWidget(confirm_button_);
    action_layout->addWidget(reject_button_);
    action_layout->addWidget(regenerate_button_);
    action_layout->addStretch();
    action_layout->addWidget(simulate_failure_button_);
    action_layout->addWidget(close_button_);

    QVBoxLayout* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(10, 8, 10, 8);
    root_layout->setSpacing(6);
    root_layout->addLayout(header_layout);
    root_layout->addWidget(target_label_);
    root_layout->addLayout(prompt_layout);
    root_layout->addWidget(tabs_, 1);
    root_layout->addLayout(rejection_layout);
    root_layout->addLayout(action_layout);

    resize(560, 380);

    connect(generate_button_, SIGNAL(clicked()), this, SLOT(generateDraft()));
    connect(confirm_button_, SIGNAL(clicked()), this, SLOT(confirmDraft()));
    connect(reject_button_, SIGNAL(clicked()), this, SLOT(rejectDraft()));
    connect(simulate_failure_button_, SIGNAL(clicked()), this, SLOT(simulateFailure()));
    connect(regenerate_button_, SIGNAL(clicked()), this, SLOT(regenerateDraft()));
    connect(close_button_, SIGNAL(clicked()), this, SLOT(accept()));
}

void AgentReviewDialog::render()
{
    renderDraft();
    renderParameters();
    renderResult();
    renderTrace();
    renderControls();
}

void AgentReviewDialog::renderDraft()
{
    trace_id_label_->setText(QString::fromUtf8("Trace: ") + fromStd(draft_.traceId()));
    draft_status_label_->setText(statusText(draft_.status()));
    if (draft_.intentCount() == 0u) {
        step_label_->setText(QString::fromUtf8("请输入需求并生成草案。"));
        return;
    }
    step_label_->setText(draftStepText());
}

void AgentReviewDialog::renderParameters()
{
    if (draft_.intentCount() == 0u) {
        for (QMap<QString, QLineEdit*>::iterator it = parameter_edits_.begin(); it != parameter_edits_.end(); ++it) {
            it.value()->clear();
            it.value()->setEnabled(false);
        }
        return;
    }
    const SemanticIntent& step = draft_.intentAt(0u);
    for (QMap<QString, QLineEdit*>::iterator it = parameter_edits_.begin(); it != parameter_edits_.end(); ++it) {
        it.value()->blockSignals(true);
        QString value;
        if (it.key() == "operation_type" && step.target.filters.find("operation_type") != step.target.filters.end()) {
            value = fromStd(step.target.filters.find("operation_type")->second);
        } else if (it.key() == "visibility" && !step.visibility_actions.empty()) {
            value = fromStd(step.visibility_actions[0].visibility);
        } else if (it.key() == "scope" && !step.visibility_actions.empty()) {
            value = fromStd(step.visibility_actions[0].target.scope);
        }
        it.value()->setText(value);
        it.value()->setEnabled(false);
        it.value()->blockSignals(false);
    }
}

void AgentReviewDialog::renderResult()
{
    if (!has_result_) {
        result_status_label_->setText(QString::fromUtf8("未执行"));
        result_body_edit_->setPlainText(QString::fromUtf8("先输入需求并生成草案，再确认执行。"));
        return;
    }
    result_status_label_->setText(result_status_);
    result_body_edit_->setPlainText(result_body_);
}

void AgentReviewDialog::renderTrace()
{
    trace_edit_->setPlainText(joinTraceEvents());
}

void AgentReviewDialog::renderControls()
{
    const bool editable = draft_.status() == SemanticDraftStatus::PendingReview;
    const bool has_draft = draft_.intentCount() > 0u;
    confirm_button_->setEnabled(editable && has_draft);
    reject_button_->setEnabled(editable && has_draft);
    simulate_failure_button_->setEnabled(editable && has_draft);
    rejection_reason_edit_->setEnabled(editable && has_draft);
    regenerate_button_->setEnabled(draft_.status() == SemanticDraftStatus::Rejected);
    generate_button_->setEnabled(draft_.status() != SemanticDraftStatus::Confirmed);
}

void AgentReviewDialog::generateDraft()
{
    const QString prompt = prompt_edit_->text().trimmed();
    if (prompt.isEmpty()) {
        setResultMessage(
            QString::fromUtf8("未生成"),
            QString::fromUtf8("{\n  \"ok\": false,\n  \"message\": \"请输入需求后再生成草案。\"\n}"));
        render();
        return;
    }

    generate_button_->setEnabled(false);
    generate_button_->setText(QString::fromUtf8("规划中..."));
    confirm_button_->setEnabled(false);
    reject_button_->setEnabled(false);
    simulate_failure_button_->setEnabled(false);
    setResultMessage(
        QString::fromUtf8("正在规划"),
        QString::fromUtf8("{\n  \"ok\": true,\n  \"message\": \"CAMClaw 正在分析当前几何、工序和刀具候选...\"\n}"));
    result_status_label_->setText(result_status_);
    result_body_edit_->setPlainText(result_body_);
    QApplication::processEvents();

    requestDraftFromBackend(prompt, QString(), QString::fromUtf8("草案已生成"));
}

bool AgentReviewDialog::requestDraftFromBackend(const QString& prompt, const QString& rejection_reason, const QString& success_status)
{
    std::ostringstream trace;
    trace << "trace_qt_agent_" << QDateTime::currentMSecsSinceEpoch();
    AgentDraftRequest request;
    request.trace_id = fromStd(trace.str());
    request.user_request = prompt;
    request.target_object_id = target_object_id_;
    request.target_display_name = target_display_name_;
    request.rejection_reason = rejection_reason;

    const AgentDraftServiceResult service_result = draft_service_.createDraft(request);
    generate_button_->setText(QString::fromUtf8("生成草案"));
    has_result_ = true;
    execution_succeeded_ = false;
    created_operation_id_.clear();
    created_toolpath_id_.clear();
    if (!service_result.ok) {
        const QString user_message = userFacingDraftErrorMessage(service_result.error_code, service_result.message);
        setResultMessage(
            QString::fromUtf8("草案生成失败"),
            QString::fromUtf8("{\n  \"ok\": false,\n  \"error_code\": ")
                + quote(service_result.error_code)
                + QString::fromUtf8(",\n  \"message\": ")
                + quote(user_message)
                + QString::fromUtf8("\n}"));
        render();
        if (tabs_ != 0) {
            tabs_->setCurrentIndex(2);
        }
        return false;
    }

    draft_ = service_result.draft;
    setResultMessage(
        success_status,
        QString::fromUtf8("{\n  \"ok\": true,\n  \"source\": \"python_backend_llm_planner\",\n  \"message\": \"草案已生成，请审核参数。\"\n}"));
    render();
    if (tabs_ != 0) {
        tabs_->setCurrentIndex(0);
    }
    return true;
}

void AgentReviewDialog::confirmDraft()
{
    if (draft_.status() != SemanticDraftStatus::PendingReview) {
        return;
    }
    draft_.confirm();
    const AgentPlanExecutionResult result = executor_.execute(draft_);
    setExecutionResult(result);
    render();
    if (execution_succeeded_) {
        accept();
    }
}

void AgentReviewDialog::rejectDraft()
{
    if (draft_.status() != SemanticDraftStatus::PendingReview) {
        return;
    }
    const QString reason = rejection_reason_edit_->toPlainText().trimmed();
    draft_.reject();
    setResultMessage(
        QString::fromUtf8("已拒绝"),
        QString::fromUtf8("{\n  \"ok\": false,\n  \"error_code\": \"draft_rejected\",\n  \"message\": ")
            + quote(reason)
            + QString::fromUtf8("\n}"));
    render();
}

void AgentReviewDialog::simulateFailure()
{
    setResultMessage(
        QString::fromUtf8("执行失败"),
        QString::fromUtf8("{\n  \"ok\": false,\n  \"error_code\": \"invalid_argument\",\n  \"message\": \"模拟失败。\"\n}"));
    render();
}

void AgentReviewDialog::editParameter()
{
    renderTrace();
}

void AgentReviewDialog::regenerateDraft()
{
    if (draft_.status() != SemanticDraftStatus::Rejected) {
        return;
    }
    const QString reason = rejection_reason_edit_->toPlainText().trimmed();
    requestDraftFromBackend(prompt_edit_->text().trimmed(), reason, QString::fromUtf8("草案已重新生成"));
}

void AgentReviewDialog::setExecutionResult(const AgentPlanExecutionResult& result)
{
    has_result_ = true;
    execution_succeeded_ = result.ok;
    created_operation_id_.clear();
    created_toolpath_id_.clear();
    if (result.ok) {
        for (std::size_t index = 0; index < result.object_ids.size(); ++index) {
            const QString object_id = fromStd(result.object_ids[index]);
            if (object_id.startsWith("op_")) {
                created_operation_id_ = object_id;
            } else if (object_id.startsWith("toolpath_")) {
                created_toolpath_id_ = object_id;
            }
        }
        if (created_operation_id_.isEmpty()) {
            created_operation_id_ = fromStd(result.primary_object_id);
        }
    }
    result_status_ = result.ok ? QString::fromUtf8("执行成功") : QString::fromUtf8("执行失败");

    QString body;
    body += "{\n";
    body += "  \"ok\": ";
    body += result.ok ? "true" : "false";
    body += ",\n  \"error_code\": ";
    body += quote(fromStd(result.error_code));
    body += ",\n  \"message\": ";
    body += quote(fromStd(result.message));
    body += ",\n  \"primary_object_id\": ";
    body += quote(fromStd(result.primary_object_id));
    body += ",\n  \"object_ids\": [";
    for (std::size_t index = 0; index < result.object_ids.size(); ++index) {
        if (index != 0u) {
            body += ", ";
        }
        body += quote(fromStd(result.object_ids[index]));
    }
    body += "]\n}";
    result_body_ = body;
}

bool AgentReviewDialog::requestClarificationAndResume(const AgentPlanExecutionResult& result)
{
    setExecutionResult(result);
    return true;
}

void AgentReviewDialog::setResultMessage(const QString& status, const QString& body)
{
    has_result_ = true;
    result_status_ = status;
    result_body_ = body;
}

QString AgentReviewDialog::statusText(SemanticDraftStatus status) const
{
    if (status == SemanticDraftStatus::Confirmed) {
        return QString::fromUtf8("已确认");
    }
    if (status == SemanticDraftStatus::Rejected) {
        return QString::fromUtf8("已拒绝");
    }
    return QString::fromUtf8("待审核");
}

QString AgentReviewDialog::draftStepText() const
{
    QString text;
    for (std::size_t index = 0; index < draft_.intentCount(); ++index) {
        const SemanticIntent& step = draft_.intentAt(index);
        if (!text.isEmpty()) {
            text += "\n\n";
        }
        text += QString::number(static_cast<int>(index + 1u));
        text += ". Intent: ";
        text += textForIntent(step);
        text += "\nTarget: ";
        if (step.target.kind == SemanticTargetKind::Query) {
            text += QString::fromUtf8("query / ") + fromStd(step.target.object_type) + " / " + fromStd(step.target.scope);
            std::map<std::string, std::string>::const_iterator operation_type = step.target.filters.find("operation_type");
            if (operation_type != step.target.filters.end()) {
                text += " / operation_type=" + fromStd(operation_type->second);
            }
        } else if (step.target.kind == SemanticTargetKind::Objects) {
            text += QString::fromUtf8("objects / ");
            for (std::size_t id_index = 0; id_index < step.target.object_ids.size(); ++id_index) {
                if (id_index != 0u) {
                    text += ",";
                }
                text += fromStd(step.target.object_ids[id_index]);
            }
        } else if (step.target.kind == SemanticTargetKind::Ref) {
            text += QString::fromUtf8("ref / ") + fromStd(step.target.ref);
        } else {
            text += QString::fromUtf8("intent-specific");
        }
        if (!step.visibility_actions.empty()) {
            text += "\nActions: ";
            for (std::size_t action_index = 0; action_index < step.visibility_actions.size(); ++action_index) {
                if (action_index != 0u) {
                    text += "; ";
                }
                text += fromStd(step.visibility_actions[action_index].visibility);
            }
        }
    }
    return text;
}

QString AgentReviewDialog::textForIntent(const SemanticIntent& step) const
{
    if (step.kind == SemanticIntentKind::MachineFeature) {
        return "machine_feature";
    }
    if (step.kind == SemanticIntentKind::CreateOperations) {
        return "create_operations";
    }
    if (step.kind == SemanticIntentKind::EditOperation) {
        return "edit_operation";
    }
    if (step.kind == SemanticIntentKind::RegenerateToolpath) {
        return "regenerate_toolpath";
    }
    if (step.kind == SemanticIntentKind::SetToolpathVisibility) {
        return "set_toolpath_visibility";
    }
    return "unknown";
}

QString AgentReviewDialog::joinTraceEvents() const
{
    QString text;
    const std::vector<std::string>& events = draft_.traceEvents();
    for (std::size_t index = 0; index < events.size(); ++index) {
        if (index != 0u) {
            text += "\n";
        }
        text += fromStd(events[index]);
    }
    return text;
}

QString AgentReviewDialog::quote(const QString& value) const
{
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    escaped.replace("\n", "\\n");
    return "\"" + escaped + "\"";
}

} // namespace camclaw
