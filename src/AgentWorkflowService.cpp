#include "camclaw/AgentWorkflowService.h"

namespace camclaw {

void Repository::save(const CamObject& object)
{
    objects_[object.object_id] = object;
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

        const std::string operation_id = "op_roughing_" + request.target_object_id;
        const std::string toolpath_id = "toolpath_" + operation_id;

        result.status = WorkflowStatus::Completed;
        result.primary_object_id = operation_id;
        result.created_object_ids.push_back(operation_id);
        result.created_object_ids.push_back(toolpath_id);
        repository_.save(CamObject(operation_id, ObjectType::Operation, "Roughing operation"));
        repository_.save(CamObject(toolpath_id, ObjectType::Toolpath, "Generated toolpath"));
        result.trace_events.push_back("gateway_validated");
        result.trace_events.push_back("adapter_dispatched");
        result.trace_events.push_back("browser_console_dispatched");
        result.trace_events.push_back("roughing_operation_created");
        result.trace_events.push_back("toolpath_generated");
        return result;
    }

    if (request.target_object_id.empty() && request.active_selection.has_object) {
        result.status = WorkflowStatus::NeedsTargetConfirmation;
        result.confirmation.target_object_id = request.active_selection.object_id;
        result.confirmation.target_object_type = request.active_selection.object_type;
        result.confirmation.target_display_name = request.active_selection.display_name;
        result.confirmation.message = "Confirm current selection before creating roughing operation and toolpath.";
        result.trace_events.push_back("target_confirmation_requested");
        return result;
    }

    result.status = WorkflowStatus::Failed;
    result.error_code = "missing_target";
    result.message = "A target object is required.";
    result.trace_events.push_back("target_missing");
    return result;
}

} // namespace camclaw
