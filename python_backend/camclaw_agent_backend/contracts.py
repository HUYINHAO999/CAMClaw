from __future__ import annotations

REQUIRED_ROUGHING_FIELDS = (
    "operation_type",
    "tool_id",
    "stepover",
    "stepdown",
    "tolerance",
)


def roughing_plan_json_contract() -> str:
    return (
        "Return only a flat JSON object with string fields: "
        "operation_type, tool_id, stepover, stepdown, tolerance. "
        "Do not execute CAM commands. Do not include prose."
    )
