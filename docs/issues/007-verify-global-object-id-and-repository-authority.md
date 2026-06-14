# 验证全局对象 ID 和 Repository 权威性

Suggested label: `交给Agent`

User stories covered: 17, 19, 30

## What to build

Build the behavior that makes global business object IDs authoritative across Agent and Console execution. Tree labels and display names are display-only. A renamed tree item should not change what object the Agent or Console targets. If a request includes a type hint, Repository lookup still decides the real object type.

This slice should protect the architecture from accidentally depending on UI tree item IDs, view IDs, or mutable display names.

## Acceptance criteria

- [ ] `target_object_id` is treated as a globally unique business object ID.
- [ ] UI tree item IDs and view IDs are not part of V1 command request or result models.
- [ ] Display names and tree labels are not used as stable command targets.
- [ ] A renamed tree item still resolves to the same business object.
- [ ] `target_object_type_hint` is optional and does not override Repository object type lookup.
- [ ] Tests verify behavior when the type hint disagrees with Repository lookup.

## Blocked by

- 创建工序并生成刀轨的最小闭环
