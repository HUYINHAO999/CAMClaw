#include "camclaw/AgentPlanDraftJsonCodec.h"

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

static int decodes_python_backend_draft_json()
{
    const std::string json =
        "{"
        "\"trace_id\":\"trace_json_001\","
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
    const camclaw::AgentPlanDraftDecodeResult result = codec.decodeBackendDraft(json);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("trace_json_001"), result.draft.traceId());
    REQUIRE_EQ(camclaw::DraftStatus::PendingReview, result.draft.status());
    REQUIRE_EQ(1u, result.draft.stepCount());
    REQUIRE_EQ(std::string("browser.create_roughing_operation_and_generate_toolpath"), result.draft.stepAt(0).skillId());
    REQUIRE_EQ(std::string("feature_001"), result.draft.stepAt(0).inputValue("target_object_id"));
    REQUIRE_EQ(std::string("tool_010"), result.draft.stepAt(0).inputValue("tool_id"));

    return EXIT_SUCCESS;
}

static int rejects_incomplete_backend_draft_json()
{
    camclaw::AgentPlanDraftJsonCodec codec;
    const camclaw::AgentPlanDraftDecodeResult result = codec.decodeBackendDraft(
        "{"
        "\"trace_id\":\"trace_json_002\","
        "\"status\":\"pending_review\","
        "\"steps\":[{"
        "\"skill_id\":\"browser.create_roughing_operation_and_generate_toolpath\","
        "\"inputs\":{\"target_object_id\":\"feature_001\"}"
        "}]"
        "}");

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("missing_draft_input"), result.error_code);

    return EXIT_SUCCESS;
}

int main()
{
    int status = decodes_python_backend_draft_json();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = rejects_incomplete_backend_draft_json();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
