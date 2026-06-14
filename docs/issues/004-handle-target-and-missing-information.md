# 处理目标缺失、类型不匹配和信息不足

Suggested label: `交给Agent`

User stories covered: 9, 10, 11, 13, 22, 28

## What to build

Build the target and missing-information behavior that prevents Agent workflows from guessing. Commands execute against explicit `target_object_id`; current selection is only a candidate source when the request lacks a target. If the candidate target type is wrong, or required information such as tool or key parameters is missing, the system should stop and ask the user for the missing decision.

This slice should make the system feel safe: it can use current context, but only after clear confirmation.

## Acceptance criteria

- [ ] If `target_object_id` exists, command execution uses it as the explicit target.
- [ ] If `target_object_id` is missing, active selection is treated only as a candidate target.
- [ ] The system never silently executes against active selection.
- [ ] If active selection has the required type, the system asks the user to confirm using it.
- [ ] If active selection has the wrong type, the system asks the user to select or specify a valid target type.
- [ ] If required tool, target, or parameter information is missing, the Agent workflow stops and asks for the missing information instead of guessing.
- [ ] Unsafe, unsupported, or schema-invalid requests are blocked before reaching Component Console.

## Blocked by

- 创建工序并生成刀轨的最小闭环
