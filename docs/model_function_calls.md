# 模型函数调用清单（Tool Calling）

本文档描述 `QwenAssistantService` 当前注册的工具函数、参数与返回约定。

## 1. `list_lines`

### 用途

获取苏州地铁线路清单。

### 参数

- 无

### 返回

- `ok` (bool)：是否成功
- `line_count` (int)：线路数量
- `lines` (array<string>)：线路显示名列表

---

## 2. `query_station_lines`

### 用途

查询指定站点所属线路，支持模糊匹配与候选建议。

### 参数

- `station_name` (string)：站点名称

### 成功返回

- `ok` (bool) = `true`
- `station_id` (string)：标准站点 ID
- `station_name` (string)：标准站点名
- `fuzzy_matched` (bool)：是否通过模糊匹配命中
- `lines` (array<string>)：该站所属线路

### 失败返回

- `ok` (bool) = `false`
- `error` = `station_not_found`
- `station_name`：原始输入
- `suggestions` (array<string>)：候选站点

---

## 3. `estimate_fare`

### 用途

估算两站间票价与里程（基于本地网络与票价策略）。

### 参数

- `from_station_name` (string)：起点站
- `to_station_name` (string)：终点站

### 成功返回

- `ok` (bool) = `true`
- `from_station_name` (string)：规范化后的起点站名
- `to_station_name` (string)：规范化后的终点站名
- `from_fuzzy_matched` (bool)
- `to_fuzzy_matched` (bool)
- `estimated_distance_km` (number)
- `estimated_fare_yuan` (int)

### 失败返回

- `ok` (bool) = `false`
- 可能的 `error`：
  - `station_not_found`
  - `fare_calculation_failed`
- `from_suggestions` / `to_suggestions`：候选站点（站点未命中时）

---

## 4. `open_settlement_panel`

### 用途

在 UI 中打开结算面板，用于用户明确购票/支付意图场景。

### 参数

- 无

### 返回

- `ok` (bool)
- `action` (string) = `open_settlement_panel`

---

## 5. 工具调用链路

1. `QwenAssistantService` 在请求中注入工具 schema。
2. 模型返回 `tool_calls` 时，服务层执行对应函数。
3. 函数结果会以 `tool` 角色消息回灌模型，进入下一轮推理。
4. 调用与结果通过 `statusChanged` 和 `ActivityLogger` 输出。

---

## 6. 设计约束

1. 工具结果必须结构化、可序列化（JSON）。
2. 工具不可依赖实时外网数据，保证离线可演示。
3. 站点匹配失败时必须返回候选建议，不能静默失败。
4. `open_settlement_panel` 仅负责 UI 动作触发，不负责订单计算。
