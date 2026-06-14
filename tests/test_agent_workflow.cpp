#include "camclaw/AgentWorkflowService.h"

#include <cstdlib>
#include <algorithm>
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

static bool contains_event(const std::vector<std::string>& events, const std::string& event_name)
{
    return std::find(events.begin(), events.end(), event_name) != events.end();
}

static int missing_target_asks_to_confirm_current_selection()
{
    camclaw::Repository repository;
    camclaw::AgentWorkflowService service(repository);

    camclaw::RoughingWorkflowRequest request;
    request.trace_id = "trace_001";
    request.active_selection.has_object = true;
    request.active_selection.object_id = "feature_001";
    request.active_selection.object_type = camclaw::ObjectType::Feature;
    request.active_selection.display_name = "型腔 A";

    const camclaw::WorkflowResult result = service.submitRoughingAndToolpath(request);

    REQUIRE_EQ(camclaw::WorkflowStatus::NeedsTargetConfirmation, result.status);
    REQUIRE_EQ(std::string("feature_001"), result.confirmation.target_object_id);
    REQUIRE_EQ(camclaw::ObjectType::Feature, result.confirmation.target_object_type);
    REQUIRE_TRUE(result.created_object_ids.empty());
    REQUIRE_TRUE(result.trace_events.size() >= 1);

    return EXIT_SUCCESS;
}

static int explicit_target_creates_operation_and_toolpath()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));

    camclaw::AgentWorkflowService service(repository);

    camclaw::RoughingWorkflowRequest request;
    request.trace_id = "trace_002";
    request.target_object_id = "feature_001";

    const camclaw::WorkflowResult result = service.submitRoughingAndToolpath(request);

    REQUIRE_EQ(camclaw::WorkflowStatus::Completed, result.status);
    REQUIRE_EQ(std::string("op_roughing_feature_001"), result.primary_object_id);
    REQUIRE_EQ(2u, result.created_object_ids.size());
    REQUIRE_EQ(std::string("op_roughing_feature_001"), result.created_object_ids[0]);
    REQUIRE_EQ(std::string("toolpath_op_roughing_feature_001"), result.created_object_ids[1]);
    REQUIRE_TRUE(result.trace_events.size() >= 2);
    REQUIRE_TRUE(contains_event(result.trace_events, "gateway_validated"));
    REQUIRE_TRUE(contains_event(result.trace_events, "adapter_dispatched"));
    REQUIRE_TRUE(contains_event(result.trace_events, "browser_console_dispatched"));
    REQUIRE_TRUE(contains_event(result.trace_events, "roughing_operation_created"));
    REQUIRE_TRUE(contains_event(result.trace_events, "toolpath_generated"));
    REQUIRE_TRUE(repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_EQ(camclaw::ObjectType::Operation, repository.get("op_roughing_feature_001").object_type);
    REQUIRE_EQ(camclaw::ObjectType::Toolpath, repository.get("toolpath_op_roughing_feature_001").object_type);

    return EXIT_SUCCESS;
}

static int non_feature_target_is_rejected_before_execution()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("op_existing", camclaw::ObjectType::Operation, "已有工序"));

    camclaw::AgentWorkflowService service(repository);

    camclaw::RoughingWorkflowRequest request;
    request.trace_id = "trace_003";
    request.target_object_id = "op_existing";

    const camclaw::WorkflowResult result = service.submitRoughingAndToolpath(request);

    REQUIRE_EQ(camclaw::WorkflowStatus::Failed, result.status);
    REQUIRE_EQ(std::string("invalid_target_type"), result.error_code);
    REQUIRE_EQ(std::string("op_existing"), result.primary_object_id);
    REQUIRE_TRUE(result.created_object_ids.empty());
    REQUIRE_TRUE(contains_event(result.trace_events, "gateway_rejected"));
    REQUIRE_TRUE(!repository.exists("op_roughing_op_existing"));

    return EXIT_SUCCESS;
}

static int missing_target_without_selection_asks_for_feature_target()
{
    camclaw::Repository repository;
    camclaw::AgentWorkflowService service(repository);

    camclaw::RoughingWorkflowRequest request;
    request.trace_id = "trace_004";

    const camclaw::WorkflowResult result = service.submitRoughingAndToolpath(request);

    REQUIRE_EQ(camclaw::WorkflowStatus::NeedsValidTarget, result.status);
    REQUIRE_EQ(std::string("missing_target"), result.error_code);
    REQUIRE_TRUE(result.created_object_ids.empty());
    REQUIRE_TRUE(contains_event(result.trace_events, "target_required"));

    return EXIT_SUCCESS;
}

static int wrong_selection_type_asks_for_valid_feature_target()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("op_existing", camclaw::ObjectType::Operation, "已有工序"));

    camclaw::AgentWorkflowService service(repository);

    camclaw::RoughingWorkflowRequest request;
    request.trace_id = "trace_005";
    request.active_selection.has_object = true;
    request.active_selection.object_id = "op_existing";
    request.active_selection.object_type = camclaw::ObjectType::Operation;
    request.active_selection.display_name = "已有工序";

    const camclaw::WorkflowResult result = service.submitRoughingAndToolpath(request);

    REQUIRE_EQ(camclaw::WorkflowStatus::NeedsValidTarget, result.status);
    REQUIRE_EQ(std::string("invalid_selection_type"), result.error_code);
    REQUIRE_EQ(std::string("op_existing"), result.primary_object_id);
    REQUIRE_TRUE(result.created_object_ids.empty());
    REQUIRE_TRUE(contains_event(result.trace_events, "valid_target_required"));

    return EXIT_SUCCESS;
}

int main()
{
    int status = missing_target_asks_to_confirm_current_selection();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = explicit_target_creates_operation_and_toolpath();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = non_feature_target_is_rejected_before_execution();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = missing_target_without_selection_asks_for_feature_target();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = wrong_selection_type_asks_for_valid_feature_target();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
