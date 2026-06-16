from __future__ import annotations

REQUIRED_ROUGHING_FIELDS = (
    "operation_type",
    "tool_id",
    "stepover",
    "stepdown",
    "tolerance",
)

SUPPORTED_COMMAND_IDS = (
    "browser.createOperation",
    "browser.updateOperation",
    "browser.generateToolpath",
    "browser.setToolpathVisibility",
)


def semantic_intent_plan_json_contract(tool_context: str = "") -> str:
    context = tool_context or "No tool catalog is available."
    return (
        "Return only strict JSON for a CAMClaw semantic intent plan. "
        "The LLM is a semantic parser, not a CAM executor. "
        "Do not name Qt slots, C++ methods, BrowserConsole commands, or repository internals. "
        "Use top-level fields schema_version and intents. schema_version must be \"1\". "
        "intents is an ordered array. Intents execute sequentially in array order. "
        "For combined requests, split them into small intents instead of inventing combined intent names. "
        "Supported intents: machine_feature, create_operations, edit_operation, regenerate_toolpath, set_toolpath_visibility. "
        "Targets use exactly one of these shapes. "
        "Objects target: {\"kind\":\"objects\",\"object_ids\":[\"...\"]}. "
        "Query target: {\"kind\":\"query\",\"object_type\":\"feature|operation|toolpath\",\"scope\":\"all|matching\",\"filters\":{...}}. "
        "Ref target: {\"kind\":\"ref\",\"ref\":\"previous.primary_object_ids\"}. "
        "Current selection is request context. If the user refers to the current selected object, use an objects target with the provided target_object_id. "
        "Use machine_feature when the user asks to machine/process the selected feature or workpiece. "
        "machine_feature fields: id, intent, target, auto_generate_toolpath, requirements. "
        "Use create_operations when the user asks to create operations. "
        "create_operations fields: id, intent, items, auto_generate_toolpath. items contains operation_type and count. "
        "Use edit_operation when the user asks to modify operation parameters or open an operation editor. "
        "edit_operation fields: id, intent, target, updates, open_editor. updates contains parameter and value or expression. "
        "Use regenerate_toolpath when the user asks to calculate/recalculate/regenerate toolpath for an existing operation. "
        "Use a ref target with ref previous.primary_object_ids when the regeneration refers to the previous intent result. "
        "Use set_toolpath_visibility when the user asks to show/hide/toggle toolpaths. "
        "set_toolpath_visibility fields: id, intent, actions. Each action has visibility and target. "
        "For all toolpaths use target {\"kind\":\"query\",\"object_type\":\"toolpath\",\"scope\":\"all\",\"filters\":{}}. "
        "For 型腔铣 use operation_type pocket. For 钻孔 use operation_type drilling. For 平面铣 use operation_type finishing. "
        "For operation-class toolpaths use a query target with object_type toolpath, scope matching, and filters.operation_type. "
        "For operation-class operations use a query target with object_type operation, scope matching, and filters.operation_type. "
        "When the user asks for a larger/smaller tool or smaller stepover, preserve the relative expression instead of choosing the final CAM value. "
        + context
        + " "
        "Do not execute CAM commands. Do not include prose."
    )


def roughing_plan_json_contract(tool_context: str = "") -> str:
    context = tool_context or "No tool catalog is available. If no tool context is provided, choose a reasonable roughing tool_id."
    return (
        "Return only a flat JSON object with exactly these non-empty string fields: "
        "operation_type, tool_id, stepover, stepdown, tolerance. "
        + context
        + " "
        "Use conservative roughing defaults when the user request omits parameters. "
        "Do not execute CAM commands. Do not include prose."
    )


def cam_action_plan_json_contract(tool_context: str = "") -> str:
    context = tool_context or "No tool catalog is available. If no tool context is provided, choose a reasonable tool_id."
    return (
        "Return only a JSON object for a CAMClaw action draft. "
        "Prefer top-level field actions: an array of action objects. Each action object uses string fields command_id, schema_id and an inputs object. "
        "Use one action for a single task and multiple actions for combined user requests such as hiding A and showing B, or creating several operation groups. "
        "Use command_id browser.createOperation for creating one or more operations. "
        "Use command_id browser.updateOperation for editing an existing operation parameter and optionally recomputing toolpath. "
        "Use command_id browser.generateToolpath for calculating toolpath for an existing operation. "
        "Use command_id browser.setToolpathVisibility for showing, hiding, or toggling toolpath visibility. "
        "For browser.createOperation use schema_id browser.createOperation.v1 and include inputs object fields: "
        "operation_type, tool_id, stepover, stepdown, tolerance, batch_count, auto_generate_toolpath. "
        "batch_count must be a positive integer string and defaults to 1. "
        "auto_generate_toolpath must be true when the user asks to machine/process/calculate/generate toolpath, and false when the user only asks to create operations. "
        "When target_object_id contains feature_holes, choose operation_type drilling. "
        "When target_object_id contains feature_plane, choose operation_type finishing. "
        "When target_object_id contains feature_001 or pocket/cavity wording, choose operation_type pocket. "
        "For drilling use a drill tool_id, stepover 0, positive stepdown, and conservative tolerance. "
        "For browser.updateOperation use schema_id browser.updateOperation.v1 and include inputs object fields: "
        "parameter_name, parameter_value, recompute_toolpath, scope, operation_type. "
        "Use scope operation_type and operation_type pocket/drilling/finishing when the user refers to an operation class instead of a specific operation id. "
        "For browser.generateToolpath use schema_id browser.generateToolpath.v1 and inputs may be empty. "
        "For browser.setToolpathVisibility use schema_id browser.setToolpathVisibility.v1 and include inputs object fields: "
        "visibility, scope, toolpath_ids, operation_type. visibility must be show, hide, or toggle. "
        "scope must be all, selected, list, or operation_type. toolpath_ids is a comma-separated string and is only required for list. "
        "Use scope operation_type when the user refers to a machining operation class, such as 型腔铣, 钻孔, or 平面铣. "
        "For 型腔铣 use operation_type pocket. For 钻孔 use operation_type drilling. For 平面铣 use operation_type finishing. "
        + context
        + " "
        "Do not execute CAM commands. Do not include prose."
    )
