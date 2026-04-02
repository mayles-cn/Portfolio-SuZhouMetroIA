# 严格 PM 质量门禁

## A. 必须通过的用户体验

- UI 不允许出现乱码。
- 助手回复必须可见流式输出（逐步渲染），不能一次性整段追加。
- 每轮助手回复都应产生 `llm.stream -> llm.stream.delta -> llm.stream finished` 日志链路。
- 可恢复错误（如 WebSocket 空闲断开）不应直接向用户暴露底层传输报错。
- 一次按住说话并松开，只能触发一次有效助手回复，禁止重复触发。
- 请求超时时应先自动重试一次，再提示失败。

## B. 必须通过的可靠性

- `input_audio_buffer.clear` 错误数必须为 0。
- `Error committing input audio buffer` 应接近 0，并持续优化降低。
- 用户连续语音输入时，不得出现无限堆积请求。
- ASR 通道掉线后必须可自动恢复，无需重启应用。

## C. 必须通过的性能

- 常见场景 `llm.http -> llm.response` 目标：<= 6 秒。
- P95 延迟目标：<= 12 秒。
- 排队等待最多保留 1 条待处理语句（最新优先覆盖旧请求）。

## D. 发版日志检查清单

1. 统计 `asr.error`、`llm.http.error`、`llm.http.retry`。
2. 校验每个 `llm.response` 是否有且仅有一个 `llm.stream finished`。
3. 校验正常长度回复的 `llm.stream.delta` 数量 > 1（防止伪流式）。
4. 校验单次语音采集没有重复 final dispatch。
5. 校验票价/线路/站点查询均有函数调用链路日志。
6. 校验用户可见状态气泡不泄露英文系统提示。
