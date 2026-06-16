#ifndef CAMCLAW_AGENT_PLAN_EXECUTION_RESULT_H
#define CAMCLAW_AGENT_PLAN_EXECUTION_RESULT_H

#include "camclaw/HumanInLoopService.h"

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

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLAN_EXECUTION_RESULT_H
