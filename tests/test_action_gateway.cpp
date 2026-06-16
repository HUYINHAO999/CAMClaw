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

static int controlled_script_can_use_registered_console_command()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("op_existing", camclaw::ObjectType::Operation, "已有工序"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);

    camclaw::ConsoleCommandRequest request;
    request.command_id = "browser.generate_toolpath";
    request.source = "script";
    request.trace_id = "trace_script_001";
    request.target_object_id = "op_existing";

    const camclaw::ConsoleCommandResult result = gateway.dispatch(request);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("toolpath_op_existing"), result.primary_object_id);
    REQUIRE_TRUE(repository.exists("toolpath_op_existing"));
    REQUIRE_TRUE(contains_event(result.trace_events, "gateway_validated"));
    REQUIRE_TRUE(contains_event(result.trace_events, "browser_console_dispatched"));

    return EXIT_SUCCESS;
}

static int unsupported_script_command_is_blocked_before_console()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("op_existing", camclaw::ObjectType::Operation, "已有工序"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);

    camclaw::ConsoleCommandRequest request;
    request.command_id = "browser.delete_operation";
    request.source = "script";
    request.trace_id = "trace_script_002";
    request.target_object_id = "op_existing";

    const camclaw::ConsoleCommandResult result = gateway.dispatch(request);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("unsupported_command"), result.error_code);
    REQUIRE_TRUE(contains_event(result.trace_events, "gateway_rejected"));
    REQUIRE_TRUE(!contains_event(result.trace_events, "browser_console_dispatched"));

    return EXIT_SUCCESS;
}

static int repository_type_overrides_request_type_hint()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("op_existing", camclaw::ObjectType::Operation, "名字像型腔的工序"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);

    camclaw::ConsoleCommandRequest request;
    request.command_id = "browser.create_roughing_operation";
    request.source = "script";
    request.trace_id = "trace_script_003";
    request.target_object_id = "op_existing";
    request.target_object_type_hint = camclaw::ObjectType::Feature;

    const camclaw::ConsoleCommandResult result = gateway.dispatch(request);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("invalid_target_type"), result.error_code);
    REQUIRE_EQ(std::string("op_existing"), result.primary_object_id);
    REQUIRE_TRUE(!repository.exists("op_roughing_op_existing"));
    REQUIRE_TRUE(contains_event(result.trace_events, "gateway_rejected"));

    return EXIT_SUCCESS;
}

static int invalid_numeric_arguments_are_blocked_before_console()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);

    camclaw::ConsoleCommandRequest request;
    request.command_id = "browser.create_roughing_operation";
    request.source = "script";
    request.trace_id = "trace_script_004";
    request.target_object_id = "feature_001";
    request.args["operation_type"] = "roughing";
    request.args["tool_id"] = "tool_010";
    request.args["stepover"] = "-2.0";
    request.args["stepdown"] = "1.0";
    request.args["tolerance"] = "0.02";

    const camclaw::ConsoleCommandResult result = gateway.dispatch(request);

    REQUIRE_TRUE(!result.ok);
    REQUIRE_EQ(std::string("invalid_argument"), result.error_code);
    REQUIRE_TRUE(!repository.exists("op_roughing_feature_001"));
    REQUIRE_TRUE(contains_event(result.trace_events, "gateway_rejected"));
    REQUIRE_TRUE(!contains_event(result.trace_events, "browser_console_dispatched"));

    return EXIT_SUCCESS;
}

static int default_registry_matches_browser_console_support()
{
    camclaw::Repository repository;
    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);

    REQUIRE_TRUE(gateway.registryMatchesConsole());

    return EXIT_SUCCESS;
}

static int visibility_by_operation_type_hides_matching_operation_toolpaths_only()
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
    drilling_toolpath.attributes["visible"] = "true";
    repository.save(drilling_toolpath);

    camclaw::BrowserConsole browser_console(repository);
    camclaw::ActionGateway gateway(repository, browser_console);

    camclaw::ConsoleCommandRequest request;
    request.command_id = "browser.setToolpathVisibility";
    request.source = "agent";
    request.trace_id = "trace_visibility_operation_type";
    request.target_object_id = "toolpath_op_drilling_feature_holes_001";
    request.args["visibility"] = "hide";
    request.args["scope"] = "operation_type";
    request.args["operation_type"] = "pocket";

    const camclaw::ConsoleCommandResult result = gateway.dispatch(request);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("toolpath_op_pocket_feature_001"), result.primary_object_id);
    REQUIRE_EQ(std::string("false"), repository.get("toolpath_op_pocket_feature_001").attributes["visible"]);
    REQUIRE_EQ(std::string("true"), repository.get("toolpath_op_drilling_feature_holes_001").attributes["visible"]);

    return EXIT_SUCCESS;
}

int main()
{
    int status = controlled_script_can_use_registered_console_command();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = unsupported_script_command_is_blocked_before_console();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = repository_type_overrides_request_type_hint();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = invalid_numeric_arguments_are_blocked_before_console();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = default_registry_matches_browser_console_support();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = visibility_by_operation_type_hides_matching_operation_toolpaths_only();
    if (status != EXIT_SUCCESS) {
        return status;
    }

    return EXIT_SUCCESS;
}
