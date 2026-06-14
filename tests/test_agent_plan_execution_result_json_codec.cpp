#include "camclaw/AgentPlanExecutionResultJsonCodec.h"

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

static int encodes_execution_result_for_ui_or_backend_feedback()
{
    camclaw::AgentPlanExecutionResult result;
    result.ok = true;
    result.primary_object_id = "op_roughing_feature_001";
    result.object_ids.push_back("op_roughing_feature_001");
    result.object_ids.push_back("toolpath_op_roughing_feature_001");
    result.trace_events.push_back("draft_execution_completed");

    camclaw::AgentPlanExecutionResultJsonCodec codec;
    const std::string json = codec.encode(result);

    REQUIRE_TRUE(json.find("\"ok\":true") != std::string::npos);
    REQUIRE_TRUE(json.find("\"primary_object_id\":\"op_roughing_feature_001\"") != std::string::npos);
    REQUIRE_TRUE(json.find("\"object_ids\":[\"op_roughing_feature_001\",\"toolpath_op_roughing_feature_001\"]") != std::string::npos);
    REQUIRE_TRUE(json.find("\"trace_events\":[\"draft_execution_completed\"]") != std::string::npos);

    return EXIT_SUCCESS;
}

int main()
{
    return encodes_execution_result_for_ui_or_backend_feedback();
}
