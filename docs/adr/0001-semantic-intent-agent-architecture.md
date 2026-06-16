# ADR 0001: Use Semantic Intent Plans for CAMClaw Agent

## Status

Accepted.

## Context

CAMClaw is being developed as a traditional CAM application first, then enhanced with LLM-driven workflows. This matches how many enterprise products adopt AI: existing software already owns the stable business model, UI, validation, and execution path, while AI is added as another interaction entry point.

The first Qt CAM prototype let the LLM return concrete action drafts using fields such as `command_id`, `schema_id`, and `inputs`. That proved useful for quickly validating workflows such as creating operations, generating toolpaths, hiding toolpaths, and editing operation parameters. However, it also exposed architectural problems:

- `command_id` mixed LLM-facing tool names, Qt/browser routing, C++ command identifiers, and schema selection.
- LLM output could contain empty or conflicting fields, such as an empty `operation_type`.
- Natural language has too many variants to safely map directly to concrete execution events.
- Some requests are naturally multi-step, such as "edit this operation, then recompute", and should not become large combined commands.
- Current selection, target query, object ids, and previous-step references were starting to mix inside one loose target vocabulary.
- C++ execution code was tempted to infer business intent from selected objects, which would make the core CAM layer brittle.

The design needs to keep the traditional CAM backend authoritative for CAM decisions: feature recognition, operation selection, tool selection, parameter policies, target resolution, toolpath generation, and UI refresh.

## Decision

The LLM will act as a semantic parser, not as a concrete execution planner.

The Python backend will ask the LLM to return a `SemanticIntentPlan`, validate it with Pydantic, normalize deterministic language variants, and then pass a validated plan to Qt/C++. Qt/C++ will dispatch those semantic intents into traditional CAM application services.

The target model will use three mutually exclusive forms:

- `objects`: explicit object ids.
- `query`: object type plus filters.
- `ref`: reference to a previous intent result.

Current Qt selection is request context, not a target kind. If the user means the current selection, the Python planner may turn selected ids from the request context into an `objects` target.

Complex user requests are represented as ordered intent sequences. For now, intents execute in array order. Dependency graphs such as `depends_on` are deferred until required.

Examples:

```json
{
  "schema_version": "1",
  "intents": [
    {
      "id": "i1",
      "intent": "edit_operation",
      "target": {
        "kind": "query",
        "object_type": "operation",
        "scope": "matching",
        "filters": {
          "operation_type": "pocket"
        }
      },
      "updates": [
        {
          "parameter": "tool",
          "expression": "larger"
        }
      ],
      "open_editor": true
    },
    {
      "id": "i2",
      "intent": "regenerate_toolpath",
      "target": {
        "kind": "ref",
        "ref": "previous.primary_object_ids"
      }
    }
  ]
}
```

```json
{
  "schema_version": "1",
  "intents": [
    {
      "id": "i1",
      "intent": "set_toolpath_visibility",
      "actions": [
        {
          "visibility": "hide",
          "target": {
            "kind": "query",
            "object_type": "toolpath",
            "scope": "matching",
            "filters": {
              "operation_type": "pocket"
            }
          }
        },
        {
          "visibility": "show",
          "target": {
            "kind": "query",
            "object_type": "toolpath",
            "scope": "matching",
            "filters": {
              "operation_type": "drilling"
            }
          }
        }
      ]
    }
  ]
}
```

## Boundaries

LLM responsibilities:

- Parse natural language into a small set of standard semantic intents.
- Preserve user meaning such as "larger tool", "smaller stepover", "hide pocket toolpaths".
- Avoid naming Qt slots, C++ methods, browser console internals, or repository ids unless ids are present in request context.

Python backend responsibilities:

- Validate HTTP request payloads with Pydantic.
- Validate LLM raw JSON with Pydantic before producing a draft.
- Normalize deterministic language variants and obvious aliases.
- Reject or interrupt when required semantic information is missing.
- Return validated `SemanticIntentPlan` JSON directly to Qt.

Qt/C++ responsibilities:

- Decode the validated semantic plan.
- Resolve targets using repository state.
- Ask HumanInLoop when a semantic target is ambiguous or missing.
- Execute CAM behavior through application services, not through LLM-chosen commands.
- Keep feature recognition, operation strategy, tool selection, parameter policies, toolpath generation, and UI refresh in traditional code.

HumanInLoop responsibilities:

- Handle uncertainty, not arbitrary business repair.
- Examples: missing operation type, multiple matching operations, ambiguous object target, conflicting intent.

## Migration Plan

Phase 1:

- Add Python Pydantic models for `AgentPlanRequest`, `SemanticIntentPlan`, target selectors, and first supported intents.
- Add a semantic prompt contract.
- Parse and validate LLM semantic output.
- Return `SemanticIntentPlan` JSON directly from the backend.

Phase 2:

- Add Qt/C++ `SemanticPlanDraftJsonCodec`, `IntentDispatcher`, and `TargetResolver` support for `objects/query/ref`.
- Switch the CAMClaw Agent dialog from `AgentPlanDraft` execution to semantic intent execution.
- Move manual UI actions and AI actions toward shared `CAMApplicationService` methods.
- Extend HumanInLoop from operation target ambiguity to generic intent clarification.

Phase 3:

- Remove old `command_id/schema_id/inputs` agent path from the CAMClaw dialog.
- Replace loose `map<string,string>` business arguments in core execution paths with strong C++ command/value types.

## Consequences

Benefits:

- LLM output is constrained to semantic intent, reducing leakage of invalid execution details into CAM core.
- Manual workflows and AI workflows can converge on the same CAM application services.
- Natural language variation is absorbed in the semantic parsing layer.
- CAM-specific expertise remains deterministic and testable in traditional code.
- Multi-step requests use ordered intent sequences instead of creating combined command names.
- Target selection becomes regular: `objects`, `query`, or `ref`.

Costs:

- Qt/C++ needs a larger refactor to consume semantic plans directly.
- Pydantic models and semantic contracts must be kept aligned with Qt/C++ application services.

## Open Follow-Ups

- Decide whether missing semantic fields should return an immediate error or an `awaiting_input` draft once the Qt HumanInLoop UI supports it.
- Decide when intent execution needs explicit dependency fields instead of ordered execution.
