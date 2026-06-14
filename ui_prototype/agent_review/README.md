# Agent Review UI Prototype

This is a lightweight review UI prototype for the AgentPlanDraft workflow.

It models the UI responsibilities that will later move into Qt:

- Show target selection context
- Show AgentPlanDraft steps
- Allow editing Skill input values
- Lock edits after confirmation or rejection
- Show execution result JSON
- Show trace events

It intentionally does not call the LLM directly. The Python backend owns LLM calls and produces Draft JSON. The C++ core owns execution.
