#ifndef CAMCLAW_AGENT_PLAN_EXECUTOR_H
#define CAMCLAW_AGENT_PLAN_EXECUTOR_H

#include "camclaw/AgentPlanDraft.h"
#include "camclaw/SkillRuntime.h"

#include <string>
#include <vector>

namespace camclaw {

struct AgentPlanExecutionResult {
    AgentPlanExecutionResult()
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

class AgentPlanExecutor {
public:
    explicit AgentPlanExecutor(SkillRuntime& skill_runtime);
    AgentPlanExecutor(SkillRuntime& skill_runtime, TraceService* trace_service);

    AgentPlanExecutionResult execute(const AgentPlanDraft& draft);

private:
    bool buildSkillDefinition(const SkillStepDraft& draft_step, SkillDefinition& skill) const;

    SkillRuntime& skill_runtime_;
    TraceService* trace_service_;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLAN_EXECUTOR_H
