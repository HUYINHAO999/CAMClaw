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

### CAM Command Console

The command entry point for operations that change CAM business state.

Examples:

- Create an operation
- Update operation parameters
- Delete an operation
- Generate a toolpath
- Create or update a tool
- Create a geometry selection
- Run simulation
- Apply a template

The Agent path should connect back to this console instead of bypassing it.

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

Maps controlled Agent actions back into the CAM Command Console.

The adapter prevents Agent code from bypassing the console and directly calling CAM Core.

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

### GeometryRepository

Stores or retrieves real geometry and heavy data.

The Canonical CAM Model keeps stable references; the repository provides shapes, faces, edges, bounding boxes, preview meshes, and future real geometry data.

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
- Reuse the CAM Command Console through an Action Adapter / Facade
