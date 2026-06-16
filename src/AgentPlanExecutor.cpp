#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/TargetResolver.h"

#include <cstdlib>

namespace camclaw {

namespace {

bool createsSingleOperation(const SkillStepDraft& draft_step)
{
    const std::string batch_count = draft_step.inputValue("batch_count");
    if (batch_count.empty()) {
        return true;
    }
    char* end = 0;
    const long parsed = std::strtol(batch_count.c_str(), &end, 10);
    return end != batch_count.c_str() && *end == '\0' && parsed == 1;
}

} // namespace

AgentPlanExecutor::AgentPlanExecutor(SkillRuntime& skill_runtime)
    : skill_runtime_(skill_runtime),
      trace_service_(0),
      repository_(0),
      human_in_loop_service_(0)
{
}

AgentPlanExecutor::AgentPlanExecutor(SkillRuntime& skill_runtime, TraceService* trace_service)
    : skill_runtime_(skill_runtime),
      trace_service_(trace_service),
      repository_(0),
      human_in_loop_service_(0)
{
}

AgentPlanExecutor::AgentPlanExecutor(
    SkillRuntime& skill_runtime,
    Repository& repository,
    HumanInLoopService& human_in_loop_service)
    : skill_runtime_(skill_runtime),
      trace_service_(0),
      repository_(&repository),
      human_in_loop_service_(&human_in_loop_service)
{
}

AgentPlanExecutionResult AgentPlanExecutor::execute(const AgentPlanDraft& draft)
{
    AgentPlanExecutionResult result;

    if (draft.status() != DraftStatus::Confirmed) {
        result.ok = false;
        result.error_code = "draft_not_confirmed";
        result.message = "AgentPlanDraft must be confirmed before execution.";
        result.trace_events.push_back("draft_execution_rejected");
        if (trace_service_ != 0) {
            trace_service_->record(draft.traceId(), "draft", "draft_execution_rejected", std::string(), result.message);
        }
        return result;
    }

    result.trace_events.push_back("draft_execution_started");
    if (trace_service_ != 0) {
        trace_service_->record(draft.traceId(), "draft", "draft_execution_started", std::string(), "Draft execution started.");
    }

    for (std::size_t index = 0; index < draft.stepCount(); ++index) {
        std::string resolved_target_object_id;
        if (!resolveDraftStepTarget(draft, index, result, resolved_target_object_id)) {
            return result;
        }

        SkillDefinition skill;
        if (!buildSkillDefinition(draft.stepAt(index), skill)) {
            result.ok = false;
            result.error_code = "unsupported_skill";
            result.message = "AgentPlanDraft contains an unsupported Skill.";
            result.trace_events.push_back("draft_execution_failed");
            if (trace_service_ != 0) {
                trace_service_->record(draft.traceId(), "draft", "draft_execution_failed", std::string(), result.message);
            }
            return result;
        }

        SkillExecutionInput input;
        input.trace_id = draft.traceId();
        input.target_object_id = resolved_target_object_id.empty()
            ? draft.stepAt(index).inputValue("target_object_id")
            : resolved_target_object_id;

        const SkillExecutionResult skill_result = skill_runtime_.execute(skill, input);
        result.trace_events.insert(result.trace_events.end(), skill_result.trace_events.begin(), skill_result.trace_events.end());

        if (!skill_result.ok) {
            result.ok = false;
            result.error_code = skill_result.error_code;
            result.message = skill_result.message;
            result.primary_object_id = skill_result.primary_object_id;
            result.object_ids.insert(result.object_ids.end(), skill_result.object_ids.begin(), skill_result.object_ids.end());
            result.trace_events.push_back("draft_execution_failed");
            if (trace_service_ != 0) {
                trace_service_->record(draft.traceId(), "draft", "draft_execution_failed", result.primary_object_id, result.message);
            }
            return result;
        }

        if (result.primary_object_id.empty()) {
            result.primary_object_id = skill_result.primary_object_id;
        }
        result.object_ids.insert(result.object_ids.end(), skill_result.object_ids.begin(), skill_result.object_ids.end());
    }

    result.ok = true;
    result.trace_events.push_back("draft_execution_completed");
    if (trace_service_ != 0) {
        trace_service_->record(draft.traceId(), "draft", "draft_execution_completed", result.primary_object_id, "Draft execution completed.");
    }
    return result;
}

bool AgentPlanExecutor::resolveDraftStepTarget(
    const AgentPlanDraft& draft,
    std::size_t step_index,
    AgentPlanExecutionResult& result,
    std::string& resolved_target_object_id)
{
    resolved_target_object_id.clear();
    if (repository_ == 0 || human_in_loop_service_ == 0) {
        return true;
    }

    const SkillStepDraft& step = draft.stepAt(step_index);
    if (step.inputValue("scope") == "operation_type"
        && !step.inputValue("operation_type").empty()
        && (step.skillId() == "browser.openOperationEditor"
            || step.skillId() == "browser.updateOperation"
            || step.skillId() == "browser.generateToolpath")) {
        std::map<std::string, std::string> query;
        query["operation_type"] = step.inputValue("operation_type");

        TargetResolver resolver(*repository_);
        const TargetResolutionResult resolution = resolver.resolveOperation(query);
        if (resolution.status == TargetResolutionStatus::Resolved) {
            resolved_target_object_id = resolution.target_object_id;
            result.trace_events.push_back("target_resolved");
            return true;
        }

        if (resolution.status == TargetResolutionStatus::NeedsClarification) {
            ResponseSchema schema;
            schema.type = "choose_one";
            schema.target_field = "target_object_id";
            schema.allow_free_text = true;
            result.clarification = human_in_loop_service_->createRequest(
                draft,
                step_index,
                "ambiguous_target",
                "operation",
                "找到多个匹配工序，请选择要操作哪一个。",
                query,
                resolution.candidates,
                schema);
            result.ok = false;
            result.needs_clarification = true;
            result.error_code = "needs_clarification";
            result.message = resolution.message;
            result.trace_events.push_back("clarification_requested");
            return false;
        }

        result.ok = false;
        result.error_code = resolution.error_code;
        result.message = resolution.message;
        result.trace_events.push_back("draft_execution_failed");
        return false;
    }

    return true;
}

bool AgentPlanExecutor::buildSkillDefinition(const SkillStepDraft& draft_step, SkillDefinition& skill) const
{
    if (draft_step.skillId() == "browser.createOperation"
        || draft_step.skillId() == "browser.updateOperation"
        || draft_step.skillId() == "browser.generateToolpath"
        || draft_step.skillId() == "browser.setToolpathVisibility") {
        skill.skill_id = draft_step.skillId();
        SkillCommandStep command;
        command.command_id = draft_step.skillId();
        command.target_object_id_expression = "$input.target_object_id";
        command.args = draft_step.inputs();
        skill.steps.push_back(command);
        if (draft_step.skillId() == "browser.createOperation"
            && draft_step.inputValue("auto_generate_toolpath") == "true"
            && createsSingleOperation(draft_step)) {
            SkillCommandStep generate_toolpath;
            generate_toolpath.command_id = "browser.generateToolpath";
            generate_toolpath.target_object_id_expression = "$step0.primary_object_id";
            skill.steps.push_back(generate_toolpath);
        }
        return true;
    }

    if (draft_step.skillId() != "browser.create_roughing_operation"
        && draft_step.skillId() != "browser.create_roughing_operation_and_generate_toolpath") {
        return false;
    }

    skill.skill_id = draft_step.skillId();

    SkillCommandStep create_operation;
    create_operation.command_id = "browser.create_roughing_operation";
    create_operation.target_object_id_expression = "$input.target_object_id";
    create_operation.args = draft_step.inputs();
    skill.steps.push_back(create_operation);

    if (draft_step.skillId() == "browser.create_roughing_operation") {
        return true;
    }

    SkillCommandStep generate_toolpath;
    generate_toolpath.command_id = "browser.generate_toolpath";
    generate_toolpath.target_object_id_expression = "$step0.primary_object_id";
    skill.steps.push_back(generate_toolpath);

    return true;
}

} // namespace camclaw
