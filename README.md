# SuZhouMetroIA

苏州地铁智能助手（Qt Widgets）。
项目聚焦“地铁问答 + 地图选站 + 购票结算 + 语音交互 + 工具调用”一体化演示，并支持通过 JSON 统一管理界面样式。

## 第一部分：常规 README 使用说明

### 1. 核心功能

1. 苏州地铁线路地图展示、缩放、平移、站点点击。
2. 当前站点与目标站点联动，展示票价、里程、预计时长。
3. 站点信息面板支持“去这里”，可直达结算。
4. 左侧快速购票面板支持：
   - 单程票快捷金额（2~7 元）
   - 自定义单程票（2~13 元）
   - 单日畅行旅游票（25 元）
5. 结算面板支持预填订单、模拟支付、出票信息展示。
6. 聊天面板支持文本与语音输入，模型回复支持流式渲染。
7. F9 模型设置面板支持调试信息开关与站点同步策略。
8. 样式系统改为 `config/ui_styles.json` 统一管理（支持代码兜底样式）。

### 2. 技术栈

- C++17
- Qt6 Widgets / Multimedia / Network（WebSockets 可选）
- CMake
- 本地 JSON 数据驱动（线路、公告、配置、样式）

### 3. 环境要求

1. CMake 3.21+
2. 支持 C++17 的编译器（MinGW/MSVC 均可）
3. Qt 6.11+（建议）

### 4. 配置文件

`config/` 下建议保留并维护以下文件：

1. `api_keys.example.json`：密钥模板。
2. `api_keys.json`：本地密钥（已在 `.gitignore` 中忽略，不提交）。
3. `assistant_rules.json`：模型与回复规则配置。
4. `ui_styles.json`：全局 QSS 样式中心。

### 5. 构建与运行

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64"
cmake --build build
```

可执行文件位于：`build/bin/SuZhouMetroIA.exe`

### 6. 快捷键

- `F7`：显示/隐藏文本输入面板
- `F8`：显示/隐藏结算面板
- `F9`：显示/隐藏模型设置面板
- `Esc`：优先关闭浮层；无浮层时关闭主窗口

### 7. 相关文档

- 运行时框架：[docs/runtime_framework.md](docs/runtime_framework.md)
- 函数调用清单：[docs/model_function_calls.md](docs/model_function_calls.md)
- 样式系统说明：[docs/style_system.md](docs/style_system.md)
- 严格质量门禁：[docs/strict_pm_quality_gates.md](docs/strict_pm_quality_gates.md)
- 项目简历描述：[docs/简历项目描述.md](docs/简历项目描述.md)

---

## 第二部分：项目工程逻辑详细解说

### 1. 总体分层设计

工程采用“UI 层 + 交互编排层 + 服务层 + 数据层”的结构：

1. UI 层（`src/widgets/` + `MainWindow`）
   - 负责可视化展示、输入交互、状态反馈。
2. 编排层（以 `ChatPanelWidget`、`MainWindow` 为主）
   - 负责跨控件事件连接、流程衔接、面板联动。
3. 服务层（`src/services/`）
   - 负责模型请求、工具执行、语音链路、日志、配置与样式加载。
4. 数据层（`src/models/` + `resources/`）
   - 负责线路网络、公告文本、站点地图等静态数据。

### 2. 关键控件设计说明

1. `MainWindow`
   - 顶层容器与布局调度中心。
   - 挂接全局快捷键（F7/F8/F9）、浮层显隐、结算动画。
   - 串联地图、聊天、结算、设置、路线规划等控件事件。

2. `MetroMapWidget`
   - 地铁地图绘制核心，负责线路绘制、站点选中、视图变换。
   - 管理线路筛选条、快速购票栏、站点信息面板。
   - 输出业务信号：站点选中、当前站点变更、路线购票请求、快速购票请求。

3. `StationPanelWidget`
   - 展示当前/目标站、线路、票价、里程、时长。
   - 提供“去这里”动作入口，触发结算联动。

4. `RoutePlanWidget`
   - 根据当前站与目标站做最短路径与换乘段分析。
   - 用蛇形/之字形线路图表达换乘结构，不依赖大量文字。

5. `ChatPanelWidget`
   - 统一文本输入、语音转写、消息气泡、流式回复渲染。
   - 对 ASR 分段结果做合并与去重，控制提交节奏。

6. `VoiceAssistantWidget`
   - 语音球采集入口，负责麦克风输入、可视化动效、音频帧输出。

7. `SettlementWidget`
   - 票单确认、支付方式选择、数量调整、模拟支付与出票结果展示。
   - 支持来自地图路线购票与快速购票的预填逻辑。

8. `ModelSettingsWidget`
   - 管理调试项开关（模型状态、函数调用、函数结果、语音状态等）。
   - 管理“选中站点后自动设为当前站”行为。

9. `TextPromptWidget`
   - 顶部快捷文本输入面板，适配键盘输入场景。

10. `InformationBarWidget`
   - 底部滚动公告条，读取 JSON 内容并动态滚动展示。

### 3. 服务层职责说明

1. `QwenAssistantService`
   - 维护模型对话上下文。
   - 构造工具 schema，处理 tool call 回合。
   - 支持排队策略、超时重试、流式分片输出。

2. `MetroToolService`
   - 提供地铁领域函数能力：线路清单、站点所属线路、票价估算。

3. `SpeechRealtimeService`
   - 管理实时语音识别链路（连接、重连、音频提交、事件回调）。

4. `FareService` / `RouteService`
   - 负责票价估算与路径相关算法支持。

5. `AppConfigService`
   - 读取 `api_keys.json` 与 `assistant_rules.json`，提供模型配置。

6. `UiStyleService`（本次关键改动）
   - 读取 `config/ui_styles.json`，按 key 分发 QSS。
   - 各控件不再硬编码样式，统一走 `UiStyleService::style(key, fallback)`。
   - 若 JSON 缺项，自动回退到代码 fallback 样式，保证可运行。

7. `ActivityLogger`
   - 结构化日志记录（交互、模型、语音、工具、错误）。

### 4. JSON 样式系统设计

#### 4.1 目标

- 统一样式入口，减少散落在代码中的 QSS 字符串。
- 提高样式维护效率，降低 UI 调整成本。
- 允许“配置优先 + 代码兜底”，避免因配置缺失导致 UI 崩坏。

#### 4.2 配置格式

`config/ui_styles.json`：

- `version`：样式版本。
- `description`：说明。
- `styles`：键值映射（`style_key -> qss_string`）。

示例 key：

- `mainwindow.window`
- `chat_panel.root`
- `settlement.panel`
- `metro_map.quick_buy_button`
- `route_plan.root`

#### 4.3 代码接入方式

```cpp
setStyleSheet(szmetro::UiStyleService::style(
    QStringLiteral("text_prompt.root"),
    QStringLiteral("background-color: rgba(255,255,255,240);")));
```

说明：

1. 先按 key 从 JSON 读取样式。
2. key 不存在时使用 fallback。
3. fallback 保证旧版本视觉稳定。

### 5. 关键交互链路

1. 文本问答链路
   - `TextPromptWidget/ChatPanelWidget` 输入 -> `QwenAssistantService` -> 工具调用（按需）-> 流式回复。

2. 语音问答链路
   - `VoiceAssistantWidget` 采集音频 -> `SpeechRealtimeService` 转写 -> `ChatPanelWidget` 合并分段 -> `QwenAssistantService`。

3. 地图到结算链路
   - 站点选中 -> `StationPanelWidget` 点击“去这里” -> `MetroMapWidget::routeSettlementRequested` -> `SettlementWidget::prefillRouteTicket`。

4. 快速购票链路
   - 左侧快速购票按钮 -> `quickPurchaseRequested` -> `SettlementWidget::prefillQuickTicket`。

### 6. 工程目录与职责

```text
.
├─ config/
│  ├─ api_keys.example.json
│  ├─ api_keys.json
│  ├─ assistant_rules.json
│  └─ ui_styles.json
├─ docs/
├─ resources/
│  ├─ icons/
│  └─ images/
├─ src/
│  ├─ models/
│  ├─ services/
│  ├─ utils/
│  ├─ widgets/
│  ├─ main.cpp
│  └─ mainwindow.cpp/.h
└─ README.md
```

### 7. 维护建议

1. 新增控件样式时，先在 `config/ui_styles.json` 增加 key，再在代码里接入该 key。
2. key 命名建议：`模块.控件.状态`，例如 `settlement.pay_button`。
3. 保留合理 fallback，避免线上配置缺失导致样式回退异常。
4. 对变更较大的 UI，先在 docs 中补充说明再提交。
