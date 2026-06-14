#include "camclaw/AgentPlanDraftJsonCodec.h"
#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/TraceService.h"

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

static int trace_service_records_draft_skill_and_gateway_events()
{
    const std::string backend_json =
        "{"
        "\"trace_id\":\"trace_service_001\","
        "\"status\":\"pending_review\","
        "\"steps\":[{"
        "\"skill_id\":\"browser.create_roughing_operation_and_generate_toolpath\","
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

    camclaw::TraceService trace_service;
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(
        repository,
        browser_console,
        camclaw::CommandRegistry::browserDefaults(),
        &trace_service);
    camclaw::SkillRuntime skill_runtime(gateway, &trace_service);
    camclaw::AgentPlanExecutor executor(skill_runtime, &trace_service);

    const camclaw::AgentPlanExecutionResult execution_result = executor.execute(decode_result.draft);
    REQUIRE_TRUE(execution_result.ok);

    REQUIRE_TRUE(trace_service.contains("trace_service_001", "draft_execution_started"));
    REQUIRE_TRUE(trace_service.contains("trace_service_001", "skill_started"));
    REQUIRE_TRUE(trace_service.contains("trace_service_001", "gateway_validated"));
    REQUIRE_TRUE(trace_service.contains("trace_service_001", "skill_completed"));
    REQUIRE_TRUE(trace_service.contains("trace_service_001", "draft_execution_completed"));

    const std::vector<camclaw::TraceEvent> events = trace_service.eventsForTrace("trace_service_001");
    REQUIRE_TRUE(events.size() >= 5u);
    REQUIRE_EQ(std::string("trace_service_001"), events[0].trace_id);

    return EXIT_SUCCESS;
}

int main()
{
    return trace_service_records_draft_skill_and_gateway_events();
}
