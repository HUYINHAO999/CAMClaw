#include "camclaw/SkillRuntime.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace camclaw {

namespace {

bool parsePositiveNumber(const std::string& value)
{
    if (value.empty()) {
        return false;
    }

    char* end = 0;
    const double parsed = std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && parsed > 0.0;
}

bool parseTolerance(const std::string& value)
{
    if (!parsePositiveNumber(value)) {
        return false;
    }

    char* end = 0;
    const double parsed = std::strtod(value.c_str(), &end);
    return parsed <= 1.0;
}

bool parseNonNegativeNumber(const std::string& value)
{
    if (value.empty()) {
        return false;
    }

    char* end = 0;
    const double parsed = std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0' && parsed >= 0.0;
}

std::vector<std::string> splitCsv(const std::string& value)
{
    std::vector<std::string> parts;
    std::istringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        const std::string::size_type first = item.find_first_not_of(" \t\r\n");
        const std::string::size_type last = item.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
            parts.push_back(item.substr(first, last - first + 1u));
        }
    }
    return parts;
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

std::string resolveRelativeParameterValue(
    const CamObject& operation,
    const std::string& parameter_name,
    const std::string& parameter_value)
{
    if (parameter_name != "stepover") {
        return parameter_value;
    }

    if (parameter_value != "smaller" && parameter_value != "更小" && parameter_value != "小一些") {
        return parameter_value;
    }

    const std::map<std::string, std::string>::const_iterator current = operation.attributes.find(parameter_name);
    if (current == operation.attributes.end()) {
        return parameter_value;
    }

    char* end = 0;
    const double parsed = std::strtod(current->second.c_str(), &end);
    if (end == current->second.c_str() || *end != '\0' || parsed <= 0.0) {
        return parameter_value;
    }

    return formatNumber(parsed * 0.5);
}

} // namespace

CommandRegistry CommandRegistry::browserDefaults()
{
    CommandRegistry registry;

    std::vector<std::string> create_required_args;
    create_required_args.push_back("operation_type");
    create_required_args.push_back("tool_id");
    create_required_args.push_back("stepover");
    create_required_args.push_back("stepdown");
    create_required_args.push_back("tolerance");

    registry.registerCommand(CommandSpec(
        "browser.createOperation",
        ObjectType::Unknown,
        create_required_args));

    std::vector<std::string> update_required_args;
    update_required_args.push_back("parameter_name");
    update_required_args.push_back("parameter_value");
    update_required_args.push_back("recompute_toolpath");
    registry.registerCommand(CommandSpec(
        "browser.updateOperation",
        ObjectType::Operation,
        update_required_args));

    registry.registerCommand(CommandSpec(
        "browser.generateToolpath",
        ObjectType::Operation,
        std::vector<std::string>()));

    std::vector<std::string> visibility_required_args;
    visibility_required_args.push_back("visibility");
    visibility_required_args.push_back("scope");
    registry.registerCommand(CommandSpec(
        "browser.setToolpathVisibility",
        ObjectType::Unknown,
        visibility_required_args));

    registry.registerCommand(CommandSpec(
        "browser.create_roughing_operation",
        ObjectType::Feature,
        create_required_args));

    registry.registerCommand(CommandSpec(
        "browser.generate_toolpath",
        ObjectType::Operation,
        std::vector<std::string>()));

    return registry;
}

void CommandRegistry::registerCommand(const CommandSpec& spec)
{
    specs_[spec.command_id] = spec;
}

bool CommandRegistry::find(const std::string& command_id, CommandSpec& spec) const
{
    const std::map<std::string, CommandSpec>::const_iterator found = specs_.find(command_id);
    if (found == specs_.end()) {
        return false;
    }

    spec = found->second;
    return true;
}

std::vector<std::string> CommandRegistry::commandIds() const
{
    std::vector<std::string> ids;
    for (std::map<std::string, CommandSpec>::const_iterator it = specs_.begin(); it != specs_.end(); ++it) {
        ids.push_back(it->first);
    }
    return ids;
}

BrowserConsole::BrowserConsole(Repository& repository)
    : operation_catalog_(OperationCatalog::defaults()),
      operation_factory_(operation_catalog_),
      operation_service_(repository, operation_factory_),
      toolpath_service_(repository),
      repository_(repository)
{
}

ConsoleCommandResult BrowserConsole::execute(const ConsoleCommandRequest& request)
{
    if (request.command_id == "browser.createOperation") {
        return createOperation(request);
    }

    if (request.command_id == "browser.updateOperation") {
        return updateOperation(request);
    }

    if (request.command_id == "browser.generateToolpath") {
        return generateToolpath(request);
    }

    if (request.command_id == "browser.setToolpathVisibility") {
        return setToolpathVisibility(request);
    }

    if (request.command_id == "browser.create_roughing_operation") {
        return createRoughingOperation(request);
    }

    if (request.command_id == "browser.generate_toolpath") {
        return generateToolpath(request);
    }

    ConsoleCommandResult result;
    result.error_code = "unsupported_command";
    result.message = "Command is not supported by BrowserConsole.";
    result.trace_events.push_back("browser_console_rejected");
    return result;
}

std::vector<std::string> BrowserConsole::supportedCommands() const
{
    std::vector<std::string> commands;
    commands.push_back("browser.createOperation");
    commands.push_back("browser.updateOperation");
    commands.push_back("browser.generateToolpath");
    commands.push_back("browser.setToolpathVisibility");
    commands.push_back("browser.create_roughing_operation");
    commands.push_back("browser.generate_toolpath");
    return commands;
}

ConsoleCommandResult BrowserConsole::createOperation(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    if (!request.target_object_id.empty()) {
        const CamObject target = repository_.get(request.target_object_id);
        if (target.object_type != ObjectType::Feature) {
            result.error_code = "invalid_target_type";
            result.message = "Operation creation target must be a feature when a target is supplied.";
            result.primary_object_id = request.target_object_id;
            result.trace_events.push_back("browser_console_rejected");
            return result;
        }
    }

    std::map<std::string, std::string>::const_iterator batch_count = request.args.find("batch_count");
    int count = 1;
    if (batch_count != request.args.end()) {
        std::istringstream stream(batch_count->second);
        stream >> count;
        if (stream.fail() || count < 1) {
            count = 1;
        }
    }

    for (int index = 0; index < count; ++index) {
        CreateOperationRequest create_request;
        create_request.operation_type = request.args.find("operation_type") == request.args.end()
            ? std::string("roughing")
            : request.args.find("operation_type")->second;
        create_request.target_object_id = request.target_object_id;
        create_request.parameters = request.args;

        CamObject operation;
        std::string error_code;
        std::string message;
        if (!operation_service_.createOperation(create_request, operation, error_code, message)) {
            result.error_code = error_code;
            result.message = message;
            result.primary_object_id = operation.object_id;
            result.trace_events.push_back("browser_console_rejected");
            return result;
        }

        if (result.primary_object_id.empty()) {
            result.primary_object_id = operation.object_id;
        }
        result.object_ids.push_back(operation.object_id);
    }

    result.ok = true;
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("operation_created");
    return result;
}

ConsoleCommandResult BrowserConsole::updateOperation(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    std::map<std::string, std::string>::const_iterator parameter_name = request.args.find("parameter_name");
    std::map<std::string, std::string>::const_iterator parameter_value = request.args.find("parameter_value");
    const std::string parameter_name_value = parameter_name == request.args.end() ? std::string() : parameter_name->second;
    const std::string raw_parameter_value = parameter_value == request.args.end() ? std::string() : parameter_value->second;
    const CamObject operation = repository_.get(request.target_object_id);
    const std::string resolved_parameter_value = resolveRelativeParameterValue(
        operation,
        parameter_name_value,
        raw_parameter_value);

    std::string error_code;
    std::string message;
    if (!operation_service_.updateOperationParameter(
            request.target_object_id,
            parameter_name_value,
            resolved_parameter_value,
            error_code,
            message)) {
        result.error_code = error_code;
        result.message = message;
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    result.ok = true;
    result.primary_object_id = request.target_object_id;
    result.object_ids.push_back(request.target_object_id);
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("operation_updated");

    std::map<std::string, std::string>::const_iterator recompute = request.args.find("recompute_toolpath");
    if (recompute != request.args.end() && recompute->second == "true") {
        toolpath_service_.deleteToolPathForOperation(request.target_object_id);
        CamObject toolpath;
        if (!toolpath_service_.generateToolPath(request.target_object_id, toolpath, error_code, message)) {
            result.ok = false;
            result.error_code = error_code;
            result.message = message;
            result.primary_object_id = toolpath.object_id.empty() ? request.target_object_id : toolpath.object_id;
            result.trace_events.push_back("browser_console_rejected");
            return result;
        }
        result.object_ids.push_back(toolpath.object_id);
        result.trace_events.push_back("toolpath_generated");
    }

    return result;
}

ConsoleCommandResult BrowserConsole::createRoughingOperation(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    if (!request.target_object_id.empty()) {
        const CamObject target = repository_.get(request.target_object_id);
        if (target.object_type != ObjectType::Feature) {
            result.error_code = "invalid_target_type";
            result.message = "Roughing operation creation requires a feature target.";
            result.primary_object_id = request.target_object_id;
            result.trace_events.push_back("browser_console_rejected");
            return result;
        }
    }

    CreateOperationRequest create_request;
    create_request.operation_type = request.args.find("operation_type") == request.args.end()
        ? std::string("roughing")
        : request.args.find("operation_type")->second;
    create_request.target_object_id = request.target_object_id;
    create_request.parameters = request.args;

    CamObject operation;
    std::string error_code;
    std::string message;
    if (!operation_service_.createOperation(create_request, operation, error_code, message)) {
        result.error_code = "invalid_target_type";
        if (!error_code.empty()) {
            result.error_code = error_code;
        }
        result.message = message;
        result.primary_object_id = operation.object_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    result.ok = true;
    result.primary_object_id = operation.object_id;
    result.object_ids.push_back(operation.object_id);
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("roughing_operation_created");
    return result;
}

ConsoleCommandResult BrowserConsole::generateToolpath(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    CamObject toolpath;
    std::string error_code;
    std::string message;
    if (!toolpath_service_.generateToolPath(request.target_object_id, toolpath, error_code, message)) {
        result.error_code = error_code;
        result.message = message;
        result.primary_object_id = toolpath.object_id.empty() ? request.target_object_id : toolpath.object_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    result.ok = true;
    result.primary_object_id = toolpath.object_id;
    result.object_ids.push_back(toolpath.object_id);
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("toolpath_generated");
    return result;
}

ConsoleCommandResult BrowserConsole::setToolpathVisibility(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    const std::map<std::string, std::string>::const_iterator visibility_arg = request.args.find("visibility");
    const std::map<std::string, std::string>::const_iterator scope_arg = request.args.find("scope");
    const std::string visibility = visibility_arg == request.args.end() ? std::string() : visibility_arg->second;
    const std::string scope = scope_arg == request.args.end() ? std::string() : scope_arg->second;

    if (scope == "all") {
        if (visibility != "show" && visibility != "hide") {
            result.error_code = "invalid_argument";
            result.message = "All toolpath visibility requires visibility show or hide.";
            result.trace_events.push_back("browser_console_rejected");
            return result;
        }
        result.object_ids = toolpath_service_.setAllToolPathVisibility(visibility == "show");
        result.ok = true;
        result.primary_object_id = result.object_ids.empty() ? std::string() : result.object_ids[0];
        result.trace_events.push_back("browser_console_dispatched");
        result.trace_events.push_back(visibility == "show" ? "toolpaths_shown" : "toolpaths_hidden");
        return result;
    }

    std::vector<std::string> target_ids;
    if (!request.target_object_id.empty()) {
        target_ids.push_back(request.target_object_id);
    }
    if (scope == "list") {
        const std::map<std::string, std::string>::const_iterator ids = request.args.find("toolpath_ids");
        target_ids = ids == request.args.end() ? std::vector<std::string>() : splitCsv(ids->second);
    } else if (scope == "operation_type") {
        const std::map<std::string, std::string>::const_iterator filter = request.args.find("operation_type");
        const std::string operation_type_filter = filter == request.args.end() ? std::string() : filter->second;
        const std::vector<CamObject> operations = repository_.objectsByType(ObjectType::Operation);
        target_ids.clear();
        for (std::size_t index = 0u; index < operations.size(); ++index) {
            const std::map<std::string, std::string>::const_iterator operation_type = operations[index].attributes.find("operation_type");
            const std::map<std::string, std::string>::const_iterator toolpath_id = operations[index].attributes.find("toolpath_id");
            if (operation_type != operations[index].attributes.end()
                && toolpath_id != operations[index].attributes.end()
                && operationTypeMatchesFilter(operation_type->second, operation_type_filter)) {
                target_ids.push_back(toolpath_id->second);
            }
        }
    }

    if (target_ids.empty()) {
        result.error_code = scope == "operation_type" ? "no_matching_toolpath" : "missing_target";
        result.message = scope == "operation_type"
            ? "No toolpath matches the requested operation type."
            : "Toolpath visibility requires a target toolpath.";
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    std::string error_code;
    std::string message;
    bool last_visible = false;
    if (visibility != "toggle" && visibility != "show" && visibility != "hide") {
        error_code = "invalid_argument";
        message = "Toolpath visibility must be show, hide, or toggle.";
        result.error_code = error_code;
        result.message = message;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    for (std::size_t index = 0u; index < target_ids.size(); ++index) {
        bool ok = false;
        if (visibility == "toggle") {
            ok = toolpath_service_.toggleToolPathVisibility(target_ids[index], last_visible, error_code, message);
        } else {
            last_visible = visibility == "show";
            ok = toolpath_service_.setToolPathVisibility(target_ids[index], last_visible, error_code, message);
        }
        if (!ok) {
            result.error_code = error_code;
            result.message = message;
            result.primary_object_id = target_ids[index];
            result.trace_events.push_back("browser_console_rejected");
            return result;
        }
        result.object_ids.push_back(target_ids[index]);
    }

    result.ok = true;
    result.primary_object_id = result.object_ids[0];
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back(last_visible ? "toolpath_shown" : "toolpath_hidden");
    return result;
}

ActionGateway::ActionGateway(Repository& repository, BrowserConsole& browser_console)
    : repository_(repository),
      browser_console_(browser_console),
      registry_(CommandRegistry::browserDefaults()),
      trace_service_(0)
{
}

ActionGateway::ActionGateway(Repository& repository, BrowserConsole& browser_console, const CommandRegistry& registry)
    : repository_(repository),
      browser_console_(browser_console),
      registry_(registry),
      trace_service_(0)
{
}

ActionGateway::ActionGateway(
    Repository& repository,
    BrowserConsole& browser_console,
    const CommandRegistry& registry,
    TraceService* trace_service)
    : repository_(repository),
      browser_console_(browser_console),
      registry_(registry),
      trace_service_(trace_service)
{
}

ConsoleCommandResult ActionGateway::dispatch(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    CommandSpec spec;
    if (!registry_.find(request.command_id, spec)) {
        result.error_code = "unsupported_command";
        result.message = "Command is not registered for controlled execution.";
        result.trace_events.push_back("gateway_rejected");
        if (trace_service_ != 0) {
            trace_service_->record(request.trace_id, "gateway", "gateway_rejected", request.target_object_id, result.message);
        }
        return result;
    }

    const CamObject target = repository_.get(request.target_object_id);
    if (spec.required_target_type != ObjectType::Unknown
        && target.object_type != spec.required_target_type) {
        result.error_code = "invalid_target_type";
        result.message = "Command target type is not valid.";
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("gateway_rejected");
        if (trace_service_ != 0) {
            trace_service_->record(request.trace_id, "gateway", "gateway_rejected", request.target_object_id, result.message);
        }
        return result;
    }

    std::string missing_arg;
    if (!commandHasRequiredArguments(request, spec, missing_arg)) {
        result.error_code = "missing_argument";
        result.message = "Command argument is required: " + missing_arg;
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("gateway_rejected");
        if (trace_service_ != 0) {
            trace_service_->record(request.trace_id, "gateway", "gateway_rejected", request.target_object_id, result.message);
        }
        return result;
    }

    std::string invalid_arg;
    if (!commandHasValidArgumentValues(request, invalid_arg)) {
        result.error_code = "invalid_argument";
        result.message = "Command argument is invalid: " + invalid_arg;
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("gateway_rejected");
        if (trace_service_ != 0) {
            trace_service_->record(request.trace_id, "gateway", "gateway_rejected", request.target_object_id, result.message);
        }
        return result;
    }

    result = browser_console_.execute(request);
    result.trace_events.insert(result.trace_events.begin(), "gateway_validated");
    if (trace_service_ != 0) {
        trace_service_->record(request.trace_id, "gateway", "gateway_validated", request.target_object_id, "Command passed Gateway validation.");
    }
    return result;
}

bool ActionGateway::registryMatchesConsole() const
{
    const std::vector<std::string> registry_commands = registry_.commandIds();
    const std::vector<std::string> console_commands = browser_console_.supportedCommands();
    if (registry_commands.size() != console_commands.size()) {
        return false;
    }

    for (std::size_t index = 0; index < registry_commands.size(); ++index) {
        if (std::find(console_commands.begin(), console_commands.end(), registry_commands[index]) == console_commands.end()) {
            return false;
        }
    }

    return true;
}

bool ActionGateway::commandHasRequiredArguments(
    const ConsoleCommandRequest& request,
    const CommandSpec& spec,
    std::string& missing_arg) const
{
    for (std::size_t index = 0; index < spec.required_args.size(); ++index) {
        const std::map<std::string, std::string>::const_iterator found = request.args.find(spec.required_args[index]);
        if (found == request.args.end() || found->second.empty()) {
            missing_arg = spec.required_args[index];
            return false;
        }
    }

    return true;
}

bool ActionGateway::commandHasValidArgumentValues(
    const ConsoleCommandRequest& request,
    std::string& invalid_arg) const
{
    if (request.command_id != "browser.create_roughing_operation"
        && request.command_id != "browser.createOperation") {
        return true;
    }

    const std::map<std::string, std::string>::const_iterator operation_type = request.args.find("operation_type");
    const bool drilling_operation = operation_type != request.args.end() && operation_type->second == "drilling";

    const std::map<std::string, std::string>::const_iterator stepover = request.args.find("stepover");
    if (stepover == request.args.end()
        || (drilling_operation ? !parseNonNegativeNumber(stepover->second) : !parsePositiveNumber(stepover->second))) {
        invalid_arg = "stepover";
        return false;
    }

    const std::map<std::string, std::string>::const_iterator stepdown = request.args.find("stepdown");
    if (stepdown == request.args.end() || !parsePositiveNumber(stepdown->second)) {
        invalid_arg = "stepdown";
        return false;
    }

    const std::map<std::string, std::string>::const_iterator tolerance = request.args.find("tolerance");
    if (tolerance == request.args.end() || !parseTolerance(tolerance->second)) {
        invalid_arg = "tolerance";
        return false;
    }

    return true;
}

SkillRuntime::SkillRuntime(ActionGateway& gateway)
    : gateway_(gateway),
      trace_service_(0)
{
}

SkillRuntime::SkillRuntime(ActionGateway& gateway, TraceService* trace_service)
    : gateway_(gateway),
      trace_service_(trace_service)
{
}

SkillExecutionResult SkillRuntime::execute(const SkillDefinition& definition, const SkillExecutionInput& input)
{
    SkillExecutionResult result;
    std::vector<ConsoleCommandResult> previous_results;

    result.trace_events.push_back("skill_started");
    if (trace_service_ != 0) {
        trace_service_->record(input.trace_id, "skill", "skill_started", input.target_object_id, "Skill execution started.");
    }

    for (std::size_t index = 0; index < definition.steps.size(); ++index) {
        bool resolved = false;
        const std::string target_object_id = resolveTargetObjectId(
            definition.steps[index].target_object_id_expression,
            input,
            previous_results,
            resolved);

        if (!resolved) {
            result.ok = false;
            result.failed_step_index = static_cast<int>(index);
            result.error_code = "unresolved_binding";
            result.message = "Skill target binding could not be resolved.";
            result.trace_events.push_back("skill_binding_failed");
            if (trace_service_ != 0) {
                trace_service_->record(input.trace_id, "skill", "skill_binding_failed", std::string(), result.message);
            }
            return result;
        }

        ConsoleCommandRequest request;
        request.command_id = definition.steps[index].command_id;
        request.source = "skill";
        request.trace_id = input.trace_id;
        request.target_object_id = target_object_id;
        request.args = definition.steps[index].args;

        ConsoleCommandResult command_result = gateway_.dispatch(request);
        result.trace_events.insert(result.trace_events.end(), command_result.trace_events.begin(), command_result.trace_events.end());

        if (!command_result.ok) {
            result.ok = false;
            result.failed_step_index = static_cast<int>(index);
            result.error_code = command_result.error_code;
            result.message = command_result.message;
            result.primary_object_id = command_result.primary_object_id;
            result.trace_events.push_back("skill_step_failed");
            if (trace_service_ != 0) {
                trace_service_->record(input.trace_id, "skill", "skill_step_failed", command_result.primary_object_id, result.message);
            }
            return result;
        }

        previous_results.push_back(command_result);
        result.object_ids.insert(result.object_ids.end(), command_result.object_ids.begin(), command_result.object_ids.end());
        if (result.primary_object_id.empty()) {
            result.primary_object_id = command_result.primary_object_id;
        }
        result.trace_events.push_back("skill_step_completed");
        if (trace_service_ != 0) {
            trace_service_->record(input.trace_id, "skill", "skill_step_completed", command_result.primary_object_id, "Skill step completed.");
        }
    }

    result.ok = true;
    result.failed_step_index = -1;
    result.trace_events.push_back("skill_completed");
    if (trace_service_ != 0) {
        trace_service_->record(input.trace_id, "skill", "skill_completed", result.primary_object_id, "Skill execution completed.");
    }
    return result;
}

std::string SkillRuntime::resolveTargetObjectId(
    const std::string& expression,
    const SkillExecutionInput& input,
    const std::vector<ConsoleCommandResult>& previous_results,
    bool& ok) const
{
    ok = false;

    if (expression == "$input.target_object_id") {
        ok = !input.target_object_id.empty();
        return input.target_object_id;
    }

    const std::string prefix = "$step";
    const std::string suffix = ".primary_object_id";
    if (expression.size() > prefix.size() + suffix.size()
        && expression.compare(0, prefix.size(), prefix) == 0
        && expression.compare(expression.size() - suffix.size(), suffix.size(), suffix) == 0) {
        const std::string index_text = expression.substr(prefix.size(), expression.size() - prefix.size() - suffix.size());
        std::istringstream stream(index_text);
        std::size_t index = 0u;
        stream >> index;
        if (!stream.fail() && index < previous_results.size() && !previous_results[index].primary_object_id.empty()) {
            ok = true;
            return previous_results[index].primary_object_id;
        }
    }

    return std::string();
}

} // namespace camclaw
