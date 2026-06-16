#ifndef CAMCLAW_QT_AGENT_REVIEW_DIALOG_H
#define CAMCLAW_QT_AGENT_REVIEW_DIALOG_H

#include "camclaw/AgentPlanDraft.h"
#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/HumanInLoopService.h"
#include "camclaw/qt/AgentDraftService.h"

#include <QDialog>
#include <QLineEdit>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>

namespace camclaw {

class AgentReviewDialog : public QDialog {
    Q_OBJECT

public:
    AgentReviewDialog(
        const QString& target_object_id,
        const QString& target_display_name,
        AgentDraftService& draft_service,
        AgentPlanExecutor& executor,
        HumanInLoopService* human_in_loop_service = 0,
        QWidget* parent = 0);

    QString parameterValue(const QString& name) const;
    QString resultStatusText() const;
    QString resultBodyText() const;
    QString traceText() const;
    QString draftStepTextForTest() const;
    bool executionSucceeded() const;
    QString createdOperationId() const;
    QString createdToolpathId() const;

private slots:
    void generateDraft();
    void confirmDraft();
    void rejectDraft();
    void simulateFailure();
    void editParameter();
    void regenerateDraft();

private:
    void buildUi();
    void render();
    void renderDraft();
    void renderParameters();
    void renderResult();
    void renderTrace();
    void renderControls();
    void setExecutionResult(const AgentPlanExecutionResult& result);
    bool requestClarificationAndResume(const AgentPlanExecutionResult& result);
    void setResultMessage(const QString& status, const QString& body);
    QString statusText(DraftStatus status) const;
    QString draftStepText() const;
    QString commandTextForSkill(const SkillStepDraft& step) const;
    QString joinTraceEvents() const;
    QString quote(const QString& value) const;
    bool requestDraftFromBackend(const QString& prompt, const QString& rejection_reason, const QString& success_status);

    QString target_object_id_;
    QString target_display_name_;
    AgentDraftService& draft_service_;
    AgentPlanDraft draft_;
    AgentPlanExecutor& executor_;
    HumanInLoopService* human_in_loop_service_;
    bool has_result_;
    bool execution_succeeded_;
    QString created_operation_id_;
    QString created_toolpath_id_;
    QString result_status_;
    QString result_body_;

    QLabel* trace_id_label_;
    QLabel* draft_status_label_;
    QLabel* target_label_;
    QLineEdit* prompt_edit_;
    QLabel* step_label_;
    QMap<QString, QLineEdit*> parameter_edits_;
    QLabel* result_status_label_;
    QPlainTextEdit* result_body_edit_;
    QPlainTextEdit* trace_edit_;
    QPlainTextEdit* rejection_reason_edit_;
    QPushButton* confirm_button_;
    QPushButton* generate_button_;
    QPushButton* reject_button_;
    QPushButton* simulate_failure_button_;
    QPushButton* regenerate_button_;
    QPushButton* close_button_;
};

} // namespace camclaw

#endif // CAMCLAW_QT_AGENT_REVIEW_DIALOG_H
