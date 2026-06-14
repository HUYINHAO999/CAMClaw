#include "camclaw/AgentPlanExecutor.h"

namespace camclaw {

AgentPlanExecutor::AgentPlanExecutor(SkillRuntime& skill_runtime)
    : skill_runtime_(skill_runtime)
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
        return result;
    }

    result.trace_events.push_back("draft_execution_started");

    for (std::size_t index = 0; index < draft.stepCount(); ++index) {
        SkillDefinition skill;
        if (!buildSkillDefinition(draft.stepAt(index), skill)) {
            result.ok = false;
            result.error_code = "unsupported_skill";
            result.message = "AgentPlanDraft contains an unsupported Skill.";
            result.trace_events.push_back("draft_execution_failed");
            return result;
        }

        SkillExecutionInput input;
        input.trace_id = draft.traceId();
        input.target_object_id = draft.stepAt(index).inputValue("target_object_id");

        const SkillExecutionResult skill_result = skill_runtime_.execute(skill, input);
        result.trace_events.insert(result.trace_events.end(), skill_result.trace_events.begin(), skill_result.trace_events.end());

        if (!skill_result.ok) {
            result.ok = false;
            result.error_code = skill_result.error_code;
            result.message = skill_result.message;
            result.primary_object_id = skill_result.primary_object_id;
            result.object_ids.insert(result.object_ids.end(), skill_result.object_ids.begin(), skill_result.object_ids.end());
            result.trace_events.push_back("draft_execution_failed");
            return result;
        }

        if (result.primary_object_id.empty()) {
            result.primary_object_id = skill_result.primary_object_id;
        }
        result.object_ids.insert(result.object_ids.end(), skill_result.object_ids.begin(), skill_result.object_ids.end());
    }

    result.ok = true;
    result.trace_events.push_back("draft_execution_completed");
    return result;
}

bool AgentPlanExecutor::buildSkillDefinition(const SkillStepDraft& draft_step, SkillDefinition& skill) const
{
    if (draft_step.skillId() != "browser.create_roughing_operation_and_generate_toolpath") {
        return false;
    }

    skill.skill_id = draft_step.skillId();

    SkillCommandStep create_operation;
    create_operation.command_id = "browser.create_roughing_operation";
    create_operation.target_object_id_expression = "$input.target_object_id";
    create_operation.args = draft_step.inputs();
    skill.steps.push_back(create_operation);

    SkillCommandStep generate_toolpath;
    generate_toolpath.command_id = "browser.generate_toolpath";
    generate_toolpath.target_object_id_expression = "$step0.primary_object_id";
    skill.steps.push_back(generate_toolpath);

    return true;
}

} // namespace camclaw
