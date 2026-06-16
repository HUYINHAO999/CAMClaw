#ifndef CAMCLAW_AGENT_PLAN_EXECUTOR_H
#define CAMCLAW_AGENT_PLAN_EXECUTOR_H

#include "camclaw/AgentPlanDraft.h"
#include "camclaw/HumanInLoopService.h"
#include "camclaw/SkillRuntime.h"

#include <string>
#include <vector>

namespace camclaw {

struct AgentPlanExecutionResult {
    AgentPlanExecutionResult()
        : ok(false),
          needs_clarification(false)
    {
    }

    bool ok;
    bool needs_clarification;
    std::string error_code;
    std::string message;
    std::string primary_object_id;
    std::vector<std::string> object_ids;
    std::vector<std::string> trace_events;
    ClarificationRequest clarification;
};

class AgentPlanExecutor {
public:
    explicit AgentPlanExecutor(SkillRuntime& skill_runtime);
    AgentPlanExecutor(SkillRuntime& skill_runtime, TraceService* trace_service);
    AgentPlanExecutor(SkillRuntime& skill_runtime, Repository& repository, HumanInLoopService& human_in_loop_service);

    AgentPlanExecutionResult execute(const AgentPlanDraft& draft);

private:
    bool buildSkillDefinition(const SkillStepDraft& draft_step, SkillDefinition& skill) const;
    bool resolveDraftStepTarget(
        const AgentPlanDraft& draft,
        std::size_t step_index,
        AgentPlanExecutionResult& result,
        std::string& resolved_target_object_id);

    SkillRuntime& skill_runtime_;
    TraceService* trace_service_;
    Repository* repository_;
    HumanInLoopService* human_in_loop_service_;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLAN_EXECUTOR_H
