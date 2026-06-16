#include "camclaw/BrowserConsole.h"

#include <sstream>

namespace camclaw {

static std::vector<std::string> availableToolIdsForOperation(const CamObject& operation)
{
    std::vector<std::string> tools;
    const std::map<std::string, std::string>::const_iterator operation_type = operation.attributes.find("operation_type");
    if (operation_type != operation.attributes.end() && operation_type->second == "drilling") {
        tools.push_back("drill_004");
        tools.push_back("drill_006");
        tools.push_back("drill_008");
        return tools;
    }

    tools.push_back("tool_006");
    tools.push_back("tool_010");
    tools.push_back("tool_016");
    return tools;
}

static std::string defaultToolIdForOperation(const CamObject& operation)
{
    const std::map<std::string, std::string>::const_iterator operation_type = operation.attributes.find("operation_type");
    if (operation_type != operation.attributes.end() && operation_type->second == "drilling") {
        return "drill_006";
    }
    return "tool_010";
}

static std::string adjacentToolId(const CamObject& operation, const std::string& current_tool_id, int direction)
{
    const std::vector<std::string> tools = availableToolIdsForOperation(operation);
    if (tools.empty()) {
        return current_tool_id;
    }

    std::size_t current_index = tools.size();
    for (std::size_t index = 0; index < tools.size(); ++index) {
        if (tools[index] == current_tool_id) {
            current_index = index;
            break;
        }
    }
    if (current_index == tools.size()) {
        return defaultToolIdForOperation(operation);
    }

    if (direction > 0 && current_index + 1u < tools.size()) {
        return tools[current_index + 1u];
    }
    if (direction < 0 && current_index > 0u) {
        return tools[current_index - 1u];
    }
    return tools[current_index];
}

static bool isLargerToolExpression(const std::string& expression)
{
    return expression == "larger"
        || expression == "larger_available_tool"
        || expression == "larger_tool"
        || expression == "大一些";
}

static bool isSmallerToolExpression(const std::string& expression)
{
    return expression == "smaller"
        || expression == "smaller_available_tool"
        || expression == "smaller_tool"
        || expression == "小一些"
        || expression == "更小";
}

BrowserConsole::BrowserConsole(Repository& repository, BrowserConsoleUi* ui)
    : repository_(repository),
      ui_(ui),
      parameter_expression_resolver_(tool_selection_policy_)
{
}

Repository& BrowserConsole::repository()
{
    return repository_;
}

const Repository& BrowserConsole::repository() const
{
    return repository_;
}

void BrowserConsole::setUi(BrowserConsoleUi* ui)
{
    ui_ = ui;
}

BrowserConsoleResult BrowserConsole::machineFeature(
    const std::vector<std::string>& feature_ids,
    bool auto_generate_toolpath)
{
    BrowserConsoleResult result;
    OperationCatalog catalog = OperationCatalog::defaults();
    OperationFactory factory(catalog);
    OperationService operation_service(repository_, factory);
    ToolPathService toolpath_service(repository_);

    for (std::size_t index = 0; index < feature_ids.size(); ++index) {
        const CamObject feature = repository_.get(feature_ids[index]);
        if (feature.object_type != ObjectType::Feature) {
            result.error_code = "invalid_target_type";
            result.message = "machineFeature requires feature targets.";
            return result;
        }
        CreateOperationRequest request;
        request.target_object_id = feature.object_id;
        request.operation_type = feature_recognition_service_.recognizeOperationType(feature);
        CamObject operation;
        if (!operation_service.createOperation(request, operation, result.error_code, result.message)) {
            return result;
        }
        appendObject(result, operation.object_id);
        if (auto_generate_toolpath) {
            CamObject toolpath;
            if (!toolpath_service.generateToolPath(operation.object_id, toolpath, result.error_code, result.message)) {
                return result;
            }
            appendObject(result, toolpath.object_id);
        }
    }

    result.ok = true;
    result.trace_events.push_back("machine_feature_completed");
    refreshAndSelect(result);
    return result;
}

BrowserConsoleResult BrowserConsole::createOperations(
    const std::vector<BrowserOperationCreateItem>& items,
    const std::string& target_feature_id,
    bool auto_generate_toolpath)
{
    BrowserConsoleResult result;
    OperationCatalog catalog = OperationCatalog::defaults();
    OperationFactory factory(catalog);
    OperationService operation_service(repository_, factory);
    ToolPathService toolpath_service(repository_);

    for (std::size_t item_index = 0; item_index < items.size(); ++item_index) {
        for (int count = 0; count < items[item_index].count; ++count) {
            CreateOperationRequest request;
            request.operation_type = items[item_index].operation_type;
            request.target_object_id = target_feature_id;
            CamObject operation;
            if (!operation_service.createOperation(request, operation, result.error_code, result.message)) {
                return result;
            }
            appendObject(result, operation.object_id);
            if (auto_generate_toolpath) {
                CamObject toolpath;
                if (!toolpath_service.generateToolPath(operation.object_id, toolpath, result.error_code, result.message)) {
                    return result;
                }
                appendObject(result, toolpath.object_id);
            }
        }
    }

    result.ok = true;
    result.trace_events.push_back("operations_created");
    refreshAndSelect(result);
    return result;
}

BrowserConsoleResult BrowserConsole::updateOperations(
    const std::vector<std::string>& operation_ids,
    const std::vector<BrowserParameterUpdate>& updates,
    bool open_editor)
{
    BrowserConsoleResult result;
    OperationCatalog catalog = OperationCatalog::defaults();
    OperationFactory factory(catalog);
    OperationService operation_service(repository_, factory);

    for (std::size_t index = 0; index < operation_ids.size(); ++index) {
        for (std::size_t update_index = 0; update_index < updates.size(); ++update_index) {
            const CamObject operation = repository_.get(operation_ids[index]);
            BrowserParameterUpdate update = updates[update_index];
            std::string parameter_name = update.parameter == "tool" ? std::string("tool_id") : update.parameter;
            const std::string parameter_value = parameter_expression_resolver_.resolve(operation, update);
            if (!operation_service.updateOperationParameter(operation_ids[index], parameter_name, parameter_value, result.error_code, result.message)) {
                return result;
            }
        }
        appendObject(result, operation_ids[index]);
        if (open_editor && ui_ != 0) {
            ui_->openOperationEditor(operation_ids[index]);
        }
    }

    result.ok = true;
    result.trace_events.push_back("operation_edited");
    refreshAndSelect(result);
    return result;
}

BrowserConsoleResult BrowserConsole::generateToolpaths(const std::vector<std::string>& operation_ids)
{
    BrowserConsoleResult result;
    ToolPathService toolpath_service(repository_);
    for (std::size_t index = 0; index < operation_ids.size(); ++index) {
        CamObject toolpath;
        if (!toolpath_service.generateToolPath(operation_ids[index], toolpath, result.error_code, result.message)) {
            return result;
        }
        appendObject(result, toolpath.object_id);
    }
    result.ok = true;
    result.trace_events.push_back("toolpath_generated");
    refreshAndSelect(result);
    return result;
}

BrowserConsoleResult BrowserConsole::regenerateToolpaths(const std::vector<std::string>& operation_ids)
{
    ToolPathService toolpath_service(repository_);
    for (std::size_t index = 0; index < operation_ids.size(); ++index) {
        toolpath_service.deleteToolPathForOperation(operation_ids[index]);
    }
    BrowserConsoleResult result = generateToolpaths(operation_ids);
    if (result.ok) {
        result.trace_events.push_back("toolpath_regenerated");
    }
    return result;
}

BrowserConsoleResult BrowserConsole::deleteToolpathForOperation(const std::string& operation_id)
{
    BrowserConsoleResult result;
    ToolPathService toolpath_service(repository_);
    toolpath_service.deleteToolPathForOperation(operation_id);
    appendObject(result, operation_id);
    result.ok = true;
    result.trace_events.push_back("toolpath_deleted");
    refreshAndSelect(result);
    return result;
}

BrowserConsoleResult BrowserConsole::setToolpathVisibility(
    const std::vector<std::string>& toolpath_ids,
    const std::string& visibility)
{
    BrowserConsoleResult result;
    ToolPathService toolpath_service(repository_);
    for (std::size_t index = 0; index < toolpath_ids.size(); ++index) {
        bool visible = false;
        bool ok = false;
        if (visibility == "toggle") {
            ok = toolpath_service.toggleToolPathVisibility(toolpath_ids[index], visible, result.error_code, result.message);
        } else {
            visible = visibility == "show";
            ok = toolpath_service.setToolPathVisibility(toolpath_ids[index], visible, result.error_code, result.message);
        }
        if (!ok) {
            return result;
        }
        appendObject(result, toolpath_ids[index]);
    }
    result.ok = true;
    result.trace_events.push_back("toolpath_visibility_updated");
    refreshAndSelect(result);
    return result;
}

void BrowserConsole::refreshAndSelect(const BrowserConsoleResult& result)
{
    if (ui_ == 0) {
        return;
    }
    ui_->refreshBrowserTree();
    if (!result.primary_object_id.empty()) {
        ui_->selectObject(result.primary_object_id);
        const CamObject object = repository_.get(result.primary_object_id);
        if (object.object_type == ObjectType::Operation) {
            ui_->showOperationPreview(object.object_id);
        } else if (object.object_type == ObjectType::Toolpath) {
            ui_->showToolpathPreview(object.object_id);
        }
    }
    ui_->syncVisibleToolpaths();
}

void BrowserConsole::appendObject(BrowserConsoleResult& result, const std::string& object_id) const
{
    if (result.primary_object_id.empty()) {
        result.primary_object_id = object_id;
    }
    result.object_ids.push_back(object_id);
}

std::string FeatureRecognitionService::recognizeOperationType(const CamObject& feature) const
{
    const std::string text = feature.object_id + " " + feature.display_name;
    if (text.find("holes") != std::string::npos || text.find("孔") != std::string::npos) {
        return "drilling";
    }
    if (text.find("plane") != std::string::npos || text.find("平面") != std::string::npos) {
        return "finishing";
    }
    return "pocket";
}

std::string ToolSelectionPolicy::resolveRelativeToolId(const CamObject& operation, const std::string& expression) const
{
    const std::map<std::string, std::string>::const_iterator current = operation.attributes.find("tool_id");
    const std::string current_tool_id = current == operation.attributes.end()
        ? defaultToolIdForOperation(operation)
        : current->second;
    if (isLargerToolExpression(expression)) {
        return adjacentToolId(operation, current_tool_id, 1);
    }
    if (isSmallerToolExpression(expression)) {
        return adjacentToolId(operation, current_tool_id, -1);
    }
    return current_tool_id;
}

ParameterExpressionResolver::ParameterExpressionResolver(const ToolSelectionPolicy& tool_selection_policy)
    : tool_selection_policy_(tool_selection_policy)
{
}

std::string ParameterExpressionResolver::resolve(const CamObject& operation, const BrowserParameterUpdate& update) const
{
    if (!update.value.empty()) {
        return update.value;
    }
    if (update.parameter == "tool" || update.parameter == "tool_id") {
        return tool_selection_policy_.resolveRelativeToolId(operation, update.expression);
    }
    return resolveRelativeNumber(operation, update.parameter, update.expression);
}

std::string ParameterExpressionResolver::resolveRelativeNumber(
    const CamObject& operation,
    const std::string& parameter_name,
    const std::string& expression) const
{
    if (expression != "smaller" && expression != "更小" && expression != "小一些") {
        const std::map<std::string, std::string>::const_iterator current = operation.attributes.find(parameter_name);
        return current == operation.attributes.end() ? std::string() : current->second;
    }
    const std::map<std::string, std::string>::const_iterator current = operation.attributes.find(parameter_name);
    if (current == operation.attributes.end()) {
        return std::string();
    }
    std::istringstream stream(current->second);
    double value = 0.0;
    stream >> value;
    if (!stream.fail()) {
        std::ostringstream out;
        out << value * 0.5;
        return out.str();
    }
    return current->second;
}

} // namespace camclaw
