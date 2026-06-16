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
    AgentPlanExecutor& executor,
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
    trace_id_label_ = new QLabel(this);
    trace_id_label_->setObjectName("traceIdLabel");
    draft_status_label_ = new QLabel(this);
    draft_status_label_->setObjectName("draftStatusLabel");
    target_label_ = new QLabel(QString::fromUtf8("目标对象: ") + target_display_name_ + " / " + target_object_id_, this);
    target_label_->setObjectName("targetLabel");

    prompt_edit_ = new QLineEdit(this);
    prompt_edit_->setObjectName("agentPromptEdit");
    prompt_edit_->setPlaceholderText(QString::fromUtf8("例如：给当前型腔创建粗加工工序，使用 10mm 平刀"));
    generate_button_ = new QPushButton(QString::fromUtf8("生成草案"), this);
    generate_button_->setObjectName("generateDraftButton");

    step_label_ = new QLabel(this);
    step_label_->setObjectName("stepLabel");
    step_label_->setWordWrap(true);

    QGroupBox* draft_group = new QGroupBox(QString::fromUtf8("草案步骤"), this);
    QVBoxLayout* draft_layout = new QVBoxLayout(draft_group);
    draft_layout->addWidget(step_label_);

    QGroupBox* parameter_group = new QGroupBox(QString::fromUtf8("参数"), this);
    QFormLayout* parameter_layout = new QFormLayout(parameter_group);
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

    result_status_label_ = new QLabel(this);
    result_status_label_->setObjectName("resultStatusLabel");
    result_body_edit_ = new QPlainTextEdit(this);
    result_body_edit_->setObjectName("resultBodyEdit");
    result_body_edit_->setReadOnly(true);

    QGroupBox* result_group = new QGroupBox(QString::fromUtf8("结果"), this);
    QVBoxLayout* result_layout = new QVBoxLayout(result_group);
    result_layout->addWidget(result_status_label_);
    result_layout->addWidget(result_body_edit_);

    trace_edit_ = new QPlainTextEdit(this);
    trace_edit_->setObjectName("traceEdit");
    trace_edit_->setReadOnly(true);
    QGroupBox* trace_group = new QGroupBox(QString::fromUtf8("Trace"), this);
    QVBoxLayout* trace_layout = new QVBoxLayout(trace_group);
    trace_layout->addWidget(trace_edit_);

    rejection_reason_edit_ = new QPlainTextEdit(this);
    rejection_reason_edit_->setObjectName("rejectionReasonEdit");
    rejection_reason_edit_->setPlaceholderText(QString::fromUtf8("例如：刀具太大，换小刀"));

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
    header_layout->addWidget(trace_id_label_);
    header_layout->addStretch();
    header_layout->addWidget(draft_status_label_);

    QHBoxLayout* prompt_layout = new QHBoxLayout();
    prompt_layout->addWidget(new QLabel(QString::fromUtf8("需求"), this));
    prompt_layout->addWidget(prompt_edit_, 1);
    prompt_layout->addWidget(generate_button_);

    QGridLayout* grid = new QGridLayout();
    grid->addWidget(draft_group, 0, 0);
    grid->addWidget(parameter_group, 0, 1);
    grid->addWidget(result_group, 1, 0);
    grid->addWidget(trace_group, 1, 1);

    QVBoxLayout* action_layout = new QVBoxLayout();
    action_layout->addWidget(confirm_button_);
    action_layout->addWidget(simulate_failure_button_);
    action_layout->addWidget(new QLabel(QString::fromUtf8("拒绝原因"), this));
    action_layout->addWidget(rejection_reason_edit_);
    action_layout->addWidget(reject_button_);
    action_layout->addWidget(regenerate_button_);
    action_layout->addStretch();
    action_layout->addWidget(close_button_);

    QVBoxLayout* left_layout = new QVBoxLayout();
    left_layout->addLayout(header_layout);
    left_layout->addWidget(target_label_);
    left_layout->addLayout(prompt_layout);
    left_layout->addLayout(grid);

    QHBoxLayout* root_layout = new QHBoxLayout(this);
    root_layout->addLayout(left_layout, 1);
    root_layout->addLayout(action_layout);

    resize(920, 620);

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
    if (draft_.stepCount() == 0u) {
        step_label_->setText(QString::fromUtf8("请输入需求并生成草案。"));
        return;
    }
    step_label_->setText(draftStepText());
}

void AgentReviewDialog::renderParameters()
{
    if (draft_.stepCount() == 0u) {
        for (QMap<QString, QLineEdit*>::iterator it = parameter_edits_.begin(); it != parameter_edits_.end(); ++it) {
            it.value()->clear();
            it.value()->setEnabled(false);
        }
        return;
    }
    const SkillStepDraft& step = draft_.stepAt(0u);
    for (QMap<QString, QLineEdit*>::iterator it = parameter_edits_.begin(); it != parameter_edits_.end(); ++it) {
        it.value()->blockSignals(true);
        it.value()->setText(fromStd(step.inputValue(toStd(it.key()))));
        it.value()->setEnabled(draft_.status() == DraftStatus::PendingReview);
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
    const bool editable = draft_.status() == DraftStatus::PendingReview;
    const bool has_draft = draft_.stepCount() > 0u;
    confirm_button_->setEnabled(editable && has_draft);
    reject_button_->setEnabled(editable && has_draft);
    simulate_failure_button_->setEnabled(editable && has_draft);
    rejection_reason_edit_->setEnabled(editable && has_draft);
    regenerate_button_->setEnabled(draft_.status() == DraftStatus::Rejected);
    generate_button_->setEnabled(draft_.status() != DraftStatus::Confirmed);
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
        return false;
    }

    draft_ = service_result.draft;
    setResultMessage(
        success_status,
        QString::fromUtf8("{\n  \"ok\": true,\n  \"source\": \"python_backend_llm_planner\",\n  \"message\": \"草案已生成，请审核参数。\"\n}"));
    render();
    return true;
}

void AgentReviewDialog::confirmDraft()
{
    if (draft_.status() != DraftStatus::PendingReview) {
        return;
    }
    draft_.confirm();
    const AgentPlanExecutionResult result = executor_.execute(draft_);
    if (result.needs_clarification && requestClarificationAndResume(result)) {
        render();
        if (execution_succeeded_) {
            accept();
        }
        return;
    }
    setExecutionResult(result);
    render();
    if (execution_succeeded_) {
        accept();
    }
}

void AgentReviewDialog::rejectDraft()
{
    if (draft_.status() != DraftStatus::PendingReview) {
        return;
    }
    const QString reason = rejection_reason_edit_->toPlainText().trimmed();
    draft_.reject(toStd(reason));
    setResultMessage(
        QString::fromUtf8("已拒绝"),
        QString::fromUtf8("{\n  \"ok\": false,\n  \"error_code\": \"draft_rejected\",\n  \"message\": ")
            + quote(reason)
            + QString::fromUtf8("\n}"));
    render();
}

void AgentReviewDialog::simulateFailure()
{
    if (draft_.status() != DraftStatus::PendingReview) {
        return;
    }
    draft_.editStepInput(0u, "stepover", "0");
    draft_.confirm();
    setExecutionResult(executor_.execute(draft_));
    render();
}

void AgentReviewDialog::editParameter()
{
    if (draft_.status() != DraftStatus::PendingReview) {
        return;
    }
    QObject* sender_object = sender();
    for (QMap<QString, QLineEdit*>::const_iterator it = parameter_edits_.begin(); it != parameter_edits_.end(); ++it) {
        if (it.value() == sender_object) {
            draft_.editStepInput(0u, toStd(it.key()), toStd(it.value()->text()));
            break;
        }
    }
    renderTrace();
}

void AgentReviewDialog::regenerateDraft()
{
    if (draft_.status() != DraftStatus::Rejected) {
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
    if (human_in_loop_service_ == 0) {
        setExecutionResult(result);
        return false;
    }

    QDialog clarification_dialog(this);
    clarification_dialog.setObjectName("humanInLoopDialog");
    clarification_dialog.setWindowTitle(QString::fromUtf8("需要补充目标"));

    QVBoxLayout* layout = new QVBoxLayout(&clarification_dialog);
    QLabel* question = new QLabel(fromStd(result.clarification.question), &clarification_dialog);
    question->setObjectName("humanInLoopQuestionLabel");
    question->setWordWrap(true);
    layout->addWidget(question);

    QListWidget* candidate_list = new QListWidget(&clarification_dialog);
    candidate_list->setObjectName("humanInLoopCandidateList");
    for (std::size_t index = 0u; index < result.clarification.candidates.size(); ++index) {
        const ClarificationCandidate& candidate = result.clarification.candidates[index];
        QListWidgetItem* item = new QListWidgetItem(
            fromStd(candidate.label)
                + " / "
                + fromStd(candidate.id)
                + "  "
                + fromStd(candidate.description),
            candidate_list);
        item->setData(Qt::UserRole, fromStd(candidate.id));
    }
    if (candidate_list->count() > 0) {
        candidate_list->setCurrentRow(candidate_list->count() - 1);
    }
    layout->addWidget(candidate_list);

    QLineEdit* free_text = new QLineEdit(&clarification_dialog);
    free_text->setObjectName("humanInLoopFreeTextEdit");
    free_text->setPlaceholderText(QString::fromUtf8("也可以输入：最后一个 / 第2个 / 工序名称"));
    layout->addWidget(free_text);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &clarification_dialog);
    buttons->setObjectName("humanInLoopButtons");
    layout->addWidget(buttons);
    connect(buttons, SIGNAL(accepted()), &clarification_dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &clarification_dialog, SLOT(reject()));

    if (clarification_dialog.exec() != QDialog::Accepted) {
        setResultMessage(
            QString::fromUtf8("等待澄清"),
            QString::fromUtf8("{\n  \"ok\": false,\n  \"error_code\": \"clarification_cancelled\",\n  \"message\": \"用户取消了目标澄清。\"\n}"));
        return false;
    }

    ClarificationResponse response;
    response.clarification_id = result.clarification.clarification_id;
    const QString typed_text = free_text->text().trimmed();
    if (!typed_text.isEmpty()) {
        response.free_text = toStd(typed_text);
    } else if (candidate_list->currentItem() != 0) {
        response.selected_ids.push_back(toStd(candidate_list->currentItem()->data(Qt::UserRole).toString()));
    }

    const ClarificationResumeResult resume = human_in_loop_service_->submitResponse(response);
    if (!resume.ok) {
        setResultMessage(
            QString::fromUtf8("澄清失败"),
            QString::fromUtf8("{\n  \"ok\": false,\n  \"error_code\": ")
                + quote(fromStd(resume.error_code))
                + QString::fromUtf8(",\n  \"message\": ")
                + quote(fromStd(resume.message))
                + QString::fromUtf8("\n}"));
        return false;
    }

    draft_ = resume.resumed_draft;
    setExecutionResult(executor_.execute(draft_));
    return true;
}

void AgentReviewDialog::setResultMessage(const QString& status, const QString& body)
{
    has_result_ = true;
    result_status_ = status;
    result_body_ = body;
}

QString AgentReviewDialog::statusText(DraftStatus status) const
{
    if (status == DraftStatus::Confirmed) {
        return QString::fromUtf8("已确认");
    }
    if (status == DraftStatus::Rejected) {
        return QString::fromUtf8("已拒绝");
    }
    return QString::fromUtf8("待审核");
}

QString AgentReviewDialog::draftStepText() const
{
    QString text;
    for (std::size_t index = 0; index < draft_.stepCount(); ++index) {
        const SkillStepDraft& step = draft_.stepAt(index);
        if (!text.isEmpty()) {
            text += "\n\n";
        }
        text += QString::number(static_cast<int>(index + 1u));
        text += ". Skill: ";
        text += fromStd(step.skillId());
        text += "\nTarget: ";
        if (step.skillId() == "browser.setToolpathVisibility" && step.inputValue("scope") == "operation_type") {
            text += QString::fromUtf8("工序类型 / ") + fromStd(step.inputValue("operation_type"));
        } else if (step.skillId() == "browser.setToolpathVisibility" && step.inputValue("scope") == "list") {
            text += QString::fromUtf8("刀轨列表 / ") + fromStd(step.inputValue("toolpath_ids"));
        } else {
            text += fromStd(step.inputValue("target_object_id"));
        }
        text += "\nCommand: ";
        text += commandTextForSkill(step);
        text += "\nInputs: ";
        bool first = true;
        const std::map<std::string, std::string>& inputs = step.inputs();
        for (std::map<std::string, std::string>::const_iterator it = inputs.begin(); it != inputs.end(); ++it) {
            if (!first) {
                text += ", ";
            }
            first = false;
            text += fromStd(it->first) + "=" + fromStd(it->second);
        }
    }
    return text;
}

QString AgentReviewDialog::commandTextForSkill(const SkillStepDraft& step) const
{
    if (step.skillId() == "browser.createOperation") {
        return "browser.createOperation";
    }
    if (step.skillId() == "browser.updateOperation") {
        if (step.inputValue("recompute_toolpath") == "true") {
            return "browser.updateOperation -> browser.generateToolpath";
        }
        return "browser.updateOperation";
    }
    if (step.skillId() == "browser.generateToolpath") {
        return "browser.generateToolpath";
    }
    if (step.skillId() == "browser.setToolpathVisibility") {
        const QString visibility = fromStd(step.inputValue("visibility"));
        const QString scope = fromStd(step.inputValue("scope"));
        return "browser.setToolpathVisibility(" + visibility + ", " + scope + ")";
    }
    if (step.skillId() == "browser.create_roughing_operation") {
        return "browser.create_roughing_operation";
    }
    if (step.skillId() == "browser.create_roughing_operation_and_generate_toolpath") {
        return "browser.create_roughing_operation -> browser.generate_toolpath";
    }
    return QString::fromUtf8("unsupported");
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
