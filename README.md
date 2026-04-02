# SuZhouMetroIA

苏州地铁智能助手（Qt Widgets）示例项目，提供地铁地图交互、语音问答、模型函数调用与购票结算演示。

## 主要功能

1. 地铁网络可视化与站点选择。
2. 当前站点与目标站点联动显示，支持票价、里程、预计时长展示。
3. 站点信息面板支持：`去这里`、`设为当前站`、`复制站名`。
4. 左侧快速购票面板（纵向布局）：
   - 单程票快捷金额 `2~7` 元
   - 自定义单程票金额 `2~13` 元
   - 单日畅行旅游票 `25` 元
5. 结算面板（中文、简约）：
   - 票种、单价、张数、支付方式
   - 模拟支付后生成出票信息
   - 支持从地图站点和快速购票直接预填并进入结算
6. F9 模型设置面板：
   - 模型日志开关
   - 语音状态显示开关
   - 函数调用显示开关
   - 函数结果显示开关
   - 当前站点同步状态展示（同步到模型上下文）

## 快捷键

- `F7`：显示/隐藏文本输入面板
- `F8`：显示/隐藏结算面板
- `F9`：显示/隐藏模型设置面板
- `Esc`：优先关闭当前浮层；无浮层时关闭窗口

## 项目结构

```text
.
|- config/
|  |- api_keys.example.json
|  |- api_keys.json            (本地配置，不提交)
|  `- assistant_rules.json
|- resources/
|  `- metro_data.qrc
|- src/
|  |- models/
|  |- services/
|  |- utils/
|  |- widgets/
|  |- main.cpp
|  `- mainwindow.cpp/.h
|- docs/
`- task_plan.md
```

## 依赖

1. CMake 3.21+
2. 支持 C++17 的编译器
3. Qt6：`Widgets`、`Multimedia`、`Network`（`WebSockets` 可选）

说明：缺少 `Qt6::WebSockets` 仍可编译，但实时语音能力会降级。

## 配置

### 1) API Key

编辑 `config/api_keys.json`：

```json
{
  "Qwen3": {
    "LLM_service_key": "YOUR_KEY"
  }
}
```

可从 `config/api_keys.example.json` 复制生成。

### 2) 模型与规则

编辑 `config/assistant_rules.json`：

1. `llm_model`：LLM 模型名
2. `asr_realtime_model`：实时语音模型名
3. `response_rules`：系统回复规则

## 构建与运行

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64"
cmake --build build
```

## 日志

- 日志文件：`logs/activity.log`
- 覆盖范围：应用生命周期、语音状态、ASR 事件、LLM 请求/响应、函数调用、错误与状态变更
