# CAMClaw Context

CAMClaw is a C++/Qt CAM application architecture project focused on adding Agent and natural-language operation capability without forcing ordinary UI interaction through the Agent stack.

## Core architecture language

### Qt UI / Interaction Layer

The normal UI layer handles menus, operation trees, parameter panels, viewport interactions, and lightweight interaction state.

Lightweight UI interactions stay in the UI layer:

- Mouse hover
- Panel expand / collapse
- Viewport rotate / pan / zoom
- Input focus
- Temporary highlight

These interactions do not need to become Agent Action Requests.

### Component Console

A Console is the command facade and coordinator for a UI component or control, not a domain-service bucket.

For example, a browser tree component may have a `Browser_BrowserConsole`. That console is the component's "leader": it coordinates trees, items, item operations, commands, menus, and refresh behavior. It does not implement low-level business logic itself.

Examples:

- `Browser_BrowserConsole`
- `ParameterPanelConsole`
- `ViewportConsole`

Console responsibilities:

- Own the component-level command entry point
- Coordinate component context and sub-objects
- Dispatch structured commands to lower layers
- Expose controlled capabilities for UI, tests, skills, and Agent automation

Console non-responsibilities:

- LLM planning
- Agent-specific reasoning
- Heavy validation policy
- Human confirmation policy
- Low-level CAM business implementation
- Directly replacing lower-level command, operation, data, or service layers

Business implementation should sink into lower layers such as tree items, item operation objects, command objects, services, and CAM Core.

### Agent Action Request

A structured request used by Agent, MCP, and controlled script entry points.

Minimum conceptual fields:

- `request_id`
- `trace_id`
- `source`
- `action_id`
- `args`

This is not a requirement that all UI events become JSON. It is the standardized shape for automated or Agent-driven CAM business actions.

### Agent Action Gateway

The safety and dispatch boundary for Agent, MCP, and controlled script actions.

It is responsible for:

- Action registry lookup
- Action schema lookup
- Parameter validation
- Realtime action validation
- Human in loop confirmation
- Trace emission

It does not take over lightweight UI interaction.

### Action Adapter / Facade

Maps controlled Agent actions back into the relevant Component Console.

The adapter prevents Agent code from bypassing the console and directly calling CAM Core.

V1 does not require a ConsoleRouter. Skills may explicitly bind to a target console. If the number of consoles grows, a router or binding registry can be introduced later.

### ConsoleCommandRequest

A structured command request accepted by a Component Console.

V1 conceptual fields:

- `command_id`
- `source`
- `trace_id`
- `target_object_id`
- `target_object_type_hint`
- `args`

Rules:

- `command_id` is globally unique.
- `target_object_id` is a globally unique object ID and is the authority for command targeting.
- `target_object_type_hint` is optional and only helps schema, trace, and debugging. The Repository remains the authority for the object's real type.
- Request and result models do not include tree item IDs or view IDs in V1.
- UI controls that start from a tree item must resolve that item to a business `object_id` before constructing the command request.

### ConsoleCommandResult

The common result shape returned by Component Consoles.

V1 conceptual fields:

- `ok`
- `error_code`
- `message`
- `primary_object_id`
- `object_ids`

Rules:

- `primary_object_id` is the command's most relevant business object.
- On success, it is usually the main created or changed object.
- On failure, it is the main object associated with the error.
- `object_ids` is the set of affected business objects.
- `object_ids` has no order semantics and should not be used for variable binding by index.
- Future commands that need several named outputs may add `named_object_ids`.

### Trace Service

A cross-cutting service that records the full chain from Agent input to client feedback.

Trace should cover:

- Prompt / MCP / script input
- LLM plan
- Parameter draft
- Action Request
- Schema and Pydantic validation
- Action Validator result
- Human confirmation
- Adapter call
- Command Bus / CAM Core execution
- UI feedback, result, and error information

### Canonical CAM Model

The stable model used by Agent, templates, scripts, and actions to refer to CAM objects.

It should use stable IDs and lightweight state rather than UI names or raw geometry payloads.

Important concepts:

- Project
- GeometryObject
- GeometrySelection
- Frame
- Tool
- Operation
- ParameterSet
- Toolpath
- SimulationResult

### Repository

The unified data access abstraction for objects and heavy data.

The Repository may internally wrap an object store, geometry store, toolpath store, simulation store, or enterprise database. External architecture should treat it as one abstraction.

The Repository is responsible for:

- Resolving global `object_id`
- Returning object type and metadata
- Returning object revision
- Returning geometry, faces, edges, bounding boxes, and preview meshes when needed
- Returning toolpath and simulation data when needed

The Canonical CAM Model keeps stable references; the Repository provides real data.

### Skill

A Skill is an Agent-facing reusable workflow.

V1 rules:

- A Skill can compose one or more globally registered console commands.
- A Skill can explicitly bind to target consoles.
- A Skill does not make CAM process decisions such as choosing operation type, choosing tool, or drafting machining parameters.
- Those decisions belong to LLM / Agent Planner and are reviewed in an AgentPlanDraft.
- A Skill validates required inputs, executes command steps, binds variables, stops on failure, and records trace.
- A Skill failure does not automatically roll back completed steps in V1.

### AgentPlanDraft

An AgentPlanDraft is the reviewed output of LLM planning before execution.

V1 rules:

- It may contain multiple ordered Skill steps.
- Steps execute sequentially.
- There are no branches, loops, or conditions in V1.
- The user may edit Skill step input values.
- The user may not delete steps, disable steps, reorder steps, change `skill_id`, or edit internal command steps.
- If the draft is not acceptable, the user rejects the whole draft and gives a reason.
- Rejection reason is recorded in Trace and used as context for the next planning attempt.
- The draft itself is not stored as a Project object or Repository object.
- Trace records `draft_created`, `draft_confirmed_final` or `draft_rejected`, and execution results.

### Target Resolution and Selection

Commands execute against an explicit `target_object_id`.

Selection is not command semantics. Selection is only a candidate target when the request does not already contain `target_object_id`.

V1 target resolution rules:

- If a request has `target_object_id`, use it.
- If a request lacks `target_object_id`, inspect the current active selection as a candidate object.
- Do not silently execute using the active selection.
- Ask the user whether to use the currently selected object.
- The confirmation text should name the selected object's type and display name when available, for example: "是否对当前选中的工序 '型腔粗加工 1' 生成刀轨？"
- If the selected object type does not match the command's required target type, ask the user to select or specify a valid target object.

Example:

```text
Command: browser.generate_toolpath
Required target type: operation

No target_object_id in request.
Current active selection: obj_op_001, type operation, display name "型腔粗加工 1".

System asks:
"是否对当前选中的工序 '型腔粗加工 1' 生成刀轨？"

Only after confirmation does the system fill:
target_object_id = obj_op_001
```

This keeps UI interactions natural while preserving an explicit command target.

## Current design principle

Agent is a controlled automation path into CAM business capabilities.

It should not:

- Directly call CAM Core
- Execute arbitrary Python, C++, system commands, or network calls
- Use UI display names or ambiguous tree labels as stable object references
- Force lightweight UI interactions through the Agent Gateway

It should:

- Use stable IDs
- Validate structured action arguments
- Validate realtime CAM state before execution
- Ask for human confirmation for risky actions
- Emit trace events across the full chain
- Reuse Component Consoles through an Action Adapter / Facade
