# 模型函数调用清单

本文档描述 `QwenAssistantService` 当前可用的函数调用（Tool Calling）能力。

## 1. `list_lines`
- 用途：查询当前地铁线路清单。
- 入参：无。
- 返回字段：
  - `ok`：是否成功。
  - `line_count`：线路数量。
  - `lines`：线路名称数组。

## 2. `query_station_lines`
- 用途：查询某站点所属线路。
- 入参：
  - `station_name` (string)：站点名称（支持模糊匹配）。
- 返回字段：
  - `ok`：是否成功。
  - `station_id`：站点 ID。
  - `station_name`：标准站名。
  - `fuzzy_matched`：是否走了模糊匹配。
  - `lines`：该站所属线路数组。
  - 失败时：`error = station_not_found`，并返回 `suggestions` 候选站名。

## 3. `estimate_fare`
- 用途：估算两站间票价与里程（基于本地拓扑和里程映射）。
- 入参：
  - `from_station_name` (string)
  - `to_station_name` (string)
- 返回字段：
  - `ok`：是否成功。
  - `from_station_name`：起点站名（标准化后）。
  - `to_station_name`：终点站名（标准化后）。
  - `from_fuzzy_matched`：起点是否模糊匹配。
  - `to_fuzzy_matched`：终点是否模糊匹配。
  - `estimated_distance_km`：估算里程（公里）。
  - `estimated_fare_yuan`：估算票价（元）。
  - 失败时可能返回：
    - `error = station_not_found`（附带候选站名）
    - `error = fare_calculation_failed`
