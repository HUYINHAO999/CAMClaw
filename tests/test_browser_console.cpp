#include "camclaw/BrowserConsole.h"
#include "camclaw/SemanticIntentPlan.h"

#include <cstdlib>
#include <iostream>

#define REQUIRE_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "Requirement failed: " #condition " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

#define REQUIRE_EQ(expected, actual) \
    do { \
        if (!((expected) == (actual))) { \
            std::cerr << "Requirement failed: expected [" << (expected) << "] actual [" << (actual) << "] at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE; \
        } \
    } while (0)

static int machine_feature_creates_matching_operations_and_toolpaths()
{
    camclaw::Repository repository;
    repository.save(camclaw::CamObject("feature_holes_001", camclaw::ObjectType::Feature, "孔组 A"));
    repository.save(camclaw::CamObject("feature_001", camclaw::ObjectType::Feature, "型腔 A"));
    repository.save(camclaw::CamObject("feature_plane_001", camclaw::ObjectType::Feature, "平面 A"));
    camclaw::BrowserConsole browser_console(repository);

    std::vector<std::string> holes;
    holes.push_back("feature_holes_001");
    camclaw::BrowserConsoleResult result = browser_console.machineFeature(holes, true);
    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("drilling"), repository.get("op_drilling_feature_holes_001").attributes["operation_type"]);
    REQUIRE_TRUE(repository.exists("toolpath_op_drilling_feature_holes_001"));

    std::vector<std::string> pocket;
    pocket.push_back("feature_001");
    result = browser_console.machineFeature(pocket, true);
    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("pocket"), repository.get("op_pocket_feature_001").attributes["operation_type"]);
    REQUIRE_TRUE(repository.exists("toolpath_op_pocket_feature_001"));

    std::vector<std::string> plane;
    plane.push_back("feature_plane_001");
    result = browser_console.machineFeature(plane, true);
    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("finishing"), repository.get("op_finishing_feature_plane_001").attributes["operation_type"]);
    REQUIRE_TRUE(repository.exists("toolpath_op_finishing_feature_plane_001"));

    return EXIT_SUCCESS;
}

static int create_operations_supports_batch_without_toolpaths()
{
    camclaw::Repository repository;
    camclaw::BrowserConsole browser_console(repository);
    camclaw::BrowserOperationCreateItem drilling;
    drilling.operation_type = "drilling";
    drilling.count = 5;
    camclaw::BrowserOperationCreateItem pocket;
    pocket.operation_type = "pocket";
    pocket.count = 3;
    camclaw::BrowserOperationCreateItem finishing;
    finishing.operation_type = "finishing";
    finishing.count = 4;
    std::vector<camclaw::BrowserOperationCreateItem> items;
    items.push_back(drilling);
    items.push_back(pocket);
    items.push_back(finishing);

    const camclaw::BrowserConsoleResult result = browser_console.createOperations(items, "", false);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(12u, repository.objectsByType(camclaw::ObjectType::Operation).size());
    REQUIRE_EQ(0u, repository.objectsByType(camclaw::ObjectType::Toolpath).size());
    REQUIRE_TRUE(repository.exists("op_drilling"));
    REQUIRE_TRUE(repository.exists("op_drilling_5"));
    REQUIRE_TRUE(repository.exists("op_pocket_3"));
    REQUIRE_TRUE(repository.exists("op_finishing_4"));

    return EXIT_SUCCESS;
}

static int visibility_actions_can_show_all_and_mix_operation_types()
{
    camclaw::Repository repository;
    camclaw::BrowserConsole browser_console(repository);
    std::vector<camclaw::BrowserOperationCreateItem> items;
    camclaw::BrowserOperationCreateItem pocket;
    pocket.operation_type = "pocket";
    camclaw::BrowserOperationCreateItem drilling;
    drilling.operation_type = "drilling";
    items.push_back(pocket);
    items.push_back(drilling);
    camclaw::BrowserConsoleResult result = browser_console.createOperations(items, "", true);
    REQUIRE_TRUE(result.ok);

    std::vector<std::string> all_toolpaths;
    all_toolpaths.push_back("toolpath_op_pocket");
    all_toolpaths.push_back("toolpath_op_drilling");
    result = browser_console.setToolpathVisibility(all_toolpaths, "hide");
    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("false"), repository.get("toolpath_op_pocket").attributes["visible"]);
    REQUIRE_EQ(std::string("false"), repository.get("toolpath_op_drilling").attributes["visible"]);

    std::vector<std::string> pocket_only;
    pocket_only.push_back("toolpath_op_pocket");
    result = browser_console.setToolpathVisibility(pocket_only, "show");
    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("true"), repository.get("toolpath_op_pocket").attributes["visible"]);
    REQUIRE_EQ(std::string("false"), repository.get("toolpath_op_drilling").attributes["visible"]);

    return EXIT_SUCCESS;
}

static int semantic_sequence_edits_then_regenerates_through_browser_console()
{
    camclaw::Repository repository;
    camclaw::CamObject operation("op_pocket_001", camclaw::ObjectType::Operation, "型腔铣");
    operation.attributes["operation_type"] = "pocket";
    operation.attributes["tool_id"] = "tool_010";
    operation.attributes["stepover"] = "2.0";
    repository.save(operation);
    camclaw::CamObject old_toolpath("toolpath_op_pocket_001", camclaw::ObjectType::Toolpath, "旧刀轨");
    old_toolpath.parent_object_id = "op_pocket_001";
    old_toolpath.attributes["visible"] = "true";
    repository.save(old_toolpath);

    camclaw::SemanticPlanDraft draft("trace_sequence");
    camclaw::SemanticIntent edit;
    edit.id = "i1";
    edit.kind = camclaw::SemanticIntentKind::EditOperation;
    edit.target.kind = camclaw::SemanticTargetKind::Query;
    edit.target.object_type = "operation";
    edit.target.scope = "matching";
    edit.target.filters["operation_type"] = "pocket";
    camclaw::ParameterUpdateIntent update;
    update.parameter = "tool";
    update.expression = "larger";
    edit.updates.push_back(update);
    draft.addIntent(edit);
    camclaw::SemanticIntent regenerate;
    regenerate.id = "i2";
    regenerate.kind = camclaw::SemanticIntentKind::RegenerateToolpath;
    regenerate.target.kind = camclaw::SemanticTargetKind::Ref;
    regenerate.target.ref = "previous.primary_object_ids";
    draft.addIntent(regenerate);
    draft.confirm();

    camclaw::BrowserConsole browser_console(repository);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::SemanticIntentExecutor executor(browser_console, human_in_loop);
    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("tool_016"), repository.get("op_pocket_001").attributes["tool_id"]);
    REQUIRE_TRUE(repository.exists("toolpath_op_pocket_001"));
    REQUIRE_EQ(std::string("calculated"), repository.get("op_pocket_001").attributes["toolpath_status"]);

    return EXIT_SUCCESS;
}

int main()
{
    int status = machine_feature_creates_matching_operations_and_toolpaths();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = create_operations_supports_batch_without_toolpaths();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = visibility_actions_can_show_all_and_mix_operation_types();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = semantic_sequence_edits_then_regenerates_through_browser_console();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    return EXIT_SUCCESS;
}
