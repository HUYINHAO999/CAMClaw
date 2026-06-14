# PRD: Agent-to-Component-Console Command Workflow

## Problem Statement

CAMClaw needs Agent and natural-language operation capability without turning ordinary UI interaction into Agent-specific machinery. The user wants Agent, MCP, and controlled script entry points to operate CAM software through controlled, traceable, confirmable business commands while preserving the existing principle that a good system can run even without UI.

The current risk is architectural drift: Agent integration could bypass Component Consoles, call CAM Core directly, depend on tree item IDs, place validators inside Console classes, or push process decision logic into Skills. Any of those would make the system harder to extend and would duplicate behavior across UI controls, Agent automation, and future scripted workflows.

## Solution

Introduce a clear V1 architecture where Agent-facing capabilities are expressed as reviewed AgentPlanDrafts, executed through Skills, validated before reaching Component Consoles, and finally dispatched as structured ConsoleCommandRequests to the relevant Component Console.

The user-facing behavior should be:

- The Agent proposes a readable plan before execution.
- The user can adjust input values such as tool, operation type, names, and machining parameters.
- The user cannot reorder, delete, or edit internal Skill steps in V1.
- If the draft is wrong, the user rejects the whole draft and gives a reason; the next plan uses that feedback.
- Commands use global business object IDs, not UI tree item IDs.
- If no target object is explicit, the system asks whether to use the currently selected object instead of silently acting on it.
- Component Consoles remain component-level coordinators, not bulky containers for LLM logic, validators, or low-level CAM implementation.

## User Stories

1. As a CAM user, I want natural language requests to produce a reviewable plan before execution, so that I can inspect what the Agent intends to do.
2. As a CAM user, I want to edit draft machining inputs before confirming, so that I can correct tool choices, operation names, or parameter values.
3. As a CAM user, I want to reject an entire AgentPlanDraft with a reason, so that the Agent can generate a better plan from my feedback.
4. As a CAM user, I want the system to ask before using the current selection as a command target, so that ambiguous prompts do not silently modify the wrong object.
5. As a CAM user, I want commands to name the selected object's type and display name before using it, so that I can verify the target.
6. As a CAM user, I want a clear error when the current selection is the wrong object type, so that I know what object to select next.
7. As a CAM user, I want failed workflows to stop without automatic rollback, so that the software does not silently delete or undo objects I may want to inspect.
8. As a CAM user, I want every Agent action to be traceable, so that I can understand what was proposed, confirmed, executed, and returned.
9. As a CAM user, I want ordinary UI interactions like hover, panel expansion, viewport movement, and focus to stay lightweight, so that the UI remains responsive and simple.
10. As a CAM user, I want business actions like creating operations or generating toolpaths to use the same command route whether triggered by UI or Agent, so that behavior is consistent.
11. As a CAM user, I want Agent automation to reuse Component Consoles, so that the Agent does not bypass established software behavior.
12. As a CAM user, I want the Agent to avoid using UI tree labels as stable references, so that renamed nodes or duplicated names do not cause incorrect actions.
13. As an Agent user, I want Skills to compose existing commands, so that repeated workflows can be automated without duplicating CAM business logic.
14. As an Agent user, I want Skills to stop when a step fails, so that later steps do not run on invalid assumptions.
15. As an Agent user, I want Skills to pass newly created object IDs between steps, so that workflows can create an operation and then update or generate toolpaths for that same object.
16. As an Agent user, I want variable binding to fail strictly when result data is missing, so that the Agent does not guess IDs from free text.
17. As an Agent user, I want LLM decisions to happen before Skill execution, so that Skills remain simple workflow composition rather than process-recommendation engines.
18. As an Agent user, I want the LLM to decide operation type, tool, and draft parameters, so that process intelligence is not hardcoded inside Skills.
19. As an Agent user, I want confirmation to happen once for a whole workflow based on the highest-risk step, so that I am not spammed by repetitive prompts.
20. As an Agent user, I want a rejected plan reason to influence the next generated plan, so that the Agent learns from my correction in the current workflow.
21. As a developer, I want Component Consoles to expose structured command entry points, so that UI, tests, Skills, and Agent automation can all call the system without UI event simulation.
22. As a developer, I want Component Consoles to stay thin coordinators, so that low-level business logic stays in item operations, command objects, services, and CAM Core.
23. As a developer, I want Validator logic outside Component Consoles, so that Consoles do not become bloated policy containers.
24. As a developer, I want global command IDs, so that Skills, Trace, confirmation policy, schemas, and command registry can reference commands unambiguously.
25. As a developer, I want command registry files to be checked against runtime supported commands, so that declared Agent capabilities cannot drift from implemented Console capabilities.
26. As a developer, I want global object IDs, so that future object-database integration can become the authority for object identity.
27. As a developer, I want object type hints to be optional hints rather than authority, so that Repository lookup remains the source of truth.
28. As a developer, I want command results to expose `primary_object_id`, so that Skill variable binding can use a stable, non-positional result field.
29. As a developer, I want `object_ids` to be unordered affected-object lists, so that they can support trace, refresh, and audit without becoming fragile variable bindings.
30. As a developer, I want future commands to be able to add `named_object_ids`, so that multi-output workflows can become explicit without breaking V1.
31. As a developer, I want AgentPlanDrafts to be Trace artifacts rather than Project objects, so that Agent process state does not pollute CAM project data.
32. As a developer, I want selection to be only a candidate target, so that command execution always resolves to an explicit `target_object_id`.
33. As a developer, I want UI controls that start from tree items to resolve those items to business object IDs, so that commands remain UI-control agnostic.
34. As a developer, I want Skills to explicitly bind to target consoles in V1, so that implementation is debuggable without introducing an early ConsoleRouter.
35. As a developer, I want the architecture to allow a future ConsoleRouter or binding registry, so that the system can scale when the number of consoles grows.
36. As a QA engineer, I want to test AgentPlanDraft review rules at the highest behavior seam, so that tests verify user-visible constraints rather than internal widget details.
37. As a QA engineer, I want to test Skill runtime step execution and partial failure, so that workflow behavior is reliable.
38. As a QA engineer, I want to test Gateway validation before Console dispatch, so that invalid Agent requests cannot reach Component Consoles.
39. As a QA engineer, I want to test Component Console structured commands without UI, so that the system proves it can operate independent of visible UI.
40. As a QA engineer, I want to test target resolution and selection confirmation, so that ambiguous targets never execute silently.

## Implementation Decisions

- Component Console is the command facade and coordinator for a UI component or control. It is not a domain-service bucket and not an Agent-specific layer.
- `Browser_BrowserConsole` is the canonical example: it coordinates browser trees, items, item operations, commands, menus, and refresh behavior, while lower layers perform actual business work.
- Component Consoles expose structured command entry points for UI, tests, Skills, and Agent automation.
- Component Consoles must not contain LLM planning, heavy validation policy, human confirmation policy, or low-level CAM implementation.
- Agent automation reaches Component Consoles through Action Adapter / Facade. It must not bypass Console and call CAM Core directly.
- V1 does not introduce a ConsoleRouter. Skills may explicitly bind to a target console; a router or binding registry can be added later if console count grows.
- `ConsoleCommandRequest` uses globally unique `command_id` and globally unique `target_object_id`.
- `target_object_type_hint` is optional and only supports schema, trace, and debugging. Repository remains authoritative for real object type.
- V1 request/result structures do not include tree item IDs or view IDs.
- `ConsoleCommandResult` includes `ok`, `error_code`, `message`, `primary_object_id`, and unordered `object_ids`.
- `primary_object_id` is the most relevant business object. It can exist on both success and failure.
- `object_ids` is an unordered set of affected business objects and must not be used for positional variable binding.
- Future multi-output commands may add `named_object_ids`.
- Repository is one unified abstraction for resolving global object IDs, object metadata, object revisions, geometry, toolpaths, and simulation data.
- `command_id` is globally unique and should use readable namespaces such as `browser.generate_toolpath`.
- Command registry files such as `commands/*.yaml` are the declaration authority for command metadata, target console, confirmation level, and schema path.
- Runtime `supportedCommands()` on Component Consoles acts as capability self-check.
- Completeness checks must ensure command registry declarations match runtime console support and schema availability.
- Skills are Agent-facing workflows that compose one or more globally registered console commands.
- Skills may explicitly bind to target consoles in V1.
- Skills do not choose operation type, choose tools, draft machining parameters, or make CAM process decisions. Those decisions belong to LLM / Agent Planner.
- Skills validate required inputs, execute command steps, bind variables, stop on failure, and record Trace.
- V1 Skill failure stops execution and returns partial failure. It does not automatically roll back completed steps.
- Skill runtime supports variable binding between steps.
- Variable binding must be strict: missing fields, type mismatches, or unresolved expressions fail the Skill instead of asking LLM to guess.
- Skill variable binding should use `primary_object_id`, not `object_ids[0]`.
- AgentPlanDraft is the reviewed output of LLM planning before execution.
- AgentPlanDraft may contain multiple ordered Skill steps in V1.
- AgentPlanDraft steps execute sequentially. V1 does not support branches, loops, or conditions.
- Users may edit Skill step input values during review.
- Users may not delete steps, disable steps, reorder steps, change `skill_id`, or edit internal command steps.
- If the draft is unacceptable, users reject the entire draft and provide a reason.
- Draft rejection reason is recorded in Trace and used as context for the next planning attempt.
- AgentPlanDraft is not stored as a Project object or Repository object. It is a Trace artifact.
- Trace records `draft_created`, `draft_confirmed_final` or `draft_rejected`, execution results, and failure details.
- Target resolution requires explicit `target_object_id` for execution.
- If a request lacks `target_object_id`, the active selection is only a candidate target.
- The system must not silently execute using the active selection.
- The system must ask whether to use the currently selected object, naming its type and display name when possible.
- If the candidate selection type does not match the command's required target type, the system asks the user to select or specify a valid target.
- Lightweight UI interactions such as hover, panel expansion, viewport movement, focus, and temporary highlight stay in UI and do not become Agent Action Requests.

## Testing Decisions

- Tests should verify external behavior at the highest seam possible and avoid coupling to implementation internals such as specific widget event handlers.
- AgentPlanDraft review tests should verify that users can edit step input values but cannot delete steps, disable steps, reorder steps, change `skill_id`, or edit internal command steps.
- Draft rejection tests should verify that a rejection reason is recorded and can be supplied to the next planning attempt.
- Skill runtime tests should verify sequential execution, strict variable binding, `primary_object_id` usage, partial failure stopping behavior, and no automatic rollback.
- Gateway validation tests should verify that unsupported commands, invalid schemas, missing confirmation, and missing target handling are stopped before reaching Component Console.
- Component Console command tests should verify that structured command requests can execute without UI events and that Console dispatches to lower layers instead of containing Agent logic.
- Command registry completeness tests should verify that every registered command has a schema, target console, confirmation level, and matching runtime supported command.
- Target resolution tests should verify that missing `target_object_id` prompts the user before using active selection, and that mismatched selection type does not execute.
- Repository-facing tests should verify that object type hints are not treated as authoritative when Repository lookup disagrees.
- Trace tests should verify that draft creation, confirmation/rejection, gateway validation, adapter dispatch, console result, and failure details are recorded.
- Prior art is currently architectural documentation rather than executable code. Initial tests should be introduced alongside the first runnable prototype seam for AgentPlanDraft, Skill runtime, Gateway validation, and Component Console command dispatch.

## Out of Scope

- Implementing a full real CAM kernel.
- Implementing real toolpath algorithms beyond mock or prototype behavior.
- Building a full UI for all AgentPlanDraft editing scenarios.
- Supporting deletion, disabling, reordering, branching, looping, or conditional execution of draft steps in V1.
- Recording field-level diffs for user edits during draft review.
- Automatically rolling back completed Skill steps on failure.
- Introducing ConsoleRouter in V1.
- Exposing tree item IDs or view IDs in ConsoleCommandRequest or ConsoleCommandResult.
- Letting Skills make CAM process decisions such as selecting operation type, tool, or machining parameters.
- Allowing Agent, MCP, or controlled script paths to call CAM Core directly.
- Supporting arbitrary Python, C++, system commands, network calls, or UI automation as Agent execution mechanisms.
- Persisting AgentPlanDraft as a Project or Repository object.
- Implementing G-code export or post-processing.

## Further Notes

- The key architectural principle is: UI is one caller of the system, not the system itself. A good CAM system should be able to execute business commands without visible UI.
- Component Console is the control-level "leader"; it coordinates but does not do low-level work.
- Selection is not command semantics. It is only a candidate target when no explicit `target_object_id` exists, and it must be confirmed before use.
- The Agent-facing architecture should remain conservative: LLM plans, the user reviews, Skills compose commands, Gateway validates, Adapter reaches Console, and Console dispatches to lower layers.
- The PRD should be published to GitHub Issues with the `交给Agent` label.
