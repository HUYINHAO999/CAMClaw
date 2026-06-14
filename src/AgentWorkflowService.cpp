#include "camclaw/AgentWorkflowService.h"
#include "camclaw/SkillRuntime.h"

namespace camclaw {

bool Repository::save(const CamObject& object)
{
    if (object.object_id.empty() || exists(object.object_id)) {
        return false;
    }

    objects_[object.object_id] = object;
    return true;
}

bool Repository::exists(const std::string& object_id) const
{
    return objects_.find(object_id) != objects_.end();
}

CamObject Repository::get(const std::string& object_id) const
{
    const std::map<std::string, CamObject>::const_iterator found = objects_.find(object_id);
    if (found == objects_.end()) {
        return CamObject();
    }

    return found->second;
}

AgentWorkflowService::AgentWorkflowService(Repository& repository)
    : repository_(repository)
{
}

WorkflowResult AgentWorkflowService::submitRoughingAndToolpath(const RoughingWorkflowRequest& request)
{
    WorkflowResult result;

    if (!request.target_object_id.empty()) {
        const CamObject target = repository_.get(request.target_object_id);
        if (target.object_type != ObjectType::Feature) {
            result.status = WorkflowStatus::Failed;
            result.primary_object_id = request.target_object_id;
            result.error_code = "invalid_target_type";
            result.message = "Roughing operation creation requires a feature target.";
            result.trace_events.push_back("gateway_rejected");
            return result;
        }

        SkillDefinition skill;
        skill.skill_id = "browser.create_roughing_operation_and_generate_toolpath";

        SkillCommandStep create_operation;
        create_operation.command_id = "browser.create_roughing_operation";
        create_operation.target_object_id_expression = "$input.target_object_id";
        create_operation.args["operation_type"] = request.operation_type;
        create_operation.args["tool_id"] = request.tool_id;
        create_operation.args["stepover"] = request.stepover;
        create_operation.args["stepdown"] = request.stepdown;
        create_operation.args["tolerance"] = request.tolerance;
        skill.steps.push_back(create_operation);

        SkillCommandStep generate_toolpath;
        generate_toolpath.command_id = "browser.generate_toolpath";
        generate_toolpath.target_object_id_expression = "$step0.primary_object_id";
        skill.steps.push_back(generate_toolpath);

        SkillExecutionInput skill_input;
        skill_input.trace_id = request.trace_id;
        skill_input.target_object_id = request.target_object_id;

        BrowserConsole browser_console(repository_);
        ActionGateway gateway(repository_, browser_console);
        SkillRuntime runtime(gateway);
        const SkillExecutionResult skill_result = runtime.execute(skill, skill_input);

        result.trace_events.push_back("gateway_validated");
        result.trace_events.push_back("adapter_dispatched");
        result.trace_events.insert(result.trace_events.end(), skill_result.trace_events.begin(), skill_result.trace_events.end());

        if (!skill_result.ok) {
            result.status = WorkflowStatus::Failed;
            result.error_code = skill_result.error_code;
            result.message = skill_result.message;
            result.primary_object_id = skill_result.primary_object_id;
            return result;
        }

        result.status = WorkflowStatus::Completed;
        result.primary_object_id = skill_result.primary_object_id;
        result.created_object_ids = skill_result.object_ids;
        return result;
    }

    if (request.active_selection.has_object && request.active_selection.object_type == ObjectType::Feature) {
        result.status = WorkflowStatus::NeedsTargetConfirmation;
        result.confirmation.target_object_id = request.active_selection.object_id;
        result.confirmation.target_object_type = request.active_selection.object_type;
        result.confirmation.target_display_name = request.active_selection.display_name;
        result.confirmation.message = "Confirm current selection before creating roughing operation and toolpath.";
        result.trace_events.push_back("target_confirmation_requested");
        return result;
    }

    if (request.active_selection.has_object) {
        result.status = WorkflowStatus::NeedsValidTarget;
        result.primary_object_id = request.active_selection.object_id;
        result.error_code = "invalid_selection_type";
        result.message = "Select or specify a feature before creating a roughing operation.";
        result.trace_events.push_back("valid_target_required");
        return result;
    }

    result.status = WorkflowStatus::NeedsValidTarget;
    result.error_code = "missing_target";
    result.message = "Select or specify a feature before creating a roughing operation.";
    result.trace_events.push_back("target_required");
    return result;
}

} // namespace camclaw
