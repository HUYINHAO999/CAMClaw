#include "camclaw/AgentPlanExecutor.h"
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

static camclaw::AgentPlanDraft create_draft()
{
    camclaw::RoughingPlanProposal proposal;
    proposal.trace_id = "trace_execute_001";
    proposal.target_object_id = "feature_001";
    proposal.operation_type = "roughing";
    proposal.tool_id = "tool_010";
    proposal.stepover = "2.0";
    proposal.stepdown = "1.0";
    proposal.tolerance = "0.02";

    camclaw::PlanDraftFactory factory;
    return factory.createRoughingDraft(proposal, 0);
}

static int confirmed_draft_executes_reviewed_skill_inputs()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    camclaw::AgentPlanDraft draft = create_draft();
    REQUIRE_EQ(camclaw::DraftEditStatus::Edited, draft.editStepInput(0, "stepover", "1.5").status);
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("op_roughing_feature_001"), result.primary_object_id);
    REQUIRE_TRUE(repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "draft_execution_completed"));
    REQUIRE_TRUE(contains_event(result.trace_events, "skill_completed"));

    return EXIT_SUCCESS;
}

static int rejected_draft_cannot_execute()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    camclaw::AgentPlanDraft draft = create_draft();
    draft.reject("只要粗加工，不要精加工");

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("draft_not_confirmed"), result.error_code);
    REQUIRE_TRUE(!repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "draft_execution_rejected"));

    return EXIT_SUCCESS;
}

int main()
{
    int status = confirmed_draft_executes_reviewed_skill_inputs();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = rejected_draft_cannot_execute();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
