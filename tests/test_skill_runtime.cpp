#include "camclaw/SkillRuntime.h"

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

static camclaw::SkillDefinition create_roughing_skill()
{
    camclaw::SkillDefinition skill;
    skill.skill_id = "browser.create_roughing_operation_and_generate_toolpath";

    camclaw::SkillCommandStep create_operation;
    create_operation.command_id = "browser.create_roughing_operation";
    create_operation.target_object_id_expression = "$input.target_object_id";
    skill.steps.push_back(create_operation);

    camclaw::SkillCommandStep generate_toolpath;
    generate_toolpath.command_id = "browser.generate_toolpath";
    generate_toolpath.target_object_id_expression = "$step0.primary_object_id";
    skill.steps.push_back(generate_toolpath);

    return skill;
}

static int skill_executes_steps_and_binds_primary_object_id()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime runtime(gateway);

    camclaw::SkillExecutionInput input;
    input.trace_id = "trace_skill_001";
    input.target_object_id = "feature_001";

    const camclaw::SkillExecutionResult result = runtime.execute(create_roughing_skill(), input);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(-1, result.failed_step_index);
    REQUIRE_EQ(std::string("toolpath_op_roughing_feature_001"), result.primary_object_id);
    REQUIRE_TRUE(repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "roughing_operation_created"));
    REQUIRE_TRUE(contains_event(result.trace_events, "toolpath_generated"));
    REQUIRE_TRUE(contains_event(result.trace_events, "skill_completed"));

    return EXIT_SUCCESS;
}

static int skill_stops_on_strict_binding_failure_without_rollback()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);
    camclaw::SkillRuntime runtime(gateway);

    camclaw::SkillDefinition skill = create_roughing_skill();
    skill.steps[1].target_object_id_expression = "$step3.primary_object_id";

    camclaw::SkillExecutionInput input;
    input.trace_id = "trace_skill_002";
    input.target_object_id = "feature_001";

    const camclaw::SkillExecutionResult result = runtime.execute(skill, input);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(1, result.failed_step_index);
    REQUIRE_EQ(std::string("unresolved_binding"), result.error_code);
    REQUIRE_TRUE(repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(!repository.exists("toolpath_op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "skill_binding_failed"));

    return EXIT_SUCCESS;
}

int main()
{
    int status = skill_executes_steps_and_binds_primary_object_id();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = skill_stops_on_strict_binding_failure_without_rollback();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
