#ifndef CAMCLAW_SEMANTIC_INTENT_PLAN_H
#define CAMCLAW_SEMANTIC_INTENT_PLAN_H

#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/AgentWorkflowService.h"

#include <map>
#include <string>
#include <vector>

namespace camclaw {

enum class SemanticDraftStatus {
    PendingReview,
    Confirmed,
    Rejected
};

enum class SemanticIntentKind {
    Unknown,
    MachineFeature,
    CreateOperations,
    EditOperation,
    RegenerateToolpath,
    SetToolpathVisibility
};

enum class SemanticTargetKind {
    Unknown,
    Objects,
    Query,
    Ref
};

struct SemanticTarget {
    SemanticTarget()
        : kind(SemanticTargetKind::Unknown)
    {
    }

    SemanticTargetKind kind;
    std::vector<std::string> object_ids;
    std::string object_type;
    std::string scope;
    std::map<std::string, std::string> filters;
    std::string ref;
};

struct OperationCreateItem {
    OperationCreateItem()
        : count(1)
    {
    }

    std::string operation_type;
    int count;
};

struct ParameterUpdateIntent {
    std::string parameter;
    std::string value;
    std::string expression;
};

struct VisibilityIntentAction {
    std::string visibility;
    SemanticTarget target;
};

struct SemanticIntent {
    SemanticIntent()
        : kind(SemanticIntentKind::Unknown),
          auto_generate_toolpath(false),
          open_editor(false)
    {
    }

    std::string id;
    SemanticIntentKind kind;
    SemanticTarget target;
    std::vector<OperationCreateItem> create_items;
    std::vector<ParameterUpdateIntent> updates;
    std::vector<VisibilityIntentAction> visibility_actions;
    bool auto_generate_toolpath;
    bool open_editor;
    std::map<std::string, std::string> requirements;
};

class SemanticPlanDraft {
public:
    explicit SemanticPlanDraft(const std::string& trace_id = "");

    const std::string& traceId() const;
    SemanticDraftStatus status() const;
    void setStatus(SemanticDraftStatus status);
    void confirm();
    void reject();
    void addIntent(const SemanticIntent& intent);
    std::size_t intentCount() const;
    const SemanticIntent& intentAt(std::size_t index) const;
    const std::vector<std::string>& traceEvents() const;
    void addTraceEvent(const std::string& event);

private:
    std::string trace_id_;
    SemanticDraftStatus status_;
    std::vector<SemanticIntent> intents_;
    std::vector<std::string> trace_events_;
};

struct SemanticPlanDraftDecodeResult {
    SemanticPlanDraftDecodeResult()
        : ok(false),
          draft("")
    {
    }

    bool ok;
    std::string error_code;
    std::string message;
    SemanticPlanDraft draft;
};

class SemanticPlanDraftJsonCodec {
public:
    SemanticPlanDraftDecodeResult decodeBackendDraft(const std::string& json) const;

private:
    bool decodeIntent(const std::string& object_json, SemanticIntent& intent, std::string& message) const;
    bool decodeTarget(const std::string& object_json, SemanticTarget& target, std::string& message) const;
};

class FeatureRecognitionService {
public:
    std::string recognizeOperationType(const CamObject& feature) const;
};

class ToolSelectionPolicy {
public:
    std::string resolveRelativeToolId(const CamObject& operation, const std::string& expression) const;
};

class ParameterExpressionResolver {
public:
    explicit ParameterExpressionResolver(const ToolSelectionPolicy& tool_selection_policy);

    std::string resolve(const CamObject& operation, const ParameterUpdateIntent& update) const;

private:
    std::string resolveRelativeNumber(
        const CamObject& operation,
        const std::string& parameter_name,
        const std::string& expression) const;

    const ToolSelectionPolicy& tool_selection_policy_;
};

class SemanticIntentExecutor {
public:
    SemanticIntentExecutor(Repository& repository, HumanInLoopService& human_in_loop_service);

    AgentPlanExecutionResult execute(const SemanticPlanDraft& draft);

private:
    struct IntentExecutionContext {
        std::vector<std::string> previous_primary_object_ids;
    };

    bool executeIntent(
        const SemanticIntent& intent,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result);
    bool executeMachineFeature(
        const SemanticIntent& intent,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result);
    bool executeCreateOperations(
        const SemanticIntent& intent,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result);
    bool executeEditOperation(
        const SemanticIntent& intent,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result);
    bool executeRegenerateToolpath(
        const SemanticIntent& intent,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result);
    bool executeSetToolpathVisibility(
        const SemanticIntent& intent,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result);
    std::vector<std::string> resolveTargetIds(
        const SemanticTarget& target,
        const IntentExecutionContext& context,
        AgentPlanExecutionResult& result,
        const std::string& expected_object_type) const;
    void appendSuccess(
        const std::string& primary_object_id,
        const std::vector<std::string>& object_ids,
        const std::string& trace_event,
        IntentExecutionContext& context,
        AgentPlanExecutionResult& result) const;

    Repository& repository_;
    HumanInLoopService& human_in_loop_service_;
    FeatureRecognitionService feature_recognition_service_;
    ToolSelectionPolicy tool_selection_policy_;
    ParameterExpressionResolver parameter_expression_resolver_;
};

} // namespace camclaw

#endif // CAMCLAW_SEMANTIC_INTENT_PLAN_H
