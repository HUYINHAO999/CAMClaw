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
        "\"command_id\":\"browser.createOperation\","
        "\"schema_id\":\"browser.createOperation.v1\","
        "\"inputs\":{\"operation_type\":\"drilling\"}"
        "}]"
        "}");

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("missing_draft_input"), result.error_code);

    return EXIT_SUCCESS;
}

static int decodes_command_schema_backend_draft_json()
{
    const std::string json =
        "{"
        "\"schema_version\":\"1\","
        "\"trace_id\":\"trace_json_003\","
        "\"status\":\"pending_review\","
        "\"steps\":[{"
        "\"command_id\":\"browser.createOperation\","
        "\"schema_id\":\"browser.createOperation.v1\","
        "\"inputs\":{"
        "\"target_object_id\":\"feature_holes_001\","
        "\"operation_type\":\"drilling\","
        "\"tool_id\":\"tool_006\","
        "\"stepover\":\"0\","
        "\"stepdown\":\"3\","
        "\"tolerance\":\"0.02\","
        "\"batch_count\":\"10\""
        "}"
        "}],"
        "\"trace_events\":[\"draft_created\"]"
        "}";

    camclaw::AgentPlanDraftJsonCodec codec;
    const camclaw::AgentPlanDraftDecodeResult result = codec.decodeBackendDraft(json);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("browser.createOperation"), result.draft.stepAt(0).skillId());
    REQUIRE_EQ(std::string("browser.createOperation.v1"), result.draft.stepAt(0).inputValue("schema_id"));
    REQUIRE_EQ(std::string("10"), result.draft.stepAt(0).inputValue("batch_count"));
    REQUIRE_EQ(std::string("drilling"), result.draft.stepAt(0).inputValue("operation_type"));

    return EXIT_SUCCESS;
}

static int decodes_action_sequence_backend_draft_json()
{
    const std::string json =
        "{"
        "\"schema_version\":\"1\","
        "\"trace_id\":\"trace_json_actions\","
        "\"status\":\"pending_review\","
        "\"steps\":["
        "{"
        "\"command_id\":\"browser.setToolpathVisibility\","
        "\"schema_id\":\"browser.setToolpathVisibility.v1\","
        "\"inputs\":{"
        "\"target_object_id\":\"toolpath_op_drilling\","
        "\"visibility\":\"hide\","
        "\"scope\":\"operation_type\","
        "\"operation_type\":\"pocket\","
        "\"toolpath_ids\":\"\""
        "}"
        "},"
        "{"
        "\"command_id\":\"browser.setToolpathVisibility\","
        "\"schema_id\":\"browser.setToolpathVisibility.v1\","
        "\"inputs\":{"
        "\"target_object_id\":\"toolpath_op_drilling\","
        "\"visibility\":\"show\","
        "\"scope\":\"operation_type\","
        "\"operation_type\":\"drilling\","
        "\"toolpath_ids\":\"\""
        "}"
        "}"
        "],"
        "\"trace_events\":[\"draft_created\"]"
        "}";

    camclaw::AgentPlanDraftJsonCodec codec;
    const camclaw::AgentPlanDraftDecodeResult result = codec.decodeBackendDraft(json);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(2u, result.draft.stepCount());
    REQUIRE_EQ(std::string("browser.setToolpathVisibility"), result.draft.stepAt(0).skillId());
    REQUIRE_EQ(std::string("hide"), result.draft.stepAt(0).inputValue("visibility"));
    REQUIRE_EQ(std::string("pocket"), result.draft.stepAt(0).inputValue("operation_type"));
    REQUIRE_EQ(std::string("show"), result.draft.stepAt(1).inputValue("visibility"));
    REQUIRE_EQ(std::string("drilling"), result.draft.stepAt(1).inputValue("operation_type"));

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

    status = decodes_command_schema_backend_draft_json();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = decodes_action_sequence_backend_draft_json();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
