#ifndef CAMCLAW_SKILL_RUNTIME_H
#define CAMCLAW_SKILL_RUNTIME_H

#include "camclaw/AgentWorkflowService.h"

#include <map>
#include <string>
#include <vector>

namespace camclaw {

struct ConsoleCommandRequest {
    std::string command_id;
    std::string source;
    std::string trace_id;
    std::string target_object_id;
    ObjectType target_object_type_hint;
    std::map<std::string, std::string> args;

    ConsoleCommandRequest()
        : target_object_type_hint(ObjectType::Unknown)
    {
    }
};

struct ConsoleCommandResult {
    ConsoleCommandResult()
        : ok(false)
    {
    }

    bool ok;
    std::string error_code;
    std::string message;
    std::string primary_object_id;
    std::vector<std::string> object_ids;
    std::vector<std::string> trace_events;
};

struct CommandSpec {
    CommandSpec()
        : required_target_type(ObjectType::Unknown)
    {
    }

    CommandSpec(
        const std::string& id,
        ObjectType target_type,
        const std::vector<std::string>& required_argument_names)
        : command_id(id),
          required_target_type(target_type),
          required_args(required_argument_names)
    {
    }

    std::string command_id;
    ObjectType required_target_type;
    std::vector<std::string> required_args;
};

class CommandRegistry {
public:
    static CommandRegistry browserDefaults();

    void registerCommand(const CommandSpec& spec);
    bool find(const std::string& command_id, CommandSpec& spec) const;
    std::vector<std::string> commandIds() const;

private:
    std::map<std::string, CommandSpec> specs_;
};

class BrowserConsole {
public:
    explicit BrowserConsole(Repository& repository);

    ConsoleCommandResult execute(const ConsoleCommandRequest& request);
    std::vector<std::string> supportedCommands() const;

private:
    ConsoleCommandResult createRoughingOperation(const ConsoleCommandRequest& request);
    ConsoleCommandResult generateToolpath(const ConsoleCommandRequest& request);

    Repository& repository_;
};

class ActionGateway {
public:
    ActionGateway(Repository& repository, BrowserConsole& browser_console);
    ActionGateway(Repository& repository, BrowserConsole& browser_console, const CommandRegistry& registry);

    ConsoleCommandResult dispatch(const ConsoleCommandRequest& request);
    bool registryMatchesConsole() const;

private:
    bool commandHasRequiredArguments(
        const ConsoleCommandRequest& request,
        const CommandSpec& spec,
        std::string& missing_arg) const;
    bool commandHasValidArgumentValues(
        const ConsoleCommandRequest& request,
        std::string& invalid_arg) const;

    Repository& repository_;
    BrowserConsole& browser_console_;
    CommandRegistry registry_;
};

struct SkillCommandStep {
    std::string command_id;
    std::string target_object_id_expression;
    std::map<std::string, std::string> args;
};

struct SkillDefinition {
    std::string skill_id;
    std::vector<SkillCommandStep> steps;
};

struct SkillExecutionInput {
    std::string trace_id;
    std::string target_object_id;
};

struct SkillExecutionResult {
    SkillExecutionResult()
        : ok(false),
          failed_step_index(-1)
    {
    }

    bool ok;
    int failed_step_index;
    std::string error_code;
    std::string message;
    std::string primary_object_id;
    std::vector<std::string> object_ids;
    std::vector<std::string> trace_events;
};

class SkillRuntime {
public:
    explicit SkillRuntime(ActionGateway& gateway);

    SkillExecutionResult execute(const SkillDefinition& definition, const SkillExecutionInput& input);

private:
    std::string resolveTargetObjectId(
        const std::string& expression,
        const SkillExecutionInput& input,
        const std::vector<ConsoleCommandResult>& previous_results,
        bool& ok) const;

    ActionGateway& gateway_;
};

} // namespace camclaw

#endif // CAMCLAW_SKILL_RUNTIME_H
