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

class BrowserConsole {
public:
    explicit BrowserConsole(Repository& repository);

    ConsoleCommandResult execute(const ConsoleCommandRequest& request);

private:
    ConsoleCommandResult createRoughingOperation(const ConsoleCommandRequest& request);
    ConsoleCommandResult generateToolpath(const ConsoleCommandRequest& request);

    Repository& repository_;
};

class ActionGateway {
public:
    ActionGateway(Repository& repository, BrowserConsole& browser_console);

    ConsoleCommandResult dispatch(const ConsoleCommandRequest& request);

private:
    bool commandRequiresTargetType(const std::string& command_id, ObjectType& required_type) const;

    Repository& repository_;
    BrowserConsole& browser_console_;
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
