# 预览并编辑粗加工 AgentPlanDraft

Suggested label: `交给Agent`

User stories covered: 1, 2, 3, 4, 5, 20, 21

## What to build

Build the review step for a CAM-facing roughing plan. When the Agent proposes a roughing workflow, the user should see an AgentPlanDraft in CAM language before execution and be able to adjust only input values such as operation type, tool, stepover, stepdown, tolerance, feed-related values, and names.

This slice should keep the responsibility boundary clear: the Agent Planner proposes the draft, the user reviews it, and execution is still performed later by Skills and Component Consoles.

## Acceptance criteria

- [ ] The system can present a readable AgentPlanDraft for a roughing workflow before execution.
- [ ] The draft explains the planned CAM action in user-facing CAM language.
- [ ] The user can edit Skill step input values needed for roughing setup.
- [ ] The user cannot delete steps, disable steps, reorder steps, change `skill_id`, or edit internal command steps in V1.
- [ ] The draft contains enough structured data to execute later without asking LLM to reinterpret edited values.
- [ ] Tests verify both allowed value edits and blocked structural edits.

## Blocked by

- 创建工序并生成刀轨的最小闭环
