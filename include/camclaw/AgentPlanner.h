#ifndef CAMCLAW_AGENT_PLANNER_H
#define CAMCLAW_AGENT_PLANNER_H

#include "camclaw/PlanDraftFactory.h"

#include <map>
#include <string>

namespace camclaw {

struct LlmEndpointConfig {
    std::string base_url;
    std::string api_key;
    std::string model;

    static LlmEndpointConfig fromEnvironment();
};

struct LlmPlanRequest {
    std::string trace_id;
    std::string user_request;
    std::string target_object_id;
};

class LlmClient {
public:
    virtual ~LlmClient()
    {
    }

    virtual std::string createPlanJson(const LlmPlanRequest& request) = 0;
};

struct AgentPlannerResult {
    AgentPlannerResult();

    bool ok;
    AgentPlanDraft draft;
    std::string error_code;
    std::string message;
};

class AgentPlanner {
public:
    AgentPlanner(LlmClient& llm_client, const PlanDraftFactory& draft_factory);

    AgentPlannerResult createDraft(
        const std::string& trace_id,
        const std::string& user_request,
        const std::string& target_object_id,
        const PlanningContextProvider* context_provider) const;

private:
    bool parseProposalJson(
        const std::string& json,
        const std::string& trace_id,
        const std::string& target_object_id,
        RoughingPlanProposal& proposal,
        std::string& error_code,
        std::string& message) const;

    LlmClient& llm_client_;
    const PlanDraftFactory& draft_factory_;
};

} // namespace camclaw

#endif // CAMCLAW_AGENT_PLANNER_H
