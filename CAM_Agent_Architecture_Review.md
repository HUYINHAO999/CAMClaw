# CAM Agent 架构说明草案

本文档用于让其他大模型或架构评审者快速理解当前设想，并帮助发现遗漏点。

## 1. 项目目标

我们要做一个 C++/Qt CAM 软件的 Agent / 自然语言操作架构。

用户未来可以通过自然语言完成：

- 工序推荐
- 工序参数草案生成
- CAM 功能树操作
- 自动化 workflow
- 工艺模板调用
- 未来 RAG 工艺知识检索
- 未来仿真结果反馈给 Agent

核心不是给 CAM 软件加一个聊天窗口，而是让 CAM 从一开始具备可被 UI、Agent、MCP、脚本共同调用的命令驱动架构。

## 2. 核心原则

不让 LLM 通过截图、鼠标点击或 UI 自动化操作 CAM。

CAM 软件应该是命令驱动的：

```text
UI 操作
Agent Prompt
MCP 调用
脚本 / Workflow
        |
        v
Console Action 中控层
        |
        v
参数校验 / Human in loop / Trace
        |
        v
Command Bus
        |
        v
CAM Core
```

LLM 只负责：

- 理解用户意图
- 读取当前 CAM 上下文
- 读取控件接口清单
- 选择已有 Console Action
- 生成结构化参数草案
- 生成 workflow 草案
- 解释推荐原因

LLM 不应该直接调用 CAM Core，也不应该绕过 Console 执行危险操作。

## 3. 总体链路

```text
User Prompt
-> LLM / Agent Planner
-> Context Provider
   -> 当前 CAM 状态
   -> 当前选中对象
   -> Console Skill Manifest
   -> Mock Feature / 未来真实特征识别
   -> RAG Knowledge Base
   -> Machining Template Library
-> Action / Parameter / Workflow Proposal
-> Human Confirmation
-> Console Action Registry
-> Command Bus
-> CAM Core
-> Global Trace
```

## 4. Console Action 中控层

Console 是整个架构的核心。

它原本可以用于处理 UI 控件事件，例如 CAM 功能树的单击、双击、右键菜单。未来它也作为 Agent、MCP、脚本的统一入口。

示例：

```text
UI 右键“生成刀路”
-> Console.Execute(GenerateToolpath)

用户 Prompt：“把当前型腔工序计算一下刀路”
-> LLM 选择 GenerateToolpath
-> Console.Execute(GenerateToolpath)
```

要求：

- UI 和 Agent 不能走两套业务逻辑
- 所有功能入口统一到 Console Action
- Console Action 需要声明名称、参数、目标类型、前置条件、是否需要用户确认、执行结果、side effects
- Console 负责把 UI、Agent、MCP、脚本调用统一转换为内部 Action

示例 Action：

```text
OpenOperationEditor
SelectTreeItem
CreateOperationDraft
SetOperationParameters
GenerateToolpath
RunSimulation
DeleteOperation
DuplicateOperation
CreateWorkflowFromTemplate
```

## 5. Console Skill Manifest

为了避免 LLM 猜接口，每个主要 UI 控件或业务区域都提供一个接口清单，供 Agent 读取。

但 Manifest 不能被设计成“给 LLM 的执行规则库”。它只做能力发现和参数形状说明，不负责让 LLM 判断某个动作在当前状态下是否真的可执行。

真实可执行性必须由 Console 根据实时 CAM 状态判断。

示例文件：

```text
cam_operation_tree.skill.json
operation_parameter_panel.skill.json
tool_library_panel.skill.json
geometry_selection_view.skill.json
simulation_panel.skill.json
```

每个 action 描述：

```json
{
  "name": "GenerateToolpath",
  "description": "生成或重新生成指定工序的刀路",
  "component": "cam_operation_tree",
  "target_types": ["operation"],
  "required_args": [
    {"name": "operation_id", "type": "OperationId"}
  ],
  "optional_args": [],
  "confirmation_level": "low",
  "validation_endpoint": "Console.ValidateAction",
  "side_effects": [
    "create_or_update_toolpath",
    "mark_simulation_outdated"
  ]
}
```

Manifest 中不建议放复杂 `preconditions` 列表，原因是：

- 不同工序、不同对象、不同状态下前置条件会组合爆炸
- LLM 无法准确判断实时 CAM 状态
- Manifest 和真实代码容易漂移
- 真正的可执行性需要查询实时 Document / Operation / Toolpath 状态

所以 Manifest 更像“接口目录”，不是“业务规则解释器”。

LLM 的调用逻辑：

```text
读取当前上下文
-> 读取相关控件 manifest
-> 从 manifest 中选择已有 action
-> 生成结构化 args
-> 调用 Console.ValidateAction
-> Console 返回 executable / blocked / need_more_args / need_confirmation
-> 如果缺少参数，Agent 补充信息或询问用户
-> 如果被阻止，Agent 展示阻止原因和可选修复动作
-> 如果需要确认，进入 Human in loop
-> 用户确认后执行 Console.ExecuteAction
```

约束：

- Agent 不能调用 manifest 中不存在的接口
- Agent 不能直接调用 CAM Core
- Agent 如果意图不明确，应返回 need_clarification，而不是硬执行
- Agent 不负责预判所有前置条件
- Console 必须返回结构化失败原因，供 Agent 纠错

Console 校验返回示例：

```json
{
  "status": "blocked",
  "action": "GenerateToolpath",
  "reason_code": "missing_tool",
  "message": "当前工序尚未指定刀具，无法生成刀路。",
  "suggested_actions": [
    {
      "name": "AssignTool",
      "args_hint": {
        "operation_id": "op_pocket_001"
      }
    },
    {
      "name": "OpenOperationEditor",
      "args_hint": {
        "operation_id": "op_pocket_001"
      }
    }
  ]
}
```

## 6. Human in loop

CAM 操作风险高，关键动作必须让用户确认。

原先只使用 `requires_confirmation` 过粗，容易导致用户确认疲劳，也会让真正危险的操作被习惯性跳过。

MVP 阶段建议先分成两级：

```text
low：低风险确认
high：高风险确认
```

未来可以扩展为四级：

```text
none：无风险，例如选中、高亮、打开面板、读取信息
low：低风险，例如创建草稿、生成预览
medium：中风险，例如修改已有参数、重新生成刀路
high：高风险，例如删除、覆盖文件、导出 NC、改后处理器
```

MVP 确认层级建议：

```text
none：
SelectTreeItem
OpenOperationEditor
GetOperationInfo
GetAvailableActions
PreviewOperationCandidate

low：
CreateOperationDraft
CreateWorkflowFromTemplate
GenerateToolpath
RunSimulation

high：
SetOperationParameters，修改已有工序时
DeleteOperation
ClearToolpath
```

未来需要确认但 MVP 暂不实现的动作：

```text
PostprocessGCode
ExportNCFile
OverwriteNCFile
```

低风险确认界面需要展示：

- 将执行什么 action
- 作用对象是什么
- 即将创建或更新什么

高风险确认界面需要展示：

- 参数 diff 或对象 diff
- 删除 / 覆盖 / 修改的不可逆影响
- 影响哪些工序、刀路、仿真结果
- 建议要求用户明确点击二次确认

确认界面通用信息：

- action 名称
- 将修改哪些参数
- 会创建或更新哪些工序 / 刀路
- 用户确认或取消

## 7. 全局 Trace / Observability

Trace 必须全局统一，不只是 Agent 日志。

所有来源都写入同一个 Trace 系统：

```text
UserUI
AgentPrompt
MCP
Script
System
```

每次 action 记录：

```json
{
  "trace_id": "trace_000123",
  "source": "AgentPrompt",
  "user_prompt": "把当前型腔工序计算一下刀路",
  "selected_action": "GenerateToolpath",
  "target": {
    "type": "operation",
    "id": "op_pocket_001"
  },
  "args": {
    "operation_id": "op_pocket_001"
  },
  "manifest_version": "2026.06.14",
  "context_snapshot_id": "ctx_000456",
  "confirmation_level": "low",
  "confirmation_result": "accepted",
  "status": "success",
  "error": null,
  "timestamp": "2026-06-14T10:30:00",
  "duration_ms": 842,
  "side_effects": [
    "create_or_update_toolpath",
    "mark_simulation_outdated"
  ]
}
```

Trace 的目标：

- 调试 Agent 为什么选错接口
- 审计谁触发了什么操作
- 复现 workflow
- 未来把仿真报警结果作为 agent loop 的反馈

## 8. 工序推荐 MVP

当前不做复杂真实特征识别，先 mock 几组 feature 数据。

重点验证 Agent 能否根据选择对象和 feature 类型推荐工序，而不是验证几何算法。

Mock Feature 示例：

```text
planar_face
closed_pocket
hole_group
engraving_region
slot
thread
rotary_region
screw_like_region
```

推荐规则示例：

```text
planar_face -> face_milling_zigzag / contour_cut
closed_pocket -> pocket_roughing / pocket_finishing / rest_machining
hole_group -> drilling
engraving_region -> engraving
slot -> slot_milling
thread -> thread_milling / turning_thread，取决于机床上下文
rotary_region -> 4axis_rotary_roughing
screw_like_region -> rotary_milling / specialized_screw_strategy
```

LLM 返回候选工序：

```json
{
  "task": "recommend_operation",
  "operation_candidates": [
    {
      "type": "pocket_roughing",
      "display_name": "型腔粗加工",
      "reason": "当前特征是封闭型腔，适合先做粗加工去除大部分余量",
      "confidence": 0.9,
      "required_inputs": ["feature_id", "tool_id", "setup_id"],
      "requires_user_selection": true,
      "confirmation_level": "low"
    }
  ]
}
```

## 9. 参数草案 MVP

参数草案必须做，但不追求完整工序参数海。

根据工序复杂度，只生成当前阶段最小可用的参数草案。简单工序给少量参数，复杂工序可以多给一点，但 MVP 阶段控制范围。

示例：

```text
平面铣：
tool
machining_boundary
cut_depth
stepover

型腔粗加工：
geometry
tool
stepover
stepdown
stock_to_leave

钻孔：
hole_group
drill_tool
cycle_type
retract_height

刻字：
geometry/text_curve
engraving_tool
depth

四轴粗加工：
MVP 只推荐工序候选，不自动生成完整参数
```

参数要求：

- 必须结构化
- 带单位
- 可校验
- 带 reason
- 可由用户确认后写入工序
- 不能直接绕过用户修改已有工序

示例：

```json
{
  "task": "draft_operation_parameters",
  "operation_type": "pocket_roughing",
  "parameter_draft": {
    "geometry": {
      "value": "feature_pocket_001",
      "reason": "当前用户选择的是封闭型腔特征"
    },
    "tool": {
      "value": "tool_flat_10",
      "reason": "10mm 平刀适合当前型腔粗加工的初始方案"
    },
    "stepover": {
      "value": 4.0,
      "unit": "mm",
      "reason": "作为粗加工初始步距"
    },
    "stepdown": {
      "value": 2.0,
      "unit": "mm",
      "reason": "作为粗加工初始分层深度"
    },
    "stock_to_leave": {
      "value": 0.3,
      "unit": "mm",
      "reason": "为后续精加工保留余量"
    }
  },
  "confirmation_level": "high"
}
```

## 10. Prompt 宏 / Workflow

用户可以通过 Prompt 生成类似宏的自动化流程。

这里的“脚本”指的是：Prompt 生成一段可追溯的受控 API 调用信息和参数，而不是让 Agent 任意写 Python 操作系统。

脚本的价值是：

- 可读：用户能看到 Agent 准备调用哪些 CAM API
- 可追溯：每一步 API 和参数都能记录到 Trace
- 可复用：相似零件可以复用脚本
- 可调试：失败时能定位到具体 API 调用

MVP 可以先生成一种受控脚本格式。它既可以显示成脚本文本，也可以被解析成 Console Action List。

示例：

```text
用户：“用型腔模板加工这个 pocket。”

Agent 输出：
cam.templates.select("pocket_standard_rough_finish_rest")
cam.workflow.instantiate(template_id, feature_id="feature_pocket_001")
cam.workflow.preview(workflow_id)
cam.confirm(level="low", message="是否应用型腔加工模板？")
cam.workflow.execute(workflow_id)
```

脚本底层等价于 API 调用信息：

```json
{
  "script_type": "cam_control_script",
  "calls": [
    {
      "api": "cam.templates.select",
      "args": {
        "template_id": "pocket_standard_rough_finish_rest"
      }
    },
    {
      "api": "cam.workflow.instantiate",
      "args": {
        "template_id": "pocket_standard_rough_finish_rest",
        "feature_id": "feature_pocket_001"
      }
    },
    {
      "api": "cam.workflow.preview",
      "args": {
        "workflow_id": "$last.workflow_id"
      }
    }
  ]
}
```

也可以显示成受控 Python 风格脚本：

```python
feature = cam.features.get("feature_pocket_001")
template = cam.templates.get("pocket_standard_rough_finish_rest")
workflow = cam.workflow.instantiate(template, feature=feature)
cam.workflow.preview(workflow)
cam.confirm("Apply pocket workflow?")
cam.workflow.execute(workflow)
```

脚本约束：

- 只能调用 `cam.*` API
- 禁止任意文件操作
- 禁止系统命令
- 禁止网络请求
- 执行前要解析成 command list 给用户预览
- 真正执行仍然经过 Console.ValidateAction / Console.ExecuteAction
- 每个 API 调用都要写入全局 Trace

## 11. RAG Knowledge Base

未来需要接入 RAG，但 MVP 可以先 mock。

RAG 主要存知识解释、策略依据和检索线索，不直接存可执行状态。

内容：

- 刀具类型说明
- 工序参数介绍
- stepover / stepdown / tolerance / stock_to_leave 含义
- 不同工件类型加工策略
- 型腔、刻字、钻孔、车螺纹、槽、螺杆等加工经验
- 企业工艺规范
- 常见问题解释

注意：

- 真实刀具库应该是结构化数据，不应该只存在 RAG 里
- RAG 可以解释为什么推荐某刀具或策略
- 实际 `tool_id` 必须来自 Tool Library
- RAG 不能绕过 Console 执行任何 CAM 动作
- RAG 可以召回“应该考虑哪些模板”的依据，但模板选择和实例化应由 Template Library 完成
- RAG 和模板库不是互相依赖关系，而是 Agent Planning 时并列输入

## 12. Machining Template Library

模板库和 RAG 分开。

RAG 是知识解释，模板库是可实例化工艺方案。

更准确的关系是：

```text
User Prompt
-> RAG 召回相关工艺知识、策略说明、模板线索
-> Template Library 根据 feature、上下文、RAG 线索检索候选模板
-> Agent 给出模板候选和原因
-> 用户确认
-> Template Instantiator 生成受控脚本 / Console Action List
```

RAG 不直接调用模板库，模板库也不依赖 RAG 才能工作。二者由 Agent Planner / Context Retriever 组合使用。

示例模板：

```text
型腔加工模板：
型腔铣1粗加工
型腔铣2精加工
清根

钻孔模板：
中心钻
钻孔
倒角

刻字模板：
选择文字 / 曲线
刻字刀
浅切深精加工

槽加工模板：
粗开槽
侧壁精修
底面精修

螺纹 / 螺杆模板：
根据机床能力选择车螺纹、铣螺纹、四轴 / 车铣策略
```

模板包含：

```json
{
  "template_id": "pocket_standard_rough_finish_rest",
  "name": "型腔标准加工模板",
  "applicable_feature_types": ["closed_pocket"],
  "workflow": [
    {
      "operation_type": "pocket_milling_rough_1",
      "purpose": "粗加工",
      "parameter_policy": "roughing_default"
    },
    {
      "operation_type": "pocket_milling_finish_2",
      "purpose": "精加工",
      "parameter_policy": "finishing_default"
    },
    {
      "operation_type": "rest_machining",
      "purpose": "清根",
      "parameter_policy": "rest_cleanup_default"
    }
  ],
  "required_inputs": ["feature_id", "tool_candidates", "setup_id"],
  "confirmation_level": "low"
}
```

Agent 使用方式：

```text
根据用户意图、当前 feature、当前机床 / 工序上下文，以及 RAG 召回的策略线索，从模板库选择候选模板
-> 用户选择 / 确认
-> 模板实例化为受控脚本 / Console Action List
-> Console 校验
-> Human in loop
-> Command Bus 执行
```

模板不能直接执行 CAM Core。

## 13. 暂不做但架构预留

复杂特征识别：

```text
MVP mock feature
未来接几何内核、拓扑分析、真实 feature recognition
```

仿真报警解释：

```text
MVP 不做
预留 SimulationFeedbackProvider
未来仿真报警、碰撞、过切、残料结果可以写入 Trace，作为 agent loop 反馈
```

后处理安全检查：

```text
MVP 不做
预留 PostprocessGuard / PreExportCheck
未来导出 NC 前检查仿真状态、机床、WCS、后处理器、文件覆盖等
```

外部 CAM Agent API 兼容：

```text
不作为 MVP 重点
架构上预留 Adapter
外部协议、MCP、脚本 API 都必须映射到内部 Canonical CAM Model / Console Action
不能直接进入 CAM Core
```

## 14. MVP 范围

要做：

```text
Console Action Registry
UI 功能树接入 Console
控件 Skill Manifest
Prompt 根据 Manifest 选择候选 Action，并由 Console 实时校验
Human in loop
全局 Trace
Mock Feature Provider
工序推荐 MVP
参数草案 MVP
受控脚本 / Workflow Action List
MCP 接入 Console
RAG / 模板库 mock 接口
```

不做：

```text
真实复杂特征识别
真实完整 CAM
真实后处理导出安全系统
仿真报警解释
完整 Python 脚本沙箱
外部 CAM API 深度兼容
```

## 15. 粗略开发时间

如果已有 CAM 基础，只做 Agent / Console 改造：

```text
单人：8-12 周
两人：5-7 周
三人：4-6 周
```

粗分：

```text
大模型 / Agent 部分：2-4 周
CAM Console / UI 改造：5-8 周
Trace / Human in loop / 集成打磨：1-2 周
```

如果从零做简易 CAM Agent Workbench，不做真实刀路，只做 mock CAM：

```text
单人：12-18 周
两人：8-12 周
三人：6-9 周
```

粗分：

```text
简易 CAM 壳和数据模型：7-10 周
大模型 / Manifest / Prompt：3-5 周
Human in loop / Trace / 集成打磨：2-3 周
```

## 16. 希望被锐评的问题

- Console Action 抽象是否足够稳定？
- Skill Manifest 是否会过于复杂？
- Human in loop 分成 low / high 是否足够，是否需要一开始就设计 none / low / medium / high？
- RAG 召回内容辅助模板选择的边界是否清晰？
- 参数草案是否应该更多依赖规则引擎，而不是 LLM？
- Prompt 直接生成受控脚本，还是先生成 Action List 再展示成脚本，哪种更适合调试和落地？
- Trace 是否还应该记录更多上下文？
- 如果未来接真实几何特征识别，当前 mock feature 是否会形成架构债？
- MCP、脚本、UI 都走 Console 是否会导致 Console 过重？
- 如何设计模板库，才能既灵活又不变成另一个 CAM 编程语言？
- RAG 中的工艺知识如何和结构化刀具库、参数库保持一致？
- 工序模板的参数策略应该写在模板里，还是单独做 Parameter Policy？
- Agent Loop 未来如何使用仿真报警结果进行自我修正？
- 哪些动作应该允许 Agent 自动执行，哪些必须永远要求用户确认？
