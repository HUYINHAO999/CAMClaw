#include "camclaw/AgentPlanExecutor.h"
#include "camclaw/HumanInLoopService.h"
#include "camclaw/TargetResolver.h"

#include <cstdlib>
#include <iostream>
#include <map>
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

static camclaw::CamObject pocket_operation(
    const std::string& object_id,
    const std::string& tool_id,
    const std::string& stepover)
{
    camclaw::CamObject operation(object_id, camclaw::ObjectType::Operation, "型腔铣");
    operation.attributes["operation_type"] = "pocket";
    operation.attributes["tool_id"] = tool_id;
    operation.attributes["stepover"] = stepover;
    return operation;
}

static int resolver_returns_unique_operation_without_human_loop()
{
    camclaw::Repository repository;
    repository.save(pocket_operation("op_pocket_001", "tool_010", "2.0"));

    std::map<std::string, std::string> query;
    query["operation_type"] = "pocket";
    camclaw::TargetResolver resolver(repository);
    const camclaw::TargetResolutionResult result = resolver.resolveOperation(query);

    REQUIRE_EQ(camclaw::TargetResolutionStatus::Resolved, result.status);
    REQUIRE_EQ(std::string("op_pocket_001"), result.target_object_id);

    return EXIT_SUCCESS;
}

static int resolver_returns_candidates_for_ambiguous_operation()
{
    camclaw::Repository repository;
    repository.save(pocket_operation("op_pocket_001", "tool_010", "2.0"));
    repository.save(pocket_operation("op_pocket_002", "tool_016", "1.0"));

    std::map<std::string, std::string> query;
    query["operation_type"] = "pocket";
    camclaw::TargetResolver resolver(repository);
    const camclaw::TargetResolutionResult result = resolver.resolveOperation(query);

    REQUIRE_EQ(camclaw::TargetResolutionStatus::NeedsClarification, result.status);
    REQUIRE_EQ(2u, result.candidates.size());
    REQUIRE_EQ(std::string("op_pocket_001"), result.candidates[0].id);
    REQUIRE_EQ(std::string("op_pocket_002"), result.candidates[1].id);

    return EXIT_SUCCESS;
}

static int human_loop_merges_last_candidate_into_draft_target()
{
    std::map<std::string, std::string> inputs;
    inputs["scope"] = "operation_type";
    inputs["operation_type"] = "pocket";
    inputs["parameter_name"] = "stepover";
    inputs["parameter_value"] = "1.0";
    inputs["recompute_toolpath"] = "true";
    camclaw::AgentPlanDraft draft("trace_hil_001");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.updateOperation", inputs));

    std::map<std::string, std::string> query;
    query["operation_type"] = "pocket";
    std::vector<camclaw::ClarificationCandidate> candidates;
    candidates.push_back(camclaw::ClarificationCandidate("op_pocket_001", "型腔铣", "tool_id=tool_010, stepover=2.0"));
    candidates.push_back(camclaw::ClarificationCandidate("op_pocket_002", "型腔铣", "tool_id=tool_016, stepover=1.0"));

    camclaw::ResponseSchema schema;
    schema.type = "choose_one";
    schema.target_field = "target_object_id";
    schema.allow_free_text = true;

    camclaw::HumanInLoopService service;
    const camclaw::ClarificationRequest request = service.createRequest(
        draft,
        0u,
        "ambiguous_target",
        "operation",
        "找到多个匹配工序，请选择要操作哪一个。",
        query,
        candidates,
        schema);

    camclaw::ClarificationResponse response;
    response.clarification_id = request.clarification_id;
    response.free_text = "最后一个";

    const camclaw::ClarificationResumeResult result = service.submitResponse(response);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("op_pocket_002"), result.resumed_draft.stepAt(0).inputValue("target_object_id"));
    REQUIRE_EQ(0u, service.pendingRequests().size());

    return EXIT_SUCCESS;
}

static int executor_requests_clarification_for_ambiguous_operation_target()
{
    camclaw::Repository repository;
    repository.save(pocket_operation("op_pocket_001", "tool_010", "2.0"));
    repository.save(pocket_operation("op_pocket_002", "tool_016", "1.0"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::AgentPlanExecutor executor(skill_runtime, repository, human_in_loop);

    std::map<std::string, std::string> inputs;
    inputs["scope"] = "operation_type";
    inputs["operation_type"] = "pocket";
    inputs["parameter_name"] = "stepover";
    inputs["parameter_value"] = "1.0";
    inputs["recompute_toolpath"] = "true";
    inputs["schema_id"] = "browser.updateOperation.v1";

    camclaw::AgentPlanDraft draft("trace_hil_execute");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.updateOperation", inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_TRUE(result.needs_clarification);
    REQUIRE_EQ(std::string("needs_clarification"), result.error_code);
    REQUIRE_EQ(std::string("ambiguous_target"), result.clarification.reason);
    REQUIRE_EQ(2u, result.clarification.candidates.size());
    REQUIRE_EQ(1u, human_in_loop.pendingRequests().size());

    return EXIT_SUCCESS;
}

static int executor_ignores_non_operation_current_target_for_operation_type_scope()
{
    camclaw::Repository repository;
    repository.save(pocket_operation("op_pocket_001", "tool_010", "2.0"));
    repository.save(pocket_operation("op_pocket_002", "tool_016", "1.0"));
    camclaw::CamObject current_toolpath("toolpath_op_pocket_002", camclaw::ObjectType::Toolpath, "型腔铣刀轨");
    current_toolpath.parent_object_id = "op_pocket_002";
    repository.save(current_toolpath);
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::AgentPlanExecutor executor(skill_runtime, repository, human_in_loop);

    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = "toolpath_op_pocket_002";
    inputs["scope"] = "operation_type";
    inputs["operation_type"] = "pocket";
    inputs["parameter_name"] = "stepover";
    inputs["parameter_value"] = "0.8";
    inputs["recompute_toolpath"] = "true";
    inputs["schema_id"] = "browser.updateOperation.v1";

    camclaw::AgentPlanDraft draft("trace_hil_toolpath_context");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.updateOperation", inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_TRUE(result.needs_clarification);
    REQUIRE_EQ(std::string("needs_clarification"), result.error_code);
    REQUIRE_EQ(2u, result.clarification.candidates.size());

    return EXIT_SUCCESS;
}

static int executor_does_not_treat_current_operation_as_precise_target_for_operation_type_scope()
{
    camclaw::Repository repository;
    repository.save(pocket_operation("op_pocket_001", "tool_010", "2.0"));
    repository.save(pocket_operation("op_pocket_002", "tool_016", "1.0"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime skill_runtime(gateway);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::AgentPlanExecutor executor(skill_runtime, repository, human_in_loop);

    std::map<std::string, std::string> inputs;
    inputs["target_object_id"] = "op_pocket_001";
    inputs["scope"] = "operation_type";
    inputs["operation_type"] = "pocket";
    inputs["parameter_name"] = "stepover";
    inputs["parameter_value"] = "0.8";
    inputs["recompute_toolpath"] = "true";
    inputs["schema_id"] = "browser.updateOperation.v1";

    camclaw::AgentPlanDraft draft("trace_hil_operation_context");
    draft.addSkillStep(camclaw::SkillStepDraft("browser.updateOperation", inputs));
    draft.confirm();

    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_TRUE(result.needs_clarification);
    REQUIRE_EQ(std::string("needs_clarification"), result.error_code);
    REQUIRE_EQ(2u, result.clarification.candidates.size());

    return EXIT_SUCCESS;
}

int main()
{
    int status = resolver_returns_unique_operation_without_human_loop();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = resolver_returns_candidates_for_ambiguous_operation();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = human_loop_merges_last_candidate_into_draft_target();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = executor_requests_clarification_for_ambiguous_operation_target();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = executor_ignores_non_operation_current_target_for_operation_type_scope();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = executor_does_not_treat_current_operation_as_precise_target_for_operation_type_scope();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
