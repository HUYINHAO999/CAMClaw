# CAMClaw

CAMClaw 是一个 Qt CAM 原型项目，用来验证“传统 CAM 软件 + 大模型辅助操作”的架构。

项目目标不是从零做一个真实 CAM 内核，而是先搭一个简化版 CAM 软件，再让大模型通过自然语言帮助用户完成工序创建、刀轨显示隐藏、参数修改、重新计算等操作。

## 项目能做什么

目前已经支持：

-  通过 CAMClaw 对话框输入 prompt，让大模型生成语义草案

- Qt 客户端显示 CAM Browser 树和绘图区

- 创建型腔铣、钻孔、平面铣工序

- 选择型腔、孔组、平面等 mock 几何特征

- 生成不同类型的 mock 刀轨

- 双击工序打开编辑界面

- 右键工序生成、重新计算、删除刀轨

- 点击刀轨显示 / 隐藏刀轨

- 用户确认草案后，由 Qt/C++ 执行真实 CAM 侧逻辑

  

## 当前架构以及架构演进

架构图在：

```text
docs\current-code-architecture.html
```





## 快速启动

### 1. 准备环境

需要：

- Windows
- Visual Studio 2022 C++ 桌面开发环境
- CMake
- Qt 6 Widgets MSVC 版本
- Python 3.10+

安装 Python 依赖：

```powershell
python -m pip install -r python_backend\requirements.txt
```

### 2. 配置大模型地址和 key

复制配置文件：

```powershell
copy .env.example .env.local
```

然后编辑项目根目录下的 `.env.local`：

```text
CAMCLAW_LLM_BASE_URL=https://jspin.cn
CAMCLAW_LLM_API_KEY=replace-with-your-api-key
CAMCLAW_LLM_MODEL=gpt-5.5
```

不要提交 `.env.local`，里面有本地密钥。

### 3. 一键启动

如果你的 Qt 路径和脚本默认路径一致，直接双击：

```text
scripts\run_qt_cam.bat
```

这个 bat 会自动调用：

```powershell
.\scripts\run_qt_cam.ps1 -StartBackend -RestartBackend
```

如果 Qt 安装路径不同：

```powershell
.\scripts\run_qt_cam.ps1 `
  -QtCMakeDir "你的Qt路径\lib\cmake" `
  -QtBinDir "你的Qt路径\bin" `
  -StartBackend `
  -RestartBackend
```

脚本会完成：

- CMake 配置
- Qt 客户端编译
- 添加 Qt DLL 路径
- 启动 Python Agent 后端
- 启动 Qt 客户端





## 大模型交互流程

CAMClaw 不是让大模型直接调用 C++ 接口。

当前流程是：

```text
用户 prompt
-> Python 后端请求大模型
-> 大模型返回 SemanticIntentPlan JSON
-> Python 用 Pydantic 校验
-> Qt 展示草案
-> 用户确认
-> SemanticIntentExecutor 执行 intent 序列
-> BrowserConsole 调度 CAM 服务
-> OperationService / ToolPathService 等服务修改 Repository
-> BrowserTree / CamViewport 刷新
```

也就是说，大模型主要负责语义解析，真正的 CAM 业务仍然由传统 C++ 代码执行。



## 当前不足

目前还是原型阶段，主要不足：

- 几何体和刀轨都是 mock 数据
- 没有真实 CAM 内核
- 刀具库和参数策略还很小
- HumanInLoop 目标澄清能力还比较基础
- 没有接特征识别机器学习模型，未来可通过零件形状直接得到需要的工序类型
- 没有工序模板库，或知识库，所以大模型没有很好的依据去设置正确的工序参数，未来可根据用户提问知识库，得到复杂零件的加工策略
- Console作为调度器，既服务于传统CAM操作，也服务于大模型调用，目前只有BrowserConsole，未来根据业务扩展补充其他调度器。

## 目录说明

```text
include/camclaw/                 C++ 核心头文件
src/                             C++ 核心实现
qt/include/camclaw/qt/           Qt 界面头文件
qt/src/                          Qt 界面实现
python_backend/                  Python 大模型后端
tests/                           C++ 核心测试
qt/tests/                        Qt 自动化测试
docs/                            架构文档和测试说明
scripts/                         启动脚本
```

## 常见问题

### 找不到 Qt DLL

说明 Qt 的 `bin` 目录没有加入 `PATH`。

可以用脚本参数指定：

```powershell
.\scripts\run_qt_cam.ps1 -QtBinDir "你的Qt路径\bin" -QtCMakeDir "你的Qt路径\lib\cmake"
```

如果想继续用 bat，也可以在命令行里给 bat 传参数：

```powershell
.\scripts\run_qt_cam.bat -QtBinDir "你的Qt路径\bin" -QtCMakeDir "你的Qt路径\lib\cmake"
```

### Agent 提示 llm_service_unavailable

检查：

- `.env.local` 是否存在
- `CAMCLAW_LLM_API_KEY` 是否配置
- 修改 `.env.local` 后是否重启了后端
- `CAMCLAW_LLM_BASE_URL` 和 `CAMCLAW_LLM_MODEL` 是否正确

可以重启后端：

```powershell
.\scripts\run_qt_cam.ps1 -StartBackend -RestartBackend -SkipBuild
```



## 自测用例

手工测试说明在：

```text
./test.docx
```
