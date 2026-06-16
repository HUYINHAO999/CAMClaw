#include "camclaw/AgentWorkflowService.h"

#include <sstream>

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

bool Repository::update(const CamObject& object)
{
    if (object.object_id.empty() || !exists(object.object_id)) {
        return false;
    }

    objects_[object.object_id] = object;
    return true;
}

bool Repository::remove(const std::string& object_id)
{
    return objects_.erase(object_id) > 0u;
}

std::vector<CamObject> Repository::objectsByType(ObjectType object_type) const
{
    std::vector<CamObject> objects;
    for (std::map<std::string, CamObject>::const_iterator it = objects_.begin(); it != objects_.end(); ++it) {
        if (it->second.object_type == object_type) {
            objects.push_back(it->second);
        }
    }
    return objects;
}

OperationCatalog OperationCatalog::defaults()
{
    OperationCatalog catalog;

    std::map<std::string, std::string> roughing_defaults;
    roughing_defaults["tool_id"] = "tool_010";
    roughing_defaults["stepover"] = "2.0";
    roughing_defaults["stepdown"] = "1.0";
    roughing_defaults["tolerance"] = "0.02";
    catalog.registerDefinition(OperationDefinition("roughing", "型腔铣", roughing_defaults));
    catalog.registerDefinition(OperationDefinition("pocket", "型腔铣", roughing_defaults));
    catalog.registerDefinition(OperationDefinition("pocket_roughing", "型腔铣", roughing_defaults));

    std::map<std::string, std::string> finishing_defaults;
    finishing_defaults["tool_id"] = "tool_006";
    finishing_defaults["stepover"] = "0.4";
    finishing_defaults["stepdown"] = "0.3";
    finishing_defaults["tolerance"] = "0.01";
    catalog.registerDefinition(OperationDefinition("finishing", "平面铣", finishing_defaults));

    std::map<std::string, std::string> drilling_defaults;
    drilling_defaults["tool_id"] = "drill_006";
    drilling_defaults["stepover"] = "0.0";
    drilling_defaults["stepdown"] = "3.0";
    drilling_defaults["tolerance"] = "0.02";
    catalog.registerDefinition(OperationDefinition("drilling", "钻孔", drilling_defaults));

    return catalog;
}

void OperationCatalog::registerDefinition(const OperationDefinition& definition)
{
    definitions_[definition.operation_type] = definition;
}

bool OperationCatalog::find(const std::string& operation_type, OperationDefinition& definition) const
{
    const std::map<std::string, OperationDefinition>::const_iterator found = definitions_.find(operation_type);
    if (found == definitions_.end()) {
        return false;
    }

    definition = found->second;
    return true;
}

std::vector<OperationDefinition> OperationCatalog::definitions() const
{
    std::vector<OperationDefinition> result;
    for (std::map<std::string, OperationDefinition>::const_iterator it = definitions_.begin(); it != definitions_.end(); ++it) {
        result.push_back(it->second);
    }
    return result;
}

OperationFactory::OperationFactory(const OperationCatalog& catalog)
    : catalog_(catalog)
{
}

bool OperationFactory::create(
    Repository& repository,
    const CreateOperationRequest& request,
    CamObject& operation,
    std::string& error_code,
    std::string& message) const
{
    OperationDefinition definition;
    if (!catalog_.find(request.operation_type, definition)) {
        error_code = "unsupported_operation_type";
        message = "Operation type is not registered in OperationCatalog.";
        return false;
    }

    operation = CamObject(uniqueObjectId(repository, baseObjectId(request)), ObjectType::Operation, definition.display_name);
    operation.parent_object_id = request.target_object_id;
    operation.attributes = definition.default_parameters;
    operation.attributes["operation_type"] = request.operation_type;
    operation.attributes["toolpath_status"] = "not_calculated";

    for (std::map<std::string, std::string>::const_iterator it = request.parameters.begin(); it != request.parameters.end(); ++it) {
        operation.attributes[it->first] = it->second;
    }

    if (!repository.save(operation)) {
        error_code = "duplicate_object_id";
        message = "Operation object ID already exists.";
        return false;
    }

    return true;
}

std::string OperationFactory::baseObjectId(const CreateOperationRequest& request) const
{
    if (!request.target_object_id.empty()) {
        return "op_" + request.operation_type + "_" + request.target_object_id;
    }
    return "op_" + request.operation_type;
}

std::string OperationFactory::uniqueObjectId(Repository& repository, const std::string& base_id) const
{
    if (!repository.exists(base_id)) {
        return base_id;
    }

    for (int index = 2; index < 10000; ++index) {
        std::ostringstream stream;
        stream << base_id << "_" << index;
        if (!repository.exists(stream.str())) {
            return stream.str();
        }
    }

    return base_id;
}

OperationService::OperationService(Repository& repository, const OperationFactory& factory)
    : repository_(repository),
      factory_(factory)
{
}

bool OperationService::createOperation(
    const CreateOperationRequest& request,
    CamObject& operation,
    std::string& error_code,
    std::string& message) const
{
    return factory_.create(repository_, request, operation, error_code, message);
}

bool OperationService::updateOperationParameter(
    const std::string& operation_object_id,
    const std::string& parameter_name,
    const std::string& parameter_value,
    std::string& error_code,
    std::string& message) const
{
    CamObject operation = repository_.get(operation_object_id);
    if (operation.object_type != ObjectType::Operation) {
        error_code = "invalid_target_type";
        message = "Operation parameter update requires an operation target.";
        return false;
    }

    if (parameter_name.empty()) {
        error_code = "missing_parameter_name";
        message = "Parameter name is required.";
        return false;
    }

    operation.attributes[parameter_name] = parameter_value;
    operation.attributes["toolpath_status"] = "needs_recalculate";
    return repository_.update(operation);
}

ToolPathService::ToolPathService(Repository& repository)
    : repository_(repository)
{
}

bool ToolPathService::generateToolPath(
    const std::string& operation_object_id,
    CamObject& toolpath,
    std::string& error_code,
    std::string& message) const
{
    const CamObject operation = repository_.get(operation_object_id);
    if (operation.object_type != ObjectType::Operation) {
        error_code = "invalid_target_type";
        message = "Toolpath generation requires an operation target.";
        return false;
    }

    const std::string toolpath_id = "toolpath_" + operation_object_id;
    if (repository_.exists(toolpath_id)) {
        error_code = "duplicate_object_id";
        message = "Toolpath object ID already exists.";
        toolpath = repository_.get(toolpath_id);
        return false;
    }

    std::string toolpath_name = "刀轨";
    const std::map<std::string, std::string>::const_iterator operation_type = operation.attributes.find("operation_type");
    if (operation_type != operation.attributes.end()) {
        if (operation_type->second == "drilling") {
            toolpath_name = "钻孔刀轨";
        } else if (operation_type->second == "finishing") {
            toolpath_name = "平面铣刀轨";
        } else {
            toolpath_name = "型腔铣刀轨";
        }
    }

    toolpath = CamObject(toolpath_id, ObjectType::Toolpath, toolpath_name);
    toolpath.parent_object_id = operation_object_id;
    toolpath.attributes["visible"] = "true";
    if (!repository_.save(toolpath)) {
        error_code = "duplicate_object_id";
        message = "Toolpath object ID already exists.";
        return false;
    }

    CamObject updated_operation = operation;
    updated_operation.attributes["toolpath_status"] = "calculated";
    updated_operation.attributes["toolpath_id"] = toolpath_id;
    repository_.update(updated_operation);

    return true;
}

bool ToolPathService::deleteToolPathForOperation(const std::string& operation_object_id)
{
    const std::string toolpath_id = "toolpath_" + operation_object_id;
    const bool removed = repository_.remove(toolpath_id);
    CamObject operation = repository_.get(operation_object_id);
    if (operation.object_type == ObjectType::Operation) {
        operation.attributes["toolpath_status"] = "not_calculated";
        operation.attributes.erase("toolpath_id");
        repository_.update(operation);
    }
    return removed;
}

bool ToolPathService::setToolPathVisibility(
    const std::string& toolpath_object_id,
    bool visible,
    std::string& error_code,
    std::string& message)
{
    CamObject toolpath = repository_.get(toolpath_object_id);
    if (toolpath.object_type != ObjectType::Toolpath) {
        error_code = "invalid_target_type";
        message = "Toolpath visibility requires a toolpath target.";
        return false;
    }

    toolpath.attributes["visible"] = visible ? "true" : "false";
    return repository_.update(toolpath);
}

bool ToolPathService::toggleToolPathVisibility(
    const std::string& toolpath_object_id,
    bool& visible,
    std::string& error_code,
    std::string& message)
{
    CamObject toolpath = repository_.get(toolpath_object_id);
    if (toolpath.object_type != ObjectType::Toolpath) {
        error_code = "invalid_target_type";
        message = "Toolpath visibility requires a toolpath target.";
        return false;
    }

    const std::map<std::string, std::string>::const_iterator current = toolpath.attributes.find("visible");
    visible = current != toolpath.attributes.end() && current->second == "false";
    toolpath.attributes["visible"] = visible ? "true" : "false";
    return repository_.update(toolpath);
}

std::vector<std::string> ToolPathService::setAllToolPathVisibility(bool visible)
{
    std::vector<std::string> changed_ids;
    const std::vector<CamObject> toolpaths = repository_.objectsByType(ObjectType::Toolpath);
    for (std::size_t index = 0; index < toolpaths.size(); ++index) {
        CamObject toolpath = toolpaths[index];
        toolpath.attributes["visible"] = visible ? "true" : "false";
        if (repository_.update(toolpath)) {
            changed_ids.push_back(toolpath.object_id);
        }
    }
    return changed_ids;
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

        OperationCatalog catalog = OperationCatalog::defaults();
        OperationFactory factory(catalog);
        OperationService operation_service(repository_, factory);
        ToolPathService toolpath_service(repository_);
        CreateOperationRequest create_request;
        create_request.target_object_id = request.target_object_id;
        create_request.operation_type = request.operation_type.empty() ? "roughing" : request.operation_type;
        if (!request.tool_id.empty()) {
            create_request.parameters["tool_id"] = request.tool_id;
        }
        if (!request.stepover.empty()) {
            create_request.parameters["stepover"] = request.stepover;
        }
        if (!request.stepdown.empty()) {
            create_request.parameters["stepdown"] = request.stepdown;
        }
        if (!request.tolerance.empty()) {
            create_request.parameters["tolerance"] = request.tolerance;
        }

        CamObject operation;
        std::string error_code;
        std::string message;
        result.trace_events.push_back("gateway_validated");
        result.trace_events.push_back("adapter_dispatched");
        if (!operation_service.createOperation(create_request, operation, error_code, message)) {
            result.status = WorkflowStatus::Failed;
            result.error_code = error_code;
            result.message = message;
            return result;
        }

        CamObject toolpath;
        if (!toolpath_service.generateToolPath(operation.object_id, toolpath, error_code, message)) {
            result.status = WorkflowStatus::Failed;
            result.error_code = error_code;
            result.message = message;
            result.primary_object_id = operation.object_id;
            result.created_object_ids.push_back(operation.object_id);
            return result;
        }

        result.status = WorkflowStatus::Completed;
        result.primary_object_id = toolpath.object_id;
        result.created_object_ids.push_back(operation.object_id);
        result.created_object_ids.push_back(toolpath.object_id);
        result.trace_events.push_back("operation_created");
        result.trace_events.push_back("toolpath_generated");
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
