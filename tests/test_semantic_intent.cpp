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

static int semantic_codec_decodes_visibility_plan()
{
    const std::string json =
        "{"
        "\"schema_version\":\"1\","
        "\"trace_id\":\"trace_semantic_visibility\","
        "\"status\":\"pending_review\","
        "\"intents\":[{"
        "\"id\":\"i1\","
        "\"intent\":\"set_toolpath_visibility\","
        "\"actions\":["
        "{\"visibility\":\"hide\",\"target\":{\"kind\":\"query\",\"object_type\":\"toolpath\",\"scope\":\"matching\",\"filters\":{\"operation_type\":\"pocket\"}}},"
        "{\"visibility\":\"show\",\"target\":{\"kind\":\"query\",\"object_type\":\"toolpath\",\"scope\":\"matching\",\"filters\":{\"operation_type\":\"drilling\"}}}"
        "]"
        "}]"
        "}";

    camclaw::SemanticPlanDraftJsonCodec codec;
    const camclaw::SemanticPlanDraftDecodeResult result = codec.decodeBackendDraft(json);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("trace_semantic_visibility"), result.draft.traceId());
    REQUIRE_EQ(1u, result.draft.intentCount());
    REQUIRE_EQ(2u, result.draft.intentAt(0).visibility_actions.size());
    REQUIRE_EQ(std::string("pocket"), result.draft.intentAt(0).visibility_actions[0].target.filters.find("operation_type")->second);

    return EXIT_SUCCESS;
}

static int semantic_executor_hides_and_shows_toolpaths_by_query()
{
    camclaw::Repository repository;
    camclaw::CamObject pocket_operation("op_pocket_001", camclaw::ObjectType::Operation, "型腔铣");
    pocket_operation.attributes["operation_type"] = "pocket";
    pocket_operation.attributes["toolpath_id"] = "toolpath_op_pocket_001";
    repository.save(pocket_operation);
    camclaw::CamObject drilling_operation("op_drilling_001", camclaw::ObjectType::Operation, "钻孔");
    drilling_operation.attributes["operation_type"] = "drilling";
    drilling_operation.attributes["toolpath_id"] = "toolpath_op_drilling_001";
    repository.save(drilling_operation);
    camclaw::CamObject pocket_toolpath("toolpath_op_pocket_001", camclaw::ObjectType::Toolpath, "型腔铣刀轨");
    pocket_toolpath.parent_object_id = "op_pocket_001";
    pocket_toolpath.attributes["visible"] = "true";
    repository.save(pocket_toolpath);
    camclaw::CamObject drilling_toolpath("toolpath_op_drilling_001", camclaw::ObjectType::Toolpath, "钻孔刀轨");
    drilling_toolpath.parent_object_id = "op_drilling_001";
    drilling_toolpath.attributes["visible"] = "false";
    repository.save(drilling_toolpath);

    camclaw::SemanticIntent intent;
    intent.id = "i1";
    intent.kind = camclaw::SemanticIntentKind::SetToolpathVisibility;
    camclaw::VisibilityIntentAction hide;
    hide.visibility = "hide";
    hide.target.kind = camclaw::SemanticTargetKind::Query;
    hide.target.object_type = "toolpath";
    hide.target.scope = "matching";
    hide.target.filters["operation_type"] = "pocket";
    intent.visibility_actions.push_back(hide);
    camclaw::VisibilityIntentAction show;
    show.visibility = "show";
    show.target.kind = camclaw::SemanticTargetKind::Query;
    show.target.object_type = "toolpath";
    show.target.scope = "matching";
    show.target.filters["operation_type"] = "drilling";
    intent.visibility_actions.push_back(show);

    camclaw::SemanticPlanDraft draft("trace_semantic_visibility");
    draft.addIntent(intent);
    draft.confirm();

    camclaw::BrowserConsole browser_console(repository);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::SemanticIntentExecutor executor(browser_console, human_in_loop);
    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("false"), repository.get("toolpath_op_pocket_001").attributes["visible"]);
    REQUIRE_EQ(std::string("true"), repository.get("toolpath_op_drilling_001").attributes["visible"]);

    return EXIT_SUCCESS;
}

static int semantic_executor_edits_operation_and_regenerates_ref_toolpath()
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

    camclaw::SemanticPlanDraft draft("trace_semantic_edit_regen");
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

    return EXIT_SUCCESS;
}

static int semantic_codec_preserves_null_value_and_expression_update()
{
    const std::string json =
        "{"
        "\"schema_version\":\"1\","
        "\"trace_id\":\"trace_semantic_expression\","
        "\"status\":\"pending_review\","
        "\"intents\":[{"
        "\"id\":\"i1\","
        "\"intent\":\"edit_operation\","
        "\"target\":{\"kind\":\"objects\",\"object_ids\":[\"op_pocket\"]},"
        "\"updates\":[{\"parameter\":\"tool_id\",\"value\":null,\"expression\":\"larger_available_tool\"}],"
        "\"open_editor\":true"
        "}]"
        "}";

    camclaw::SemanticPlanDraftJsonCodec codec;
    const camclaw::SemanticPlanDraftDecodeResult result = codec.decodeBackendDraft(json);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(1u, result.draft.intentCount());
    REQUIRE_EQ(1u, result.draft.intentAt(0).updates.size());
    REQUIRE_EQ(std::string("tool_id"), result.draft.intentAt(0).updates[0].parameter);
    REQUIRE_EQ(std::string(""), result.draft.intentAt(0).updates[0].value);
    REQUIRE_EQ(std::string("larger_available_tool"), result.draft.intentAt(0).updates[0].expression);

    return EXIT_SUCCESS;
}

static int semantic_executor_resolves_larger_available_tool_expression()
{
    camclaw::Repository repository;
    camclaw::CamObject operation("op_pocket_001", camclaw::ObjectType::Operation, "型腔铣");
    operation.attributes["operation_type"] = "pocket";
    operation.attributes["tool_id"] = "tool_010";
    repository.save(operation);

    camclaw::SemanticPlanDraft draft("trace_semantic_larger_tool");
    camclaw::SemanticIntent edit;
    edit.id = "i1";
    edit.kind = camclaw::SemanticIntentKind::EditOperation;
    edit.target.kind = camclaw::SemanticTargetKind::Objects;
    edit.target.object_ids.push_back("op_pocket_001");
    camclaw::ParameterUpdateIntent update;
    update.parameter = "tool_id";
    update.expression = "larger_available_tool";
    edit.updates.push_back(update);
    draft.addIntent(edit);
    draft.confirm();

    camclaw::BrowserConsole browser_console(repository);
    camclaw::HumanInLoopService human_in_loop;
    camclaw::SemanticIntentExecutor executor(browser_console, human_in_loop);
    const camclaw::AgentPlanExecutionResult result = executor.execute(draft);

    REQUIRE_TRUE(result.ok);
    REQUIRE_EQ(std::string("tool_016"), repository.get("op_pocket_001").attributes["tool_id"]);

    return EXIT_SUCCESS;
}

int main()
{
    int status = semantic_codec_decodes_visibility_plan();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = semantic_executor_hides_and_shows_toolpaths_by_query();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = semantic_executor_edits_operation_and_regenerates_ref_toolpath();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = semantic_codec_preserves_null_value_and_expression_update();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    status = semantic_executor_resolves_larger_available_tool_expression();
    if (status != EXIT_SUCCESS) {
        return status;
    }
    return EXIT_SUCCESS;
}
