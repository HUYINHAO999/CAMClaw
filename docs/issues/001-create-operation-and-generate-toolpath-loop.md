# 创建工序并生成刀轨的最小闭环

Suggested label: `交给Agent`

User stories covered: 1, 9, 10, 12, 16, 19, 25, 30

## What to build

Build the first demoable Agent command loop where a CAM user can ask for a roughing operation on the current candidate target, confirm the target, create the operation through the normal Component Console path, and generate its toolpath through the same path.

The slice should prove the boundary: Agent proposes or requests a business action, Gateway validates it, Adapter maps it into the relevant Component Console, and the Console coordinates lower layers without containing LLM planning or low-level CAM implementation. The resulting operation and toolpath should appear as normal CAM objects, not separate Agent artifacts.

## Acceptance criteria

- [ ] A user request such as "给当前型腔做粗加工并生成刀轨" can create a roughing operation and generate a toolpath through a structured command path.
- [ ] If no `target_object_id` is supplied, the system asks whether to use the currently selected object before executing.
- [ ] The confirmation message names the selected object's type and display name when available.
- [ ] The execution path goes through Gateway, Adapter, Component Console, and lower command/service/core layers.
- [ ] Agent, MCP, or script execution does not bypass Component Console to call CAM Core directly.
- [ ] The created operation and toolpath are visible through normal CAM UI/state as regular CAM objects.
- [ ] Trace records the user request, target confirmation, console command dispatch, result, and any error.

## Blocked by

None - can start immediately.
