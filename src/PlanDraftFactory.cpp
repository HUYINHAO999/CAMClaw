#include "camclaw/PlanDraftFactory.h"

#include <map>

namespace camclaw {

AgentPlanDraft PlanDraftFactory::createRoughingDraft(
    const RoughingPlanProposal& proposal,
    const PlanningContextProvider* context_provider) const
{
    AgentPlanDraft draft(proposal.trace_id);

    if (context_provider != 0) {
        const std::vector<PlanningContextItem> context_items = context_provider->contextForTarget(proposal.target_object_id);
        for (std::size_t index = 0; index < context_items.size(); ++index) {
            draft.addPlanningContext(context_items[index]);
        }
    }

    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = proposal.target_object_id;
    inputs["operation_type"] = proposal.operation_type;
    inputs["tool_id"] = proposal.tool_id;
    inputs["stepover"] = proposal.stepover;
    inputs["stepdown"] = proposal.stepdown;
    inputs["tolerance"] = proposal.tolerance;

    draft.addSkillStep(SkillStepDraft("browser.create_roughing_operation_and_generate_toolpath", inputs));
    return draft;
}

} // namespace camclaw
