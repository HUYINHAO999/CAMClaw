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
    : repository_(repository)
{
}

ConsoleCommandResult BrowserConsole::execute(const ConsoleCommandRequest& request)
{
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
    commands.push_back("browser.create_roughing_operation");
    commands.push_back("browser.generate_toolpath");
    return commands;
}

ConsoleCommandResult BrowserConsole::createRoughingOperation(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    const CamObject target = repository_.get(request.target_object_id);
    if (target.object_type != ObjectType::Feature) {
        result.error_code = "invalid_target_type";
        result.message = "Roughing operation creation requires a feature target.";
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    const std::string operation_id = "op_roughing_" + request.target_object_id;
    if (!repository_.save(CamObject(operation_id, ObjectType::Operation, "Roughing operation"))) {
        result.error_code = "duplicate_object_id";
        result.message = "Operation object ID already exists.";
        result.primary_object_id = operation_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    result.ok = true;
    result.primary_object_id = operation_id;
    result.object_ids.push_back(operation_id);
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("roughing_operation_created");
    return result;
}

ConsoleCommandResult BrowserConsole::generateToolpath(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    const CamObject target = repository_.get(request.target_object_id);
    if (target.object_type != ObjectType::Operation) {
        result.error_code = "invalid_target_type";
        result.message = "Toolpath generation requires an operation target.";
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    const std::string toolpath_id = "toolpath_" + request.target_object_id;
    if (!repository_.save(CamObject(toolpath_id, ObjectType::Toolpath, "Generated toolpath"))) {
        result.error_code = "duplicate_object_id";
        result.message = "Toolpath object ID already exists.";
        result.primary_object_id = toolpath_id;
        result.trace_events.push_back("browser_console_rejected");
        return result;
    }

    result.ok = true;
    result.primary_object_id = toolpath_id;
    result.object_ids.push_back(toolpath_id);
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("toolpath_generated");
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
    if (target.object_type != spec.required_target_type) {
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
    if (request.command_id != "browser.create_roughing_operation") {
        return true;
    }

    const std::map<std::string, std::string>::const_iterator stepover = request.args.find("stepover");
    if (stepover == request.args.end() || !parsePositiveNumber(stepover->second)) {
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
