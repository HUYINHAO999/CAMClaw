# 顺序执行多步骤 Skill 并反馈失败位置

Suggested label: `交给Agent`

User stories covered: 14, 15, 23, 24

## What to build

Build V1 Skill runtime behavior for repeatable CAM workflows such as "创建粗加工工序 -> 设置参数 -> 生成刀轨". Skills execute ordered steps, call globally registered console commands, bind variables from previous results, stop on failure, and return enough feedback for the user to inspect partial results.

This slice should prove Skills are reusable Agent-facing workflows, not CAM process decision makers. The operation type, tool, and parameter choices come from the reviewed AgentPlanDraft.

## Acceptance criteria

- [ ] A Skill can execute multiple steps sequentially.
- [ ] A Skill can bind variables from `ConsoleCommandResult.primary_object_id`.
- [ ] Skill binding does not use positional assumptions such as `object_ids[0]`.
- [ ] Missing fields, type mismatches, or unresolved expressions fail the Skill strictly.
- [ ] When a step fails, later steps are not executed.
- [ ] V1 does not automatically roll back completed steps.
- [ ] The user can see which steps succeeded, which step failed, and the failure reason.
- [ ] Trace records step execution, variable binding, partial success, and failure details.

## Blocked by

- 预览并编辑粗加工 AgentPlanDraft
- 处理目标缺失、类型不匹配和信息不足
