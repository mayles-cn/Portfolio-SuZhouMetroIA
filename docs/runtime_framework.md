# 运行时框架（Runtime Framework）

本文档描述 SuZhouMetroIA 的运行时组织方式、事件流与关键稳定性策略。

## 1. 分层模型

### 1.1 UI 层

- `MainWindow`：主窗口与全局布局调度。
- `MetroMapWidget`：地图与站点交互。
- `ChatPanelWidget`：聊天与语音结果展示。
- `VoiceAssistantWidget`：语音采集入口。
- `StationPanelWidget`：站点详情与“去这里”动作。
- `RoutePlanWidget`：路线图形化展示。
- `SettlementWidget`：结算与出票。
- `TextPromptWidget`：快捷文本输入。
- `ModelSettingsWidget`：模型调试项开关。
- `InformationBarWidget`：底部公告滚动条。

### 1.2 编排层

- `MainWindow` 通过 `signal/slot` 串联核心业务流程：
  - 地图选站 -> 路线规划更新
  - 地图选站/快速购票 -> 结算预填并打开
  - 结算完成 -> 聊天区重置
  - F7/F8/F9 管理浮层开关

- `ChatPanelWidget` 负责语音与模型请求编排：
  - partial/final 文本合并
  - 去重节流
  - 一次语音会话只提交一条有效文本

### 1.3 服务层

- `SpeechRealtimeService`：实时 ASR 通道维护与音频提交。
- `QwenAssistantService`：LLM 请求、工具回合、流式输出。
- `MetroToolService`：线路/站点/票价领域工具。
- `FareService`、`RouteService`：票价与路径算法支持。
- `AppConfigService`：模型与规则配置。
- `UiStyleService`：统一样式 JSON 读取。
- `ActivityLogger`：结构化日志。

### 1.4 数据层

- `src/models/suzhou_metro_network_core.json`：地铁网络基础数据。
- `src/models/informationBar.json`：公告条内容与滚动参数。
- `src/models/station_maps/*.png`：站点周边示意图。
- `resources/metro_data.qrc`：Qt 资源汇总入口。

## 2. 关键运行链路

### 2.1 文本问答链路

1. 用户在 `TextPromptWidget` 或聊天框输入问题。
2. `ChatPanelWidget` 发送 `promptSubmitted`。
3. `QwenAssistantService` 发起请求。
4. 若触发工具调用：`QwenAssistantService -> MetroToolService`。
5. 模型文本以流式分片返回至聊天面板。

### 2.2 语音问答链路

1. 用户按下语音球，`VoiceAssistantWidget` 开始采集。
2. `SpeechRealtimeService` 回传 partial/final 文本。
3. `ChatPanelWidget` 合并本轮分段，保持单条用户气泡。
4. 释放语音球后触发一次最终提交。

### 2.3 地图到结算链路

1. 地图选站，更新 `StationPanelWidget`。
2. 点击“去这里”触发 `routeSettlementRequested`。
3. `SettlementWidget::prefillRouteTicket` 填充并展示订单。

### 2.4 快速购票链路

1. 快速购票按钮触发 `quickPurchaseRequested`。
2. `SettlementWidget::prefillQuickTicket` 生成快捷订单。

## 3. 并发与状态控制

1. `QwenAssistantService` 采用单飞请求模型：
   - 同时仅允许一个在途请求。
   - 若新请求到来，保留最新请求覆盖排队。

2. ASR 与 LLM 解耦：
   - 语音链路仅负责转写，提交时机由 UI 编排层决定。

3. 面板状态隔离：
   - 结算、文本输入、模型设置面板各自维护显隐状态。

## 4. 稳定性策略

1. ASR 通道异常自动重连。
2. 网络超时自动重试一次（模型请求）。
3. 工具调用失败时返回结构化错误，不中断主流程。
4. 关键节点均有日志埋点，便于回放问题。

## 5. 样式系统接入点（本次改动）

- 新增 `UiStyleService`，统一从 `config/ui_styles.json` 读取 QSS。
- 控件调用模式：`UiStyleService::style(key, fallback)`。
- 若配置缺失，自动使用 fallback，保证 UI 可用性。

相关说明见：[style_system.md](style_system.md)
