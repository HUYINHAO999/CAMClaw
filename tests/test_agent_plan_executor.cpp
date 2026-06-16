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
    REQUIRE_TRUE(!repository.exists("toolpath_op_roughing_feature_001"));
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

static int command_draft_batch_creates_operations()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_holes_001", camclaw::ObjectType::Feature, "孔组 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = "feature_holes_001";
    inputs["operation_type"] = "drilling";
    inputs["tool_id"] = "tool_006";
    inputs["stepover"] = "0";
    inputs["stepdown"] = "3";
    inputs["tolerance"] = "0.02";
    inputs["batch_count"] = "10";
    inputs["schema_id"] = "browser.createOperation.v1";

    camclaw::AgentPlanDraft draft("trace_execute_batch");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.createOperation", inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(10u, result.object_ids.size());
    REQUIRE_TRUE(repository.exists("op_drilling_feature_holes_001"));
    REQUIRE_TRUE(repository.exists("op_drilling_feature_holes_001_10"));
    REQUIRE_TRUE(!repository.exists("toolpath_op_drilling_feature_holes_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "operation_created"));

    return EXIT_SUCCESS;
}

static int command_draft_auto_generates_toolpath_for_single_operation()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_holes_001", camclaw::ObjectType::Feature, "孔组 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = "feature_holes_001";
    inputs["operation_type"] = "drilling";
    inputs["tool_id"] = "drill_006";
    inputs["stepover"] = "0";
    inputs["stepdown"] = "3";
    inputs["tolerance"] = "0.02";
    inputs["batch_count"] = "1";
    inputs["auto_generate_toolpath"] = "true";
    inputs["schema_id"] = "browser.createOperation.v1";

    camclaw::AgentPlanDraft draft("trace_execute_auto_toolpath");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.createOperation", inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("op_drilling_feature_holes_001"), result.primary_object_id);
    REQUIRE_TRUE(repository.exists("op_drilling_feature_holes_001"));
    REQUIRE_TRUE(repository.exists("toolpath_op_drilling_feature_holes_001"));
    const camclaw::CamObject toolpath = repository.get("toolpath_op_drilling_feature_holes_001");
    REQUIRE_EQ(std::string("钻孔刀轨"), toolpath.display_name);
    REQUIRE_EQ(std::string("op_drilling_feature_holes_001"), toolpath.parent_object_id);
    REQUIRE_TRUE(toolpath.attributes.find("operation_type") == toolpath.attributes.end());
    REQUIRE_TRUE(contains_event(result.trace_events, "toolpath_generated"));

    return EXIT_SUCCESS;
}

static int command_draft_updates_operation_and_recomputes_toolpath()
{
    camclaw::Repository repository;
    camclaw::CamObject operation("op_roughing_feature_001", camclaw::ObjectType::Operation, "型腔铣");
    operation.attributes["stepover"] = "2";
    repository.save(operation);
    repository.save(camclaw::CamObject("toolpath_op_roughing_feature_001", camclaw::ObjectType::Toolpath, "旧刀轨"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = "op_roughing_feature_001";
    inputs["parameter_name"] = "stepover";
    inputs["parameter_value"] = "0.8";
    inputs["recompute_toolpath"] = "true";
    inputs["schema_id"] = "browser.updateOperation.v1";

    camclaw::AgentPlanDraft draft("trace_execute_update");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.updateOperation", inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("0.8"), repository.get("op_roughing_feature_001").attributes["stepover"]);
    REQUIRE_TRUE(repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "operation_updated"));
    REQUIRE_TRUE(contains_event(result.trace_events, "toolpath_generated"));

    return EXIT_SUCCESS;
}

static int command_draft_executes_visibility_action_sequence()
{
    camclaw::Repository repository;
    camclaw::CamObject pocket_operation("op_pocket_feature_001", camclaw::ObjectType::Operation, "型腔铣");
    pocket_operation.attributes["operation_type"] = "pocket";
    pocket_operation.attributes["toolpath_id"] = "toolpath_op_pocket_feature_001";
    repository.save(pocket_operation);
    camclaw::CamObject drilling_operation("op_drilling_feature_holes_001", camclaw::ObjectType::Operation, "钻孔");
    drilling_operation.attributes["operation_type"] = "drilling";
    drilling_operation.attributes["toolpath_id"] = "toolpath_op_drilling_feature_holes_001";
    repository.save(drilling_operation);
    camclaw::CamObject pocket_toolpath("toolpath_op_pocket_feature_001", camclaw::ObjectType::Toolpath, "型腔铣刀轨");
    pocket_toolpath.parent_object_id = "op_pocket_feature_001";
    pocket_toolpath.attributes["visible"] = "true";
    repository.save(pocket_toolpath);
    camclaw::CamObject drilling_toolpath("toolpath_op_drilling_feature_holes_001", camclaw::ObjectType::Toolpath, "钻孔刀轨");
    drilling_toolpath.parent_object_id = "op_drilling_feature_holes_001";
    drilling_toolpath.attributes["visible"] = "false";
    repository.save(drilling_toolpath);

    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::AgentPlanExecutor executor(skill_runtime);

    std::map<std::string, std::string> hide_inputs;
    hide_inputs["target_object_id"] = "toolpath_op_drilling_feature_holes_001";
    hide_inputs["visibility"] = "hide";
    hide_inputs["scope"] = "operation_type";
    hide_inputs["operation_type"] = "pocket";
    hide_inputs["schema_id"] = "browser.setToolpathVisibility.v1";
    std::map<std::string, std::string> show_inputs;
    show_inputs["target_object_id"] = "toolpath_op_drilling_feature_holes_001";
    show_inputs["visibility"] = "show";
    show_inputs["scope"] = "operation_type";
    show_inputs["operation_type"] = "drilling";
    show_inputs["schema_id"] = "browser.setToolpathVisibility.v1";

    camclaw::AgentPlanDraft draft("trace_execute_visibility_sequence");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.setToolpathVisibility", hide_inputs));
    draft.addSkillStep(camclaw::SkillStepDraft("browser.setToolpathVisibility", show_inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("false"), repository.get("toolpath_op_pocket_feature_001").attributes["visible"]);
    REQUIRE_EQ(std::string("true"), repository.get("toolpath_op_drilling_feature_holes_001").attributes["visible"]);
    REQUIRE_TRUE(contains_event(result.trace_events, "toolpath_hidden"));
    REQUIRE_TRUE(contains_event(result.trace_events, "toolpath_shown"));

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

    status = command_draft_batch_creates_operations();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = command_draft_auto_generates_toolpath_for_single_operation();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = command_draft_updates_operation_and_recomputes_toolpath();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = command_draft_executes_visibility_action_sequence();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
