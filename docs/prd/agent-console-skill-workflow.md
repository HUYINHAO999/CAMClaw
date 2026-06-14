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

1. As a CAM user, I want to say "给当前型腔做粗加工", so that the software can propose an operation plan without me manually finding every command.
2. As a CAM user, I want to review the proposed operation type before execution, so that the Agent does not create the wrong machining operation.
3. As a CAM user, I want to review the proposed tool before execution, so that the Agent does not use an unsuitable cutter.
4. As a CAM user, I want to review the proposed stepover, stepdown, tolerance, and feed-related parameters before execution, so that I can correct unsafe or inefficient values.
5. As a CAM user, I want to edit draft parameter values directly in the review step, so that I can keep a good plan but adjust the details.
6. As a CAM user, I want to reject the whole proposed plan with a reason, so that the Agent can regenerate a better plan instead of forcing me to manually repair it.
7. As a CAM user, I want the next generated plan to consider my rejection reason, so that repeated mistakes are less likely.
8. As a CAM user, I want to say "只要粗加工，不要精加工", so that the Agent regenerates a simpler plan rather than making me delete steps manually.
9. As a CAM user, I want the software to ask before using the currently selected object, so that vague requests do not silently operate on the wrong object.
10. As a CAM user, I want the confirmation prompt to name the currently selected object, so that I know exactly what will be changed.
11. As a CAM user, I want a clear prompt when the current selection is the wrong kind of object, so that I know whether to select a feature, operation, setup, or tool.
12. As a CAM user, I want to generate a toolpath for a selected operation from natural language, so that I do not need to hunt through menus for a common action.
13. As a CAM user, I want the Agent to stop when required information is missing, so that it asks for a tool, target, or parameter instead of guessing.
14. As a CAM user, I want a failed Agent workflow to stop without automatic cleanup, so that I can inspect any objects that were already created.
15. As a CAM user, I want to see what part of a multi-step plan succeeded and where it failed, so that I can decide whether to continue, fix, or undo manually.
16. As a CAM user, I want Agent-created operations and toolpaths to appear through the normal software UI, so that they feel like regular CAM objects rather than separate Agent artifacts.
17. As a CAM user, I want tree labels and display names to be treated as display-only, so that renaming an item does not cause the Agent to target the wrong object.
18. As a CAM user, I want ordinary UI actions like expanding a panel or rotating the viewport to stay fast and local, so that adding Agent support does not make the interface feel heavy.
19. As a CAM user, I want Agent-triggered commands and manually triggered commands to produce consistent CAM results, so that I can trust both paths.
20. As a CAM user, I want the Agent to explain what it is about to do in CAM language, so that I can judge the plan before approving it.
21. As a novice CAM user, I want the Agent to propose a reasonable machining workflow for a selected feature, so that I can learn a sensible starting point.
22. As a novice CAM user, I want the Agent to tell me when the current context is insufficient, so that I know what to select or define next.
23. As a process engineer, I want repeatable prompt workflows for common machining patterns, so that roughing, finishing, and toolpath generation can be made consistent.
24. As a process engineer, I want reusable workflows to follow the same software command path as manual operations, so that automation does not drift from normal CAM behavior.
25. As a reviewer, I want a trace of the prompt, draft, confirmation, execution result, and failure reason, so that I can audit how an Agent-produced CAM object was created.
26. As a reviewer, I want rejected plans and rejection reasons to be traceable, so that Agent behavior can be improved from real feedback.
27. As an automation user, I want controlled scripts to call the same approved capabilities as natural-language prompts, so that scripts are powerful but not unrestricted.
28. As an automation user, I want unsupported or unsafe requests to be blocked before execution, so that scripts and Agent plans cannot bypass the software's safety boundary.
29. As a CAM user, I want future workflows to support templates and RAG-based recommendations without changing how I confirm the final plan, so that added intelligence still feels controlled.
30. As a CAM user, I want the system to preserve a clear boundary between "the Agent proposes" and "the CAM software executes", so that responsibility remains understandable.

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
