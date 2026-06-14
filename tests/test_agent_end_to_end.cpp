#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/AgentPlanner.h"

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

class FakeLlmClient : public camclaw::LlmClient {
public:
    std::string createPlanJson(const camclaw::LlmPlanRequest&)
    {
        return "{"
            "\"operation_type\":\"roughing\","
            "\"tool_id\":\"tool_010\","
            "\"stepover\":\"2.0\","
            "\"stepdown\":\"1.0\","
            "\"tolerance\":\"0.02\""
            "}";
    }
};

static bool contains_event(const std::vector<std::string>& events, const std::string& event_name)
{
    return std::find(events.begin(), events.end(), event_name) != events.end();
}

static int user_request_becomes_confirmed_draft_and_normal_cam_objects()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));

    FakeLlmClient llm;
    camclaw::PlanDraftFactory draft_factory;
    camclaw::AgentPlanner planner(llm, draft_factory);

    camclaw::AgentPlannerResult plan_result = planner.createDraft(
        "trace_e2e_001",
        "给当前型腔做粗加工",
        "feature_001",
        0);

    REQUIRE_TRUE(plan_result.ok);
    REQUIRE_EQ(camclaw::DraftStatus::PendingReview, plan_result.draft.status());
    REQUIRE_EQ(std::string("tool_010"), plan_result.draft.stepAt(0).inputValue("tool_id"));

    plan_result.draft.confirm();

    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    const camclaw::AgentPlanExecutionResult execution_result = executor.execute(plan_result.draft);

    REQUIRE_TRUE(execution_result.ok);
    REQUIRE_EQ(std::string("op_roughing_feature_001"), execution_result.primary_object_id);
    REQUIRE_TRUE(repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(execution_result.trace_events, "gateway_validated"));
    REQUIRE_TRUE(contains_event(execution_result.trace_events, "draft_execution_completed"));

    return EXIT_SUCCESS;
}

int main()
{
    return user_request_becomes_confirmed_draft_and_normal_cam_objects();
}
