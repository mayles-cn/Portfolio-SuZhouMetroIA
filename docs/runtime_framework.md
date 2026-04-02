# 运行时框架

## 1. 分层结构

- UI 层
  - `MainWindow`
  - `ChatPanelWidget`
  - `VoiceAssistantWidget`
  - `MetroMapWidget`
  - `SettlementWidget`
  - `ModelSettingsWidget`
- 编排层
  - `ChatPanelWidget` 负责交互编排（语音启停、转写合并、消息分发）
- 服务层
  - `SpeechRealtimeService`：ASR WebSocket、音频提交策略、重连
  - `QwenAssistantService`：模型请求流水线、函数调用、重试
  - `MetroToolService`：地铁领域工具执行
  - `AppConfigService`：配置加载（Key/模型/规则）
  - `ActivityLogger`：结构化日志
- 数据层
  - `src/models/` 下的 JSON 数据与模型

## 2. 关键交互流程

1. 用户通过文本或语音发起请求。
2. `ChatPanelWidget` 负责转写拼接与去重，触发模型请求。
3. `QwenAssistantService` 携带工具定义请求模型。
4. 如果模型返回工具调用，执行工具并续轮请求。
5. 模型回复以流式片段返回到聊天面板。
6. 当回复涉及购票/结算意图时，可打开结算面板。
7. 地图侧支持两条直接购票链路：
   - 站点信息面板 `去这里` -> 预填路线购票 -> 打开结算
   - 左侧快速购票 -> 预填快捷票单 -> 打开结算

## 3. 稳定性策略

- ASR
  - 启动预连接，断线自动重连
  - 提交保护：无音频不提交、服务端已提交不重复提交
- LLM
  - 单飞请求（同一时间仅一个请求）
  - 最新请求排队（last-write-wins）
  - 短暂网络异常超时重试一次
  - 对话历史长度控制
- UI
  - 面板显示/隐藏由快捷键控制：`F7/F8/F9`
  - 结算面板使用动画展开，防止突兀跳变

## 4. 日志分类

- 生命周期：`app`、`ui.window`
- 语音：`asr.*`、`asr.socket*`、`asr.capture`、`asr.final`
- 模型：`llm.request`、`llm.http`、`llm.response`、`llm.tool`
- 流式：`llm.stream`、`llm.stream.delta`
- 交互：`chat.message`、`service.status`

## 5. 产品约束

- 中文优先界面与文案。
- 地铁领域问答严格限定在苏州地铁范围。
- 涉及时效性信息时需避免无依据陈述。
- 购票与支付流程为模拟演示，不接入真实支付通道。
