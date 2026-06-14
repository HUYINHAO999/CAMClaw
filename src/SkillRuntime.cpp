#include "camclaw/SkillRuntime.h"

#include <sstream>

namespace camclaw {

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
    repository_.save(CamObject(operation_id, ObjectType::Operation, "Roughing operation"));

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
    repository_.save(CamObject(toolpath_id, ObjectType::Toolpath, "Generated toolpath"));

    result.ok = true;
    result.primary_object_id = toolpath_id;
    result.object_ids.push_back(toolpath_id);
    result.trace_events.push_back("browser_console_dispatched");
    result.trace_events.push_back("toolpath_generated");
    return result;
}

ActionGateway::ActionGateway(Repository& repository, BrowserConsole& browser_console)
    : repository_(repository),
      browser_console_(browser_console)
{
}

ConsoleCommandResult ActionGateway::dispatch(const ConsoleCommandRequest& request)
{
    ConsoleCommandResult result;
    ObjectType required_type = ObjectType::Unknown;
    if (!commandRequiresTargetType(request.command_id, required_type)) {
        result.error_code = "unsupported_command";
        result.message = "Command is not registered for controlled execution.";
        result.trace_events.push_back("gateway_rejected");
        return result;
    }

    const CamObject target = repository_.get(request.target_object_id);
    if (target.object_type != required_type) {
        result.error_code = "invalid_target_type";
        result.message = "Command target type is not valid.";
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("gateway_rejected");
        return result;
    }

    std::string missing_arg;
    if (!commandHasRequiredArguments(request, missing_arg)) {
        result.error_code = "missing_argument";
        result.message = "Command argument is required: " + missing_arg;
        result.primary_object_id = request.target_object_id;
        result.trace_events.push_back("gateway_rejected");
        return result;
    }

    result = browser_console_.execute(request);
    result.trace_events.insert(result.trace_events.begin(), "gateway_validated");
    return result;
}

bool ActionGateway::commandRequiresTargetType(const std::string& command_id, ObjectType& required_type) const
{
    if (command_id == "browser.create_roughing_operation") {
        required_type = ObjectType::Feature;
        return true;
    }

    if (command_id == "browser.generate_toolpath") {
        required_type = ObjectType::Operation;
        return true;
    }

    return false;
}

bool ActionGateway::commandHasRequiredArguments(const ConsoleCommandRequest& request, std::string& missing_arg) const
{
    if (request.command_id != "browser.create_roughing_operation") {
        return true;
    }

    const char* required[] = {
        "operation_type",
        "tool_id",
        "stepover",
        "stepdown",
        "tolerance"
    };

    for (std::size_t index = 0; index < sizeof(required) / sizeof(required[0]); ++index) {
        const std::map<std::string, std::string>::const_iterator found = request.args.find(required[index]);
        if (found == request.args.end() || found->second.empty()) {
            missing_arg = required[index];
            return false;
        }
    }

    return true;
}

SkillRuntime::SkillRuntime(ActionGateway& gateway)
    : gateway_(gateway)
{
}

SkillExecutionResult SkillRuntime::execute(const SkillDefinition& definition, const SkillExecutionInput& input)
{
    SkillExecutionResult result;
    std::vector<ConsoleCommandResult> previous_results;

    result.trace_events.push_back("skill_started");

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
            return result;
        }

        previous_results.push_back(command_result);
        result.object_ids.insert(result.object_ids.end(), command_result.object_ids.begin(), command_result.object_ids.end());
        if (result.primary_object_id.empty()) {
            result.primary_object_id = command_result.primary_object_id;
        }
        result.trace_events.push_back("skill_step_completed");
    }

    result.ok = true;
    result.failed_step_index = -1;
    result.trace_events.push_back("skill_completed");
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
