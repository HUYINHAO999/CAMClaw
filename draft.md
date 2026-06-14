# CAMClaw Agent Architecture Draft

本文档是本轮对话的阶段性结论，用于开启新会话后继续讨论，不需要从零开始。

## 1. 总目标

要做一个 C++/Qt CAM 软件，并为它设计 Agent / 自然语言操作能力。

用户希望通过自然语言驱动 CAM 软件完成：

- 工序推荐
- 工序参数草案生成
- CAM 功能树操作
- Prompt 宏 / 受控脚本
- 工艺模板调用
- 未来接入 RAG 工艺知识
- 未来把仿真结果作为 Agent loop 的反馈

核心不是“给 CAM 加聊天窗口”，而是从架构上把 CAM 做成命令驱动软件：

```text
UI 操作
Agent Prompt
MCP 调用
受控脚本
        |
        v
Console / Action 调度层
        |
        v
Action Schema + Validator
        |
        v
Human in loop + Trace
        |
        v
Command Bus / CAM Core
```

### 1.1 架构修正：区分轻量 UI 交互和 CAM 业务命令

不论是新开发软件还是已有软件，都不应把所有轻量 UI 交互都塞进 Agent / Gateway / JSON 请求链路。

例如这些交互应留在 UI 层：

```text
鼠标悬停
面板展开 / 折叠
Viewport 旋转 / 平移 / 缩放
输入框 focus
临时高亮
```

只有会改变 CAM 业务状态、需要校验 / 撤销 / Trace / Agent 复用的操作，才应进入 CAM Command Console：

```text
创建工序
修改工序参数
删除工序
生成刀轨
创建刀具
修改刀具
创建几何选择
运行仿真
应用模板
```

主链路更准确地表达为：

```text
Qt UI / Interaction Layer
-> CAM Command Console
-> Action Dispatcher / Command Bus
-> CAM Core
-> Canonical Model / Repository
```

因此更准确的 Agent 接入方式是旁路扩展：

```text
Agent / MCP / 受控脚本
-> Agent Action Request
-> Agent Action Gateway
-> Action Adapter / Facade
-> CAM Command Console
-> Action Dispatcher / Command Bus
-> CAM Core
```

其中：

```text
Agent Action Request:
Agent / MCP / 受控脚本入口使用的标准化操作请求。
它不是要求所有 UI 操作都先转 JSON。

Agent Action Gateway:
只管理自动化入口的调度、安全校验、确认和 Trace。
它不接管轻量 UI 交互。

Action Adapter / Facade:
把受控 action 映射到 CAM Command Console。
它避免 Agent 绕过 Console 直接调用 CAM Core。
```

UI 层可以继续直接处理轻量交互。只有希望暴露给 Agent / MCP / 脚本、且会改变 CAM 业务状态的能力，才需要包装成受控 action。

Trace Service 也应修正为横切能力，而不是只属于 Console / Gateway 层：

```text
Trace Service 贯穿：
Prompt / MCP / Script
-> LLM plan
-> 参数草案
-> Action Request
-> Schema / Pydantic
-> Action Validator
-> Human confirmation
-> Adapter 调用
-> Command Bus / CAM Core
-> UI feedback / result
```

Trace 的 `trace_id` 应尽量从入口创建，并在后续 Action Request、Adapter 调用、CAM 命令、UI 反馈中延续。

## 2. 基本原则

不建议让 AI 通过截图、鼠标点击、UI 自动化去操作 CAM。

CAM 操作应通过受控接口完成：

```text
自然语言
-> LLM / Agent Planner
-> Pydantic 参数校验
-> Console Action Request
-> Action Validator
-> Human in loop
-> CAM Command Bus
```

LLM 负责：

- 理解用户意图
- 选择已有 action
- 生成参数草案
- 生成受控脚本 / workflow 草案
- 解释推荐原因

LLM 不负责：

- 判断实时 CAM 状态是否可执行
- 直接调用 CAM Core
- 任意执行 Python / C++ / 系统命令
- 绕过用户确认

## 3. Manifest / Schema / Validator / Console 的分工

之前讨论中修正过一个重要点：不要让 Manifest 变成“给 LLM 的业务规则 SQL”。

最终分工：

```text
Skill Manifest:
告诉 LLM 有哪些能力、每个能力大概需要什么参数。
它是能力目录，不是业务规则源头。

Action Schema:
定义某个 action 的输入结构、参数类型、确认策略。

Pydantic:
Python Agent 层用于校验 LLM 输出参数结构和值域。

Action Validator:
每个 action 自己负责实时状态判断。
例如 GenerateToolpathAction 判断 operation 是否存在、刀具是否绑定、几何是否有效。

Console:
只做调度、确认、Trace，不写大量业务 if/else。
```

更准确的执行流程：

```text
LLM 根据 Manifest 选择候选 action
-> Python Pydantic 校验 action args
-> Console 找到 action
-> action.validate(context, args)
-> 返回 executable / blocked / need_more_args / need_confirmation
-> Human in loop
-> action.execute()
-> Trace 记录全过程
```

## 4. Human in loop

一开始曾考虑只用 `requires_confirmation`，后来确认这太粗。

V1 先分两级：

```text
low:
低风险确认。例如创建工序草稿、生成刀轨、运行 mock 仿真。

high:
高风险确认。例如删除工序、修改已有工序参数、清除刀轨、执行受控脚本。
```

未来可扩展为：

```text
none / low / medium / high
```

确认策略要避免“所有操作都弹一样的确认框”，否则用户会疲劳。

`GenerateToolpath` 和 `DeleteOperation` 不应使用同一种确认体验。

## 5. Canonical CAM Model 修正版

需要有稳定的 Canonical CAM Model，否则 LLM、模板、脚本、Action 都会引用不稳定对象。

本轮讨论中已经修正如下：

### 5.1 Project

```text
Project
- project_id
- name
- unit_system
- geometry_objects[]
- frames[]
- tools[]
- operations[]
- toolpaths[]
- simulation_results[]
```

不把 `operation_order` 混入 Project Context。

工序顺序属于 Operation Tree，不属于 active context。

### 5.2 OperationContext

原先叫 `MachiningContext`，已改为 `OperationContext`。

但是第一版不一定需要单独暴露 `GetOperationContext`，因为它可以由更基础接口组合：

```text
GetActivePart
GetActiveStock
GetActiveFrame
GetActiveTool
```

随后又进一步收敛：V1 不一定保留这些 active 快捷接口，优先通过 current selection 和明确对象 ID 驱动。

### 5.3 GeometryObject

Part 和 Stock 本质上都是几何体，只是对象类型不同。

```text
GeometryObject
- geom_id
- object_type: part / stock
- name
- shape_revision
- shape_provider
- shape_handle
```

说明：

- `name` 是显示名，可以改，不作为稳定引用。
- `shape_revision` 用于判断 selection / feature / operation 是否因为几何变化而失效。
- 形状参数不塞进 Agent JSON。
- 真实几何数据通过 `GeometryRepository` 获取。

### 5.4 GeometryRepository

当前可 mock，未来对接数据库或几何内核。

```text
GeometryRepository.getGeometryObject(geom_id)
GeometryRepository.getShape(geom_id, shape_revision)
GeometryRepository.getFace(geom_id, shape_revision, face_ref)
GeometryRepository.getEdge(geom_id, shape_revision, edge_ref)
GeometryRepository.getBoundingBox(geom_id, shape_revision)
GeometryRepository.getPreviewMesh(geom_id, shape_revision)
```

关键原则：

```text
Canonical Model 存稳定引用。
Repository 存真实几何。
上层通过 geom_id + shape_revision 访问。
```

### 5.5 Frame

WCS 是唯一世界坐标系，不建立 `wcs_id`。

加工坐标 / MCS / 局部坐标统一叫 `Frame`。

```text
Frame
- frame_id
- name
- matrix_3x4
- frame_type
```

`matrix_3x4` 表达：

```text
[ r00 r01 r02 tx
  r10 r11 r12 ty
  r20 r21 r22 tz ]
```

不单独存 origin / axes。

### 5.6 Machine

MVP 只需要当前机床能力配置，不需要 `machine_id`。

```text
MachineConfig
- machine_type
- axis_count
```

第一版目标是验证刀轨是否正确，不做后处理导出。

### 5.7 Tool

刀具类型先收敛：

```text
Tool
- tool_id
- name
- category: mill / drill / lathe
- schema_id
- schema_version
- parameters
```

Mill 里再区分：

```text
tip_type: flat / ball / bull
```

暂不做 Holder。

暂不做 Material。

### 5.8 Operation

```text
Operation
- operation_id
- operation_type
- name
- geometry_selection_id
- tool_id
- frame_id
- parameter_set_id
- state
- toolpath_id
- simulation_result_id
- created_by: user / agent / script / template
```

Operation 状态建议拆分：

```text
input_state: draft / valid / dirty
toolpath_state: missing / generated / outdated / failed
simulation_state: not_run / passed / warning / failed / outdated
```

不要用一个巨大 enum 表达所有组合状态。

### 5.9 ParameterSet

参数系统要支持扩展：

```text
ParameterSet
- parameter_set_id
- schema_id
- schema_version
- values
```

新增刀具参数或工序参数时，应该优先改 schema，而不是改 Canonical Model 主结构。

长期目标：

```text
schema/*.json 作为 source of truth
-> Python Pydantic
-> C++ ParameterValidator
-> UI 参数面板
-> Agent 参数草案
```

## 6. RAG 和模板库

RAG 和模板库要分开。

RAG 存：

- 刀具类型说明
- 工序参数解释
- 加工策略知识
- 型腔、刻字、钻孔、槽、螺纹、螺杆等经验
- 企业工艺规范

模板库存：

- 可实例化的工艺方案
- 例如型腔粗加工 + 精加工 + 清根
- 钻孔模板、刻字模板、槽加工模板、螺纹模板等

关系：

```text
User Prompt
-> RAG 召回相关知识和模板线索
-> Template Library 根据 feature/context/rag_hint 搜索模板
-> Agent 给出候选模板
-> 用户确认
-> 模板实例化为受控脚本 / 基础接口组合
```

RAG 不直接调用模板库。

模板库也不依赖 RAG 才能工作。

二者是 Agent Planning 的并列输入。

## 7. Prompt 脚本 / 受控脚本

用户说的脚本不是“Action List 转脚本”，而是：

```text
Prompt 直接生成受控 API 调用脚本。
脚本本质是 API 调用信息和参数。
```

示例：

```python
feature = cam.features.get("feature_pocket_001")
tool = cam.tools.get("tool_flat_10")
op = cam.operations.create(type="pocket_roughing")
cam.operations.update(op.id, geometry=feature.selection_id, tool=tool.id)
cam.operations.update_parameters(op.id, stepover=4.0, stepdown=2.0)
cam.toolpath.generate(op.id)
```

约束：

- 只能调用 `cam.*` API
- 不允许任意文件操作
- 不允许系统命令
- 不允许网络请求
- 每一步仍然经过 Pydantic、Action Validator、Human in loop、Trace

## 8. V1 Command Registry 思路

本轮最后确认：不要按模块想象列一大堆接口。

应该从测试用例反推最小基础接口。

复杂能力用上层 Skill / 脚本组合基础接口完成。

例如：

```text
GenerateAllToolpaths
RunSetupSimulation
CreateWorkflowFromTemplate
```

这些暂不进入 V1 基础接口，而是由 Skill 组合：

```text
GetOperationTree
-> 循环 ValidateOperation
-> GenerateToolpath
-> GetToolpath
```

### V1 基础接口候选

当前收敛到大约 24 个，仍需下一轮继续检查。

```text
GetProjectInfo
GetCurrentSelection

GetGeometryObject
CreateGeometrySelection
GetGeometrySelection
GetFeatures

GetToolLibrary
GetTool
CreateTool

GetOperationTree
GetOperation
CreateOperation
UpdateOperation
DeleteOperation
ValidateOperation

RecommendOperations
DraftOperationParameters

GenerateToolpath
GetToolpath

GetTrace
SearchTrace

GetToolSchema
GetOperationSchema
ValidateParameters
```

注意：

- `GetTool(tool_id)` 是按 ID 查某把刀。
- `GetActiveTool()` 是查当前默认刀具。后续倾向于 V1 不保留 active tool 快捷接口。
- Trace 必须保留，用于定位问题。
- Trace 创建可以内部自动做，不一定暴露 `CreateTraceSnapshot`。

### 上层 Skill 组合示例

这些不是基础接口，是 Agent 侧组合流程：

```text
RecommendOperationForCurrentSelection
= GetCurrentSelection
+ GetFeatures
+ GetOperationSchema
+ RecommendOperations

DraftParametersForSelectedOperation
= GetCurrentSelection
+ GetOperation
+ GetOperationSchema
+ ValidateParameters
+ DraftOperationParameters

RegenerateSelectedOperationToolpath
= GetCurrentSelection
+ GetOperation
+ ValidateOperation
+ GenerateToolpath
+ GetToolpath

PatchSelectedOperationParameters
= GetCurrentSelection
+ GetOperation
+ GetOperationSchema
+ ValidateParameters
+ UpdateOperation
+ ValidateOperation
```

## 9. 测试用例覆盖思路

我们已经讨论过的测试场景包括：

- 新手选择二维平面加工工序
- 型腔粗加工推荐
- 参数草案生成但上下文不足
- 少量关键参数草案
- Prompt 宏创建粗加工流程
- 禁止直接覆盖 NC 文件，后处理只预留
- 仿真报警解释，暂不做但预留
- 生成可追溯受控脚本
- 四轴回转类零件推荐
- Prompt 触发当前选中工序生成刀轨
- 意图不明确时不执行删除
- 根据 Manifest 打开工序参数界面
- 参数面板通过 Prompt 修改 stepover/tolerance
- Agent 尝试调用不存在接口时返回 unsupported

下一轮应继续用这些测试用例反推接口，删掉用不上的基础接口。

## 10. FreeCAD MCP 可借鉴点

参考项目：

```text
F:\projects\freecad-mcp-dev\freecad-mcp-dev
```

比 BlenderMCP 更工程化，值得吸收。

### 10.1 分组式工具

FreeCAD MCP 没有把每个细碎动作都暴露成一个 MCP tool，而是分组：

```text
cam_operations
cam_tools
part_operations
sketch_operations
measurement_operations
view_control
```

CAMClaw 也可以将 20 多个基础接口在 MCP 层分组：

```text
cam_context
cam_geometry
cam_tools
cam_operations
cam_toolpath
cam_schema
cam_trace
```

内部再通过 `action` 分发。

### 10.2 Handler 分层

FreeCAD MCP 有：

```text
BaseHandler
-> CAMOpsHandler
-> CAMToolsHandler
-> PartHandler
...
```

CAMClaw 可借鉴：

```text
BaseActionHandler
-> GeometryActions
-> ToolActions
-> OperationActions
-> ToolpathActions
-> SchemaActions
-> TraceActions
```

每个 Action 自己实现 schema、validator、execute。

### 10.3 明确拒绝歧义对象

FreeCAD `BaseHandler.get_object()` 如果 Label 重名，会拒绝猜测。

CAMClaw 也必须坚持：

```text
Agent 不能用 UI 显示名、树路径、模糊文字直接执行。
必须解析到稳定 object_id。
```

### 10.4 Socket bridge

FreeCAD MCP 使用：

```text
MCP Server
-> socket bridge
-> FreeCAD handler
```

并使用 length-prefixed framing，避免 JSON 消息粘包/截断。

CAMClaw 可借鉴：

```text
Python Agent/MCP Service
-> QLocalSocket/TCP
-> Qt CAM Console Bridge
-> Action Registry
```

### 10.5 长任务

FreeCAD MCP 有：

```text
execute_python_async
poll_job
list_jobs
cancel_job
```

CAMClaw 未来生成刀轨、仿真、特征识别都可能需要：

```text
job_id
poll
cancel
```

V1 可先同步或 mock，但架构应预留。

### 10.6 诊断和 Trace

FreeCAD MCP 有：

```text
get_debug_logs
get_last_traceback
operation logging
crash watcher
health check
```

CAMClaw 必须第一版就做 Trace。

### 10.7 Dispatch completeness 测试

FreeCAD MCP 有静态测试：

```text
注册的 MCP tool
必须在 bridge 和 handler 中可路由
```

CAMClaw 应做类似测试：

```text
Manifest 中声明的 action
必须存在 Action Registry
必须存在 Action Schema
必须存在 Validator / Handler
```

这可以防止 Manifest、Schema、Registry、代码漂移。

### 10.8 不应照搬的点

不要照搬：

```text
execute_python / execute_python_async 任意 Python
过宽的工具面
FreeCAD object_name / Label 引用模式
post_process / export_gcode
```

CAMClaw 第一版目标是看刀轨是否正确，不做 NC 导出。

## 11. 参数扩展 Skill

用户提出一个重要要求：

未来如果业务扩展，例如：

- 刀具增加一个参数
- 工序增加一个参数
- 参数默认值变化
- 校验规则变化

必须有一个项目级 Skill，让任何接手的人或 Agent 都能自动更新这套业务参数。

Skill 放置位置：

```text
F:\CAMClaw\skills\cam-parameter-extension
```

暂未实施。

原因：还有一些 schema、Action Validator、模板关系问题没完全确认。

初步方向：

```text
参数 schema 使用 JSON/YAML 作为权威源头。
Pydantic 用于 Python Agent 层校验。
C++/Qt Core 仍需最终 Validator。
```

该 Skill 未来应指导 Agent：

- 读取目标 schema
- 修改参数定义
- 更新示例
- 更新测试
- 判断是否需要 schema_version 升级
- 检查是否影响参数草案、UI 参数面板、Action Validator、模板实例化

## 12. 已决定暂不做的内容

第一版不做：

- 完整真实 CAM
- 复杂真实特征识别
- 后处理导出 G-code
- 后处理安全检查
- Holder
- Material
- PostProcessor
- 多 Setup 管理
- 任意 Python 执行
- 完整脚本运行时
- 真实仿真报警解释
- 外部 CAM API 深度兼容

只在架构上预留：

- SimulationFeedbackProvider
- PostprocessGuard / PreExportCheck
- External Agent API Adapter
- Job / async task
- RAG
- Template Library

## 13. 下一会话需要继续确认的问题

建议下一轮优先讨论：

1. V1 Command Registry 是否还能继续压缩
2. 哪些接口应该作为 MCP 分组工具暴露，哪些只是内部 Action
3. Trace 的最小数据结构
4. Pydantic 模型和 JSON/YAML schema 的关系
5. Action Schema 的最小字段
6. Operation Schema 如何表达参数分组、单位、默认值、显示名、枚举、可见性
7. 参数扩展 Skill 的具体目录结构和内容
8. GeometryRepository mock 的最小接口和 mock 数据形态
9. 上层 Skill 组合如何描述，是否也需要 Manifest
10. Human in loop 的 low/high UI 差异
11. 长任务 job_id 是否要进入 V1
12. 是否需要 `GetAvailableActions`
13. 是否需要 active part/stock/frame/tool 快捷接口

## 14. 当前最重要的架构句子

```text
Manifest 解决“有哪些能力”。
Schema 解决“参数长什么样”。
Pydantic 解决“LLM 输出是否合规”。
Canonical CAM Model 解决“参数引用什么对象”。
Repository 解决“真实几何/刀轨数据从哪里取”。
Validator 解决“当前状态能不能执行”。
Agent Action Gateway 解决“自动化入口的统一调度、安全校验和确认”。
Action Adapter / Facade 解决“受控 action 如何接回 CAM Command Console”。
Trace Service 解决“从 LLM 到客户端反馈的全链路追踪”。
Skill 解决“多个基础接口如何组合成可复用流程”。
```
