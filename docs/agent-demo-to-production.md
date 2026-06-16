# Agent Demo To Production Notes

This document records how the current Agent review demo should evolve into a production CAM integration.

## Current Demo Shape

The demo proves this flow:

1. Python backend calls the LLM and produces an `AgentPlanDraft`.
2. The user reviews editable inputs such as `tool_id`, `stepover`, `stepdown`, and `tolerance`.
3. The reviewed draft is confirmed or rejected.
4. Confirmed drafts execute through the controlled Skill / Gateway / Console path.

The current Python backend uses `python_backend/config/tool_library.json` as a small file-backed `ToolLibrary`.

This file is only a demo substitute for an enterprise tool data source.

## Production Tool Library Direction

Production systems may have hundreds or thousands of tools. They should not be hardcoded in prompts.

The future `ToolLibrary` should become an interface over an enterprise data source, for example:

- CAM project tool table
- Tool crib database
- ERP / MES tool master
- Repository-backed canonical Tool objects
- A dedicated tool recommendation service

The planner should ask the `ToolLibrary` for a small, relevant planning context instead of receiving the full catalog.

## Possible Retrieval Path

A production path may look like this:

1. Agent Planner receives user request, target object, and rejection reason.
2. Planner asks a `ToolLibrary` capability for candidate tools.
3. `ToolLibrary` queries a DB or service using structured filters:
   - operation type
   - material
   - target feature type
   - diameter range
   - holder constraints
   - machine constraints
   - user feedback such as "tool too large"
4. If natural language needs to become a query, Text-to-SQL may be used inside the backend service boundary.
5. The result is normalized into a small candidate list with stable `tool_id` values.
6. The LLM receives only this candidate list and must choose one stable `tool_id`.
7. The selected `tool_id` remains editable in `AgentPlanDraft` before execution.

Text-to-SQL should not directly execute CAM commands. It only helps retrieve planning context.

## Stable Boundary

Even if the tool source changes from JSON to DB, these boundaries should remain stable:

- LLM / Agent Planner proposes structured draft inputs.
- The user reviews the `AgentPlanDraft`.
- Skills do not choose tools or machining parameters.
- C++ / Qt execution receives structured IDs and parameters.
- Gateway validates before reaching Component Console.
- Component Console coordinates lower layers instead of containing LLM logic.

## Suggested ToolLibrary Interface

Conceptually:

```text
ToolLibrary.find_candidates(context) -> ToolCandidateList
```

Where `context` may include:

- `target_object_id`
- `target_object_type`
- `operation_type`
- `material_id`
- `machine_id`
- `current_tool_id`
- `rejection_reason`
- `max_candidates`

And each candidate should include:

- stable `tool_id`
- display name
- diameter
- tool type
- material suitability
- source / revision
- optional explanation for why it is a candidate

## Demo Migration Rule

When moving this demo to Qt, do not bake `tool_library.json` into UI code.

Keep the UI concerned with review and confirmation only:

- show candidate-derived `tool_id`
- allow the user to edit it
- reject and regenerate when the whole draft is wrong
- confirm only reviewed structured data

The backend can later replace `ToolLibrary.from_file()` with DB, Repository, or service-backed implementations without changing the review UI contract.
