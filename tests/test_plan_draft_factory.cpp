#include "camclaw/PlanDraftFactory.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define REQUIRE_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Requirement failed: " #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

#define REQUIRE_EQ(expected, actual) \
    do { \
        if (!((expected) == (actual))) { \
            std::cerr << "Requirement failed: " #expected " == " #actual << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

static bool contains_event(const std::vector<std::string>& events, const std::string& event_name)
{
    return std::find(events.begin(), events.end(), event_name) != events.end();
}

class StubPlanningContextProvider : public camclaw::PlanningContextProvider {
public:
    std::vector<camclaw::PlanningContextItem> contextForTarget(const std::string& target_object_id) const
    {
        std::vector<camclaw::PlanningContextItem> items;
        items.push_back(camclaw::PlanningContextItem("template", "roughing-default-for-" + target_object_id));
        return items;
    }
};

static int draft_factory_accepts_structured_proposal_and_optional_context()
{
    camclaw::RoughingPlanProposal proposal;
    proposal.trace_id = "trace_factory_001";
    proposal.target_object_id = "feature_001";
    proposal.operation_type = "roughing";
    proposal.tool_id = "tool_010";
    proposal.stepover = "2.0";
    proposal.stepdown = "1.0";
    proposal.tolerance = "0.02";

    StubPlanningContextProvider context_provider;
    camclaw::PlanDraftFactory factory;

    const camclaw::AgentPlanDraft draft = factory.createRoughingDraft(proposal, &context_provider);

    REQUIRE_EQ(camclaw::DraftStatus::PendingReview, draft.status());
    REQUIRE_EQ(1u, draft.stepCount());
    REQUIRE_EQ(std::string("browser.create_roughing_operation_and_generate_toolpath"), draft.stepAt(0).skillId());
    REQUIRE_EQ(std::string("feature_001"), draft.stepAt(0).inputValue("target_object_id"));
    REQUIRE_EQ(std::string("tool_010"), draft.stepAt(0).inputValue("tool_id"));
    REQUIRE_EQ(1u, draft.planningContextCount());
    REQUIRE_EQ(std::string("template"), draft.planningContextAt(0).source);
    REQUIRE_TRUE(contains_event(draft.traceEvents(), "planning_context_attached"));

    return EXIT_SUCCESS;
}

int main()
{
    return draft_factory_accepts_structured_proposal_and_optional_context();
}
