# Agent Demo To Production Notes

This document records how the current Agent review demo should evolve into a production CAM integration.

## Current Demo Shape

The demo proves this flow:

1. Python backend calls the LLM and produces an `AgentPlanDraft`.
2. The user reviews editable inputs such as `tool_id`, `stepover`, `stepdown`, and `tolerance`.
3. The reviewed draft is confirmed or rejected.
4. Confirmed drafts execute through the controlled Skill / Gateway / Console path.

The Python backend no longer uses a file-backed tool library. It asks the LLM to preserve relative tool intent, such as `larger_available_tool` or `smaller_available_tool`, and leaves final tool selection to the CAM execution side.

The current Qt / C++ prototype resolves those expressions through `ToolSelectionPolicy`.

## Production Tool Library Direction

Production systems may have hundreds or thousands of tools. They should not be hardcoded in prompts.

The future `ToolLibrary` should become an interface over an enterprise data source, for example:

- CAM project tool table
- Tool crib database
- ERP / MES tool master
- Repository-backed canonical Tool objects
- A dedicated tool recommendation service

The CAM execution side should own final tool selection. If a production planner needs tool hints, it should ask a tool capability for a small, relevant planning context without making the LLM the authoritative decision maker.

## Possible Retrieval Path

A production path may look like this:

1. Agent Planner receives user request, target object, and rejection reason.
2. CAM service asks a `ToolLibrary` capability for candidate tools.
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
6. The CAM side applies a deterministic tool selection policy.
7. The resolved `tool_id` remains visible and editable before or after execution.

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

Do not bake a demo `tool_library.json` into UI code or prompt code.

Keep the UI concerned with review and confirmation only:

- show CAM-resolved `tool_id`
- allow the user to edit it
- reject and regenerate when the whole draft is wrong
- confirm only reviewed structured data

The CAM side can later replace the current in-code mock policy with DB, Repository, or service-backed implementations without changing the LLM semantic intent contract.
