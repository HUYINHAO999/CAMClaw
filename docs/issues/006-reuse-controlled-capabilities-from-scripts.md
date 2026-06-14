# 让脚本和 MCP 复用同一套受控能力

Suggested label: `交给Agent`

User stories covered: 24, 27, 28, 30

## What to build

Build the controlled automation path so scripts and MCP entry points can call the same approved capabilities as natural-language Agent workflows. Scripted automation should submit structured requests through Gateway validation and the Action Adapter / Facade, then reach Component Console just like Agent execution.

This slice should avoid two dangerous outcomes: unrestricted script execution and automation code bypassing Console to call CAM Core directly.

## Acceptance criteria

- [ ] A controlled script or MCP request can invoke a registered CAM capability through the same Gateway and Adapter path used by Agent workflows.
- [ ] Unsupported command IDs are rejected before reaching Component Console.
- [ ] Schema-invalid arguments are rejected before reaching Component Console.
- [ ] Requests that require confirmation cannot execute without the required confirmation state.
- [ ] Script and Agent paths produce consistent command results for the same approved capability.
- [ ] Direct CAM Core access from Agent, MCP, or controlled script paths is not part of the supported execution mechanism.

## Blocked by

- 创建工序并生成刀轨的最小闭环
- 处理目标缺失、类型不匹配和信息不足
