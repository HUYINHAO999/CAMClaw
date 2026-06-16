#ifndef CAMCLAW_PLAN_DRAFT_FACTORY_H
#define CAMCLAW_PLAN_DRAFT_FACTORY_H

#include "camclaw/AgentPlanDraft.h"

#include <string>
#include <vector>

namespace camclaw {

struct RoughingPlanProposal {
    std::string trace_id;
    std::string target_object_id;
    std::string operation_type;
    std::string tool_id;
    std::string stepover;
    std::string stepdown;
    std::string tolerance;
};

class PlanningContextProvider {
public:
    virtual ~PlanningContextProvider()
    {
    }

    virtual std::vector<PlanningContextItem> contextForTarget(const std::string& target_object_id) const = 0;
};

class PlanDraftFactory {
public:
    AgentPlanDraft createRoughingDraft(
        const RoughingPlanProposal& proposal,
        const PlanningContextProvider* context_provider) const;
    AgentPlanDraft createRoughingOperationDraft(
        const RoughingPlanProposal& proposal,
        const PlanningContextProvider* context_provider) const;
};

} // namespace camclaw

#endif // CAMCLAW_PLAN_DRAFT_FACTORY_H
