#include "camclaw/AgentPlanDraftJsonCodec.h"
#include "camclaw/AgentPlanExecutor.h"

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

static int python_backend_draft_can_be_confirmed_and_executed_by_cpp_core()
{
    const std::string backend_json =
        "{"
        "\"trace_id\":\"trace_bridge_001\","
        "\"status\":\"pending_review\","
        "\"steps\":[{"
        "\"skill_id\":\"browser.create_roughing_operation\","
        "\"inputs\":{"
        "\"target_object_id\":\"feature_001\","
        "\"operation_type\":\"roughing\","
        "\"tool_id\":\"tool_010\","
        "\"stepover\":\"2.0\","
        "\"stepdown\":\"1.0\","
        "\"tolerance\":\"0.02\""
        "}"
        "}],"
        "\"trace_events\":[\"draft_created\"]"
        "}";

    camclaw::AgentPlanDraftJsonCodec codec;
    camclaw::AgentPlanDraftDecodeResult decode_result = codec.decodeBackendDraft(backend_json);
    REQUIRE_TRUE(decode_result.ok);

    decode_result.draft.confirm();

    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    const camclaw::AgentPlanExecutionResult execution_result = executor.execute(decode_result.draft);

    REQUIRE_TRUE(execution_result.ok);
    REQUIRE_EQ(std::string("op_roughing_feature_001"), execution_result.primary_object_id);
    REQUIRE_TRUE(repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(!repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(execution_result.trace_events, "gateway_validated"));
    REQUIRE_TRUE(contains_event(execution_result.trace_events, "draft_execution_completed"));

    return EXIT_SUCCESS;
}

int main()
{
    return python_backend_draft_can_be_confirmed_and_executed_by_cpp_core();
}
