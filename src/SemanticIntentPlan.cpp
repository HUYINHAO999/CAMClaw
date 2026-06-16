#include "camclaw/SemanticIntentPlan.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace camclaw {

namespace {

std::string trim(const std::string& value)
{
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos) {
        return std::string();
    }
    return value.substr(first, last - first + 1u);
}

bool findJsonStringValue(const std::string& json, const std::string& key, std::string& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::string::size_type key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }
    const std::string::size_type colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) {
        return false;
    }
    std::string::size_type quote_pos = json.find('"', colon_pos + 1u);
    if (quote_pos == std::string::npos) {
        return false;
    }
    ++quote_pos;
    std::ostringstream stream;
    bool escaped = false;
    for (std::string::size_type index = quote_pos; index < json.size(); ++index) {
        const char ch = json[index];
        if (escaped) {
            stream << ch;
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            value = stream.str();
            return true;
        }
        stream << ch;
    }
    return false;
}

bool findJsonBoolValue(const std::string& json, const std::string& key, bool& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::string::size_type key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }
    const std::string::size_type colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) {
        return false;
    }
    const std::string tail = json.substr(colon_pos + 1u);
    const std::string cleaned = trim(tail);
    if (cleaned.find("true") == 0u) {
        value = true;
        return true;
    }
    if (cleaned.find("false") == 0u) {
        value = false;
        return true;
    }
    return false;
}

bool findJsonIntValue(const std::string& json, const std::string& key, int& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::string::size_type key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }
    const std::string::size_type colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) {
        return false;
    }
    std::string::size_type first = json.find_first_of("-0123456789", colon_pos + 1u);
    if (first == std::string::npos) {
        return false;
    }
    std::string::size_type last = first;
    while (last < json.size() && (json[last] == '-' || (json[last] >= '0' && json[last] <= '9'))) {
        ++last;
    }
    value = std::atoi(json.substr(first, last - first).c_str());
    return true;
}

std::string objectForKey(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const std::string::size_type key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return std::string();
    }
    const std::string::size_type colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) {
        return std::string();
    }
    const std::string::size_type start = json.find('{', colon_pos + 1u);
    if (start == std::string::npos) {
        return std::string();
    }
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::string::size_type index = start; index < json.size(); ++index) {
        const char ch = json[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
        } else if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) {
                return json.substr(start, index - start + 1u);
            }
        }
    }
    return std::string();
}

std::string arrayForKey(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const std::string::size_type key_pos = json.find(needle);
    if (key_pos == std::string::npos) {
        return std::string();
    }
    const std::string::size_type colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos) {
        return std::string();
    }
    const std::string::size_type start = json.find('[', colon_pos + 1u);
    if (start == std::string::npos) {
        return std::string();
    }
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::string::size_type index = start; index < json.size(); ++index) {
        const char ch = json[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
        } else if (ch == '[') {
            ++depth;
        } else if (ch == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(start, index - start + 1u);
            }
        }
    }
    return std::string();
}

std::vector<std::string> objectArrayItems(const std::string& array_json)
{
    std::vector<std::string> result;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    std::string::size_type start = std::string::npos;
    for (std::string::size_type index = 0; index < array_json.size(); ++index) {
        const char ch = array_json[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }
        if (ch == '"') {
            in_string = true;
        } else if (ch == '{') {
            if (depth == 0) {
                start = index;
            }
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0 && start != std::string::npos) {
                result.push_back(array_json.substr(start, index - start + 1u));
                start = std::string::npos;
            }
        }
    }
    return result;
}

std::vector<std::string> stringArrayItems(const std::string& array_json)
{
    std::vector<std::string> result;
    bool in_string = false;
    bool escaped = false;
    std::ostringstream stream;
    for (std::string::size_type index = 0; index < array_json.size(); ++index) {
        const char ch = array_json[index];
        if (!in_string) {
            if (ch == '"') {
                in_string = true;
                stream.str("");
                stream.clear();
            }
            continue;
        }
        if (escaped) {
            stream << ch;
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '"') {
            in_string = false;
            result.push_back(stream.str());
        } else {
            stream << ch;
        }
    }
    return result;
}

SemanticIntentKind intentKindFromString(const std::string& intent)
{
    if (intent == "machine_feature") {
        return SemanticIntentKind::MachineFeature;
    }
    if (intent == "create_operations") {
        return SemanticIntentKind::CreateOperations;
    }
    if (intent == "edit_operation") {
        return SemanticIntentKind::EditOperation;
    }
    if (intent == "regenerate_toolpath") {
        return SemanticIntentKind::RegenerateToolpath;
    }
    if (intent == "set_toolpath_visibility") {
        return SemanticIntentKind::SetToolpathVisibility;
    }
    return SemanticIntentKind::Unknown;
}

bool objectTypeMatches(ObjectType type, const std::string& expected)
{
    return (expected == "feature" && type == ObjectType::Feature)
        || (expected == "operation" && type == ObjectType::Operation)
        || (expected == "toolpath" && type == ObjectType::Toolpath);
}

bool operationTypeMatchesFilter(const std::string& operation_type, const std::string& filter)
{
    if (filter == "pocket") {
        return operation_type == "pocket" || operation_type == "roughing" || operation_type == "pocket_roughing";
    }
    return operation_type == filter;
}

std::string formatNumber(double value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

} // namespace

SemanticPlanDraft::SemanticPlanDraft(const std::string& trace_id)
    : trace_id_(trace_id),
      status_(SemanticDraftStatus::PendingReview)
{
}

const std::string& SemanticPlanDraft::traceId() const
{
    return trace_id_;
}

SemanticDraftStatus SemanticPlanDraft::status() const
{
    return status_;
}

void SemanticPlanDraft::setStatus(SemanticDraftStatus status)
{
    status_ = status;
}

void SemanticPlanDraft::confirm()
{
    if (status_ == SemanticDraftStatus::PendingReview) {
        status_ = SemanticDraftStatus::Confirmed;
        trace_events_.push_back("semantic_draft_confirmed");
    }
}

void SemanticPlanDraft::reject()
{
    if (status_ == SemanticDraftStatus::PendingReview) {
        status_ = SemanticDraftStatus::Rejected;
        trace_events_.push_back("semantic_draft_rejected");
    }
}

void SemanticPlanDraft::addIntent(const SemanticIntent& intent)
{
    intents_.push_back(intent);
}

std::size_t SemanticPlanDraft::intentCount() const
{
    return intents_.size();
}

const SemanticIntent& SemanticPlanDraft::intentAt(std::size_t index) const
{
    return intents_[index];
}

const std::vector<std::string>& SemanticPlanDraft::traceEvents() const
{
    return trace_events_;
}

void SemanticPlanDraft::addTraceEvent(const std::string& event)
{
    trace_events_.push_back(event);
}

SemanticPlanDraftDecodeResult SemanticPlanDraftJsonCodec::decodeBackendDraft(const std::string& json) const
{
    SemanticPlanDraftDecodeResult result;
    std::string trace_id;
    findJsonStringValue(json, "trace_id", trace_id);
    result.draft = SemanticPlanDraft(trace_id);

    std::string schema_version;
    if (!findJsonStringValue(json, "schema_version", schema_version) || schema_version != "1") {
        result.error_code = "invalid_semantic_draft_json";
        result.message = "Semantic draft JSON requires schema_version 1.";
        return result;
    }

    const std::string intents_array = arrayForKey(json, "intents");
    const std::vector<std::string> intent_objects = objectArrayItems(intents_array);
    if (intent_objects.empty()) {
        result.error_code = "invalid_semantic_draft_json";
        result.message = "Semantic draft JSON requires at least one intent.";
        return result;
    }

    for (std::size_t index = 0; index < intent_objects.size(); ++index) {
        SemanticIntent intent;
        if (!decodeIntent(intent_objects[index], intent, result.message)) {
            result.error_code = "invalid_semantic_intent";
            return result;
        }
        result.draft.addIntent(intent);
    }

    result.draft.addTraceEvent("semantic_draft_created");
    result.ok = true;
    return result;
}

bool SemanticPlanDraftJsonCodec::decodeIntent(const std::string& object_json, SemanticIntent& intent, std::string& message) const
{
    if (!findJsonStringValue(object_json, "id", intent.id)) {
        message = "Semantic intent requires id.";
        return false;
    }
    std::string intent_name;
    if (!findJsonStringValue(object_json, "intent", intent_name)) {
        message = "Semantic intent requires intent.";
        return false;
    }
    intent.kind = intentKindFromString(intent_name);
    if (intent.kind == SemanticIntentKind::Unknown) {
        message = "Unsupported semantic intent: " + intent_name;
        return false;
    }

    bool bool_value = false;
    if (findJsonBoolValue(object_json, "auto_generate_toolpath", bool_value)) {
        intent.auto_generate_toolpath = bool_value;
    }
    if (findJsonBoolValue(object_json, "open_editor", bool_value)) {
        intent.open_editor = bool_value;
    }

    const std::string target_json = objectForKey(object_json, "target");
    if (!target_json.empty() && !decodeTarget(target_json, intent.target, message)) {
        return false;
    }

    const std::vector<std::string> create_items = objectArrayItems(arrayForKey(object_json, "items"));
    for (std::size_t index = 0; index < create_items.size(); ++index) {
        OperationCreateItem item;
        findJsonStringValue(create_items[index], "operation_type", item.operation_type);
        findJsonIntValue(create_items[index], "count", item.count);
        intent.create_items.push_back(item);
    }

    const std::vector<std::string> updates = objectArrayItems(arrayForKey(object_json, "updates"));
    for (std::size_t index = 0; index < updates.size(); ++index) {
        ParameterUpdateIntent update;
        findJsonStringValue(updates[index], "parameter", update.parameter);
        findJsonStringValue(updates[index], "value", update.value);
        findJsonStringValue(updates[index], "expression", update.expression);
        intent.updates.push_back(update);
    }

    const std::vector<std::string> actions = objectArrayItems(arrayForKey(object_json, "actions"));
    for (std::size_t index = 0; index < actions.size(); ++index) {
        VisibilityIntentAction action;
        findJsonStringValue(actions[index], "visibility", action.visibility);
        const std::string action_target = objectForKey(actions[index], "target");
        if (!decodeTarget(action_target, action.target, message)) {
            return false;
        }
        intent.visibility_actions.push_back(action);
    }

    return true;
}

bool SemanticPlanDraftJsonCodec::decodeTarget(const std::string& object_json, SemanticTarget& target, std::string& message) const
{
    std::string kind;
    if (!findJsonStringValue(object_json, "kind", kind)) {
        message = "Target requires kind.";
        return false;
    }
    if (kind == "objects") {
        target.kind = SemanticTargetKind::Objects;
        target.object_ids = stringArrayItems(arrayForKey(object_json, "object_ids"));
        if (target.object_ids.empty()) {
            message = "Objects target requires object_ids.";
            return false;
        }
        return true;
    }
    if (kind == "query") {
        target.kind = SemanticTargetKind::Query;
        findJsonStringValue(object_json, "object_type", target.object_type);
        findJsonStringValue(object_json, "scope", target.scope);
        const std::string filters = objectForKey(object_json, "filters");
        std::string operation_type;
        if (findJsonStringValue(filters, "operation_type", operation_type)) {
            target.filters["operation_type"] = operation_type;
        }
        return true;
    }
    if (kind == "ref") {
        target.kind = SemanticTargetKind::Ref;
        findJsonStringValue(object_json, "ref", target.ref);
        return true;
    }
    message = "Unsupported target kind: " + kind;
    return false;
}

SemanticIntentExecutor::SemanticIntentExecutor(Repository& repository, HumanInLoopService& human_in_loop_service)
    : repository_(repository),
      human_in_loop_service_(human_in_loop_service),
      feature_recognition_service_(),
      tool_selection_policy_(),
      parameter_expression_resolver_(tool_selection_policy_)
{
}

AgentPlanExecutionResult SemanticIntentExecutor::execute(const SemanticPlanDraft& draft)
{
    AgentPlanExecutionResult result;
    if (draft.status() != SemanticDraftStatus::Confirmed) {
        result.error_code = "draft_not_confirmed";
        result.message = "Semantic draft must be confirmed before execution.";
        return result;
    }

    IntentExecutionContext context;
    for (std::size_t index = 0; index < draft.intentCount(); ++index) {
        if (!executeIntent(draft.intentAt(index), context, result)) {
            return result;
        }
    }

    result.ok = true;
    result.trace_events.push_back("semantic_execution_completed");
    return result;
}

bool SemanticIntentExecutor::executeIntent(
    const SemanticIntent& intent,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result)
{
    if (intent.kind == SemanticIntentKind::MachineFeature) {
        return executeMachineFeature(intent, context, result);
    }
    if (intent.kind == SemanticIntentKind::CreateOperations) {
        return executeCreateOperations(intent, context, result);
    }
    if (intent.kind == SemanticIntentKind::EditOperation) {
        return executeEditOperation(intent, context, result);
    }
    if (intent.kind == SemanticIntentKind::RegenerateToolpath) {
        return executeRegenerateToolpath(intent, context, result);
    }
    if (intent.kind == SemanticIntentKind::SetToolpathVisibility) {
        return executeSetToolpathVisibility(intent, context, result);
    }
    result.error_code = "unsupported_intent";
    result.message = "Unsupported semantic intent.";
    return false;
}

bool SemanticIntentExecutor::executeMachineFeature(
    const SemanticIntent& intent,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result)
{
    const std::vector<std::string> feature_ids = resolveTargetIds(intent.target, context, result, "feature");
    if (!result.ok && !result.error_code.empty()) {
        return false;
    }

    OperationCatalog catalog = OperationCatalog::defaults();
    OperationFactory factory(catalog);
    OperationService operation_service(repository_, factory);
    ToolPathService toolpath_service(repository_);

    std::vector<std::string> created_ids;
    for (std::size_t index = 0; index < feature_ids.size(); ++index) {
        const CamObject feature = repository_.get(feature_ids[index]);
        CreateOperationRequest request;
        request.target_object_id = feature.object_id;
        request.operation_type = feature_recognition_service_.recognizeOperationType(feature);
        CamObject operation;
        std::string error_code;
        std::string message;
        if (!operation_service.createOperation(request, operation, error_code, message)) {
            result.error_code = error_code;
            result.message = message;
            return false;
        }
        created_ids.push_back(operation.object_id);
        if (intent.auto_generate_toolpath) {
            CamObject toolpath;
            if (!toolpath_service.generateToolPath(operation.object_id, toolpath, error_code, message)) {
                result.error_code = error_code;
                result.message = message;
                return false;
            }
            created_ids.push_back(toolpath.object_id);
        }
    }

    appendSuccess(created_ids.empty() ? std::string() : created_ids[0], created_ids, "machine_feature_completed", context, result);
    return true;
}

bool SemanticIntentExecutor::executeCreateOperations(
    const SemanticIntent& intent,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result)
{
    OperationCatalog catalog = OperationCatalog::defaults();
    OperationFactory factory(catalog);
    OperationService operation_service(repository_, factory);
    std::vector<std::string> created_ids;
    std::string target_object_id;
    if (intent.target.kind != SemanticTargetKind::Unknown) {
        const std::vector<std::string> targets = resolveTargetIds(intent.target, context, result, "feature");
        if (!result.ok && !result.error_code.empty()) {
            return false;
        }
        if (!targets.empty()) {
            target_object_id = targets[0];
        }
    }

    for (std::size_t item_index = 0; item_index < intent.create_items.size(); ++item_index) {
        const OperationCreateItem& item = intent.create_items[item_index];
        for (int count = 0; count < item.count; ++count) {
            CreateOperationRequest request;
            request.operation_type = item.operation_type;
            request.target_object_id = target_object_id;
            CamObject operation;
            std::string error_code;
            std::string message;
            if (!operation_service.createOperation(request, operation, error_code, message)) {
                result.error_code = error_code;
                result.message = message;
                return false;
            }
            created_ids.push_back(operation.object_id);
        }
    }

    appendSuccess(created_ids.empty() ? std::string() : created_ids[0], created_ids, "operations_created", context, result);
    return true;
}

bool SemanticIntentExecutor::executeEditOperation(
    const SemanticIntent& intent,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result)
{
    const std::vector<std::string> operation_ids = resolveTargetIds(intent.target, context, result, "operation");
    if (!result.ok && !result.error_code.empty()) {
        return false;
    }
    OperationCatalog catalog = OperationCatalog::defaults();
    OperationFactory factory(catalog);
    OperationService operation_service(repository_, factory);

    for (std::size_t index = 0; index < operation_ids.size(); ++index) {
        for (std::size_t update_index = 0; update_index < intent.updates.size(); ++update_index) {
            const CamObject operation = repository_.get(operation_ids[index]);
            const std::string parameter_value = parameter_expression_resolver_.resolve(operation, intent.updates[update_index]);
            std::string parameter_name = intent.updates[update_index].parameter;
            if (parameter_name == "tool") {
                parameter_name = "tool_id";
            }
            std::string error_code;
            std::string message;
            if (!operation_service.updateOperationParameter(operation_ids[index], parameter_name, parameter_value, error_code, message)) {
                result.error_code = error_code;
                result.message = message;
                return false;
            }
        }
    }

    appendSuccess(operation_ids.empty() ? std::string() : operation_ids[0], operation_ids, "operation_edited", context, result);
    return true;
}

bool SemanticIntentExecutor::executeRegenerateToolpath(
    const SemanticIntent& intent,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result)
{
    const std::vector<std::string> operation_ids = resolveTargetIds(intent.target, context, result, "operation");
    if (!result.ok && !result.error_code.empty()) {
        return false;
    }
    ToolPathService toolpath_service(repository_);
    std::vector<std::string> object_ids;
    for (std::size_t index = 0; index < operation_ids.size(); ++index) {
        toolpath_service.deleteToolPathForOperation(operation_ids[index]);
        CamObject toolpath;
        std::string error_code;
        std::string message;
        if (!toolpath_service.generateToolPath(operation_ids[index], toolpath, error_code, message)) {
            result.error_code = error_code;
            result.message = message;
            return false;
        }
        object_ids.push_back(toolpath.object_id);
    }
    appendSuccess(object_ids.empty() ? std::string() : object_ids[0], object_ids, "toolpath_regenerated", context, result);
    return true;
}

bool SemanticIntentExecutor::executeSetToolpathVisibility(
    const SemanticIntent& intent,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result)
{
    ToolPathService toolpath_service(repository_);
    std::vector<std::string> changed_ids;
    for (std::size_t action_index = 0; action_index < intent.visibility_actions.size(); ++action_index) {
        const VisibilityIntentAction& action = intent.visibility_actions[action_index];
        const std::vector<std::string> toolpath_ids = resolveTargetIds(action.target, context, result, "toolpath");
        if (!result.ok && !result.error_code.empty()) {
            return false;
        }
        for (std::size_t index = 0; index < toolpath_ids.size(); ++index) {
            bool visible = false;
            std::string error_code;
            std::string message;
            bool ok = false;
            if (action.visibility == "toggle") {
                ok = toolpath_service.toggleToolPathVisibility(toolpath_ids[index], visible, error_code, message);
            } else {
                visible = action.visibility == "show";
                ok = toolpath_service.setToolPathVisibility(toolpath_ids[index], visible, error_code, message);
            }
            if (!ok) {
                result.error_code = error_code;
                result.message = message;
                return false;
            }
            changed_ids.push_back(toolpath_ids[index]);
        }
    }
    appendSuccess(changed_ids.empty() ? std::string() : changed_ids[0], changed_ids, "toolpath_visibility_updated", context, result);
    return true;
}

std::vector<std::string> SemanticIntentExecutor::resolveTargetIds(
    const SemanticTarget& target,
    const IntentExecutionContext& context,
    AgentPlanExecutionResult& result,
    const std::string& expected_object_type) const
{
    result.ok = false;
    std::vector<std::string> ids;
    if (target.kind == SemanticTargetKind::Objects) {
        for (std::size_t index = 0; index < target.object_ids.size(); ++index) {
            const CamObject object = repository_.get(target.object_ids[index]);
            if (!objectTypeMatches(object.object_type, expected_object_type)) {
                result.error_code = "invalid_target_type";
                result.message = "Semantic target object type does not match intent.";
                return std::vector<std::string>();
            }
            ids.push_back(object.object_id);
        }
        result.ok = true;
        return ids;
    }
    if (target.kind == SemanticTargetKind::Ref) {
        if (target.ref != "previous.primary_object_ids" || context.previous_primary_object_ids.empty()) {
            result.error_code = "missing_ref_target";
            result.message = "Semantic ref target could not be resolved.";
            return std::vector<std::string>();
        }
        for (std::size_t index = 0; index < context.previous_primary_object_ids.size(); ++index) {
            const CamObject object = repository_.get(context.previous_primary_object_ids[index]);
            if (objectTypeMatches(object.object_type, expected_object_type)) {
                ids.push_back(object.object_id);
            }
        }
        if (ids.empty()) {
            result.error_code = "invalid_ref_target";
            result.message = "Semantic ref target did not resolve to the expected object type.";
            return ids;
        }
        result.ok = true;
        return ids;
    }
    if (target.kind == SemanticTargetKind::Query) {
        if (target.scope == "all") {
            const ObjectType type = expected_object_type == "operation" ? ObjectType::Operation
                : expected_object_type == "toolpath" ? ObjectType::Toolpath
                : ObjectType::Feature;
            const std::vector<CamObject> objects = repository_.objectsByType(type);
            for (std::size_t index = 0; index < objects.size(); ++index) {
                ids.push_back(objects[index].object_id);
            }
            result.ok = true;
            return ids;
        }
        const std::map<std::string, std::string>::const_iterator operation_type_filter = target.filters.find("operation_type");
        if (operation_type_filter != target.filters.end()) {
            const std::vector<CamObject> operations = repository_.objectsByType(ObjectType::Operation);
            for (std::size_t index = 0; index < operations.size(); ++index) {
                const std::map<std::string, std::string>::const_iterator operation_type = operations[index].attributes.find("operation_type");
                if (operation_type == operations[index].attributes.end()
                    || !operationTypeMatchesFilter(operation_type->second, operation_type_filter->second)) {
                    continue;
                }
                if (expected_object_type == "operation") {
                    ids.push_back(operations[index].object_id);
                } else if (expected_object_type == "toolpath") {
                    const std::map<std::string, std::string>::const_iterator toolpath_id = operations[index].attributes.find("toolpath_id");
                    if (toolpath_id != operations[index].attributes.end()) {
                        ids.push_back(toolpath_id->second);
                    } else {
                        const std::vector<CamObject> toolpaths = repository_.objectsByType(ObjectType::Toolpath);
                        for (std::size_t toolpath_index = 0; toolpath_index < toolpaths.size(); ++toolpath_index) {
                            if (toolpaths[toolpath_index].parent_object_id == operations[index].object_id) {
                                ids.push_back(toolpaths[toolpath_index].object_id);
                            }
                        }
                    }
                }
            }
        }
        if (ids.empty()) {
            result.error_code = "target_not_found";
            result.message = "Semantic query target did not match any objects.";
            return ids;
        }
        result.ok = true;
        return ids;
    }
    result.error_code = "missing_target";
    result.message = "Semantic intent target is missing.";
    return ids;
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
    if (expression == "larger" || expression == "大一些") {
        if (current != operation.attributes.end() && current->second == "tool_010") {
            return "tool_016";
        }
        if (current != operation.attributes.end()) {
            return current->second;
        }
    }
    return current == operation.attributes.end() ? std::string() : current->second;
}

ParameterExpressionResolver::ParameterExpressionResolver(const ToolSelectionPolicy& tool_selection_policy)
    : tool_selection_policy_(tool_selection_policy)
{
}

std::string ParameterExpressionResolver::resolve(const CamObject& operation, const ParameterUpdateIntent& update) const
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
        return expression;
    }
    const std::map<std::string, std::string>::const_iterator current = operation.attributes.find(parameter_name);
    if (current == operation.attributes.end()) {
        return expression;
    }
    char* end = 0;
    const double parsed = std::strtod(current->second.c_str(), &end);
    if (end == current->second.c_str() || *end != '\0' || parsed <= 0.0) {
        return expression;
    }
    return formatNumber(parsed * 0.5);
}

void SemanticIntentExecutor::appendSuccess(
    const std::string& primary_object_id,
    const std::vector<std::string>& object_ids,
    const std::string& trace_event,
    IntentExecutionContext& context,
    AgentPlanExecutionResult& result) const
{
    result.primary_object_id = primary_object_id;
    result.object_ids.insert(result.object_ids.end(), object_ids.begin(), object_ids.end());
    result.trace_events.push_back(trace_event);
    context.previous_primary_object_ids = object_ids;
}

} // namespace camclaw
