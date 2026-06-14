#include "camclaw/AgentPlanner.h"

#include <cstdlib>
#include <iostream>
#include <string>

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
    explicit FakeLlmClient(const std::string& response)
        : response_(response)
    {
    }

    std::string createPlanJson(const camclaw::LlmPlanRequest& request)
    {
        last_request = request;
        return response_;
    }

    camclaw::LlmPlanRequest last_request;

private:
    std::string response_;
};

static int planner_builds_draft_from_structured_llm_response()
{
    FakeLlmClient llm(
        "{"
        "\"operation_type\":\"roughing\","
        "\"tool_id\":\"tool_010\","
        "\"stepover\":\"2.0\","
        "\"stepdown\":\"1.0\","
        "\"tolerance\":\"0.02\""
        "}");
    camclaw::PlanDraftFactory factory;
    camclaw::AgentPlanner planner(llm, factory);

    const camclaw::AgentPlannerResult result = planner.createDraft(
        "trace_plan_001",
        "给当前型腔做粗加工",
        "feature_001",
        0);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("trace_plan_001"), llm.last_request.trace_id);
    REQUIRE_EQ(std::string("给当前型腔做粗加工"), llm.last_request.user_request);
    REQUIRE_EQ(std::string("feature_001"), llm.last_request.target_object_id);
    REQUIRE_TRUE(llm.last_request.response_contract.find("operation_type") != std::string::npos);
    REQUIRE_TRUE(llm.last_request.response_contract.find("Do not execute CAM commands") != std::string::npos);
    REQUIRE_EQ(1u, result.draft.stepCount());
    REQUIRE_EQ(std::string("roughing"), result.draft.stepAt(0).inputValue("operation_type"));
    REQUIRE_EQ(std::string("tool_010"), result.draft.stepAt(0).inputValue("tool_id"));

    return EXIT_SUCCESS;
}

static int planner_rejects_unstructured_llm_response()
{
    FakeLlmClient llm("roughing with tool_010");
    camclaw::PlanDraftFactory factory;
    camclaw::AgentPlanner planner(llm, factory);

    const camclaw::AgentPlannerResult result = planner.createDraft(
        "trace_plan_002",
        "给当前型腔做粗加工",
        "feature_001",
        0);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("invalid_llm_response"), result.error_code);
    REQUIRE_EQ(0u, result.draft.stepCount());

    return EXIT_SUCCESS;
}

static int planner_rejects_missing_required_llm_fields()
{
    FakeLlmClient llm(
        "{"
        "\"operation_type\":\"roughing\","
        "\"tool_id\":\"tool_010\""
        "}");
    camclaw::PlanDraftFactory factory;
    camclaw::AgentPlanner planner(llm, factory);

    const camclaw::AgentPlannerResult result = planner.createDraft(
        "trace_plan_003",
        "给当前型腔做粗加工",
        "feature_001",
        0);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("missing_llm_field"), result.error_code);
    REQUIRE_EQ(0u, result.draft.stepCount());

    return EXIT_SUCCESS;
}

int main()
{
    int status = planner_builds_draft_from_structured_llm_response();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = planner_rejects_unstructured_llm_response();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = planner_rejects_missing_required_llm_fields();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
