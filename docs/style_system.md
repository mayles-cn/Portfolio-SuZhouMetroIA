# 样式系统说明（JSON 统一管理）

## 1. 改动背景

项目此前样式分散在多个控件的 `setStyleSheet` 字符串中，存在以下问题：

1. 修改成本高，需要跨文件查找。
2. 同类控件风格容易偏移，缺乏统一来源。
3. 设计迭代时无法做到“配置化调样”。

为解决上述问题，新增 `UiStyleService` + `config/ui_styles.json`。

## 2. 设计目标

1. 样式配置集中管理，便于统一维护。
2. 代码层保留 fallback，配置缺失时不影响运行。
3. 命名规范统一，支持持续扩展。

## 3. 文件结构

1. 配置文件：`config/ui_styles.json`
2. 服务文件：
   - `src/services/style_service.h`
   - `src/services/style_service.cpp`

## 4. JSON 格式

```json
{
  "version": 1,
  "description": "样式中心",
  "styles": {
    "mainwindow.window": "background-color: #f7f8f8;",
    "chat_panel.root": "background-color: rgba(230,230,230,230);"
  }
}
```

说明：

1. `styles` 的 key 为样式唯一标识。
2. value 为 QSS 字符串。
3. 推荐使用单行 QSS，减少转义复杂度。

## 5. 代码使用方式

```cpp
setStyleSheet(szmetro::UiStyleService::style(
    QStringLiteral("text_prompt.root"),
    QStringLiteral("background-color: rgba(255,255,255,240);")));
```

逻辑：

1. 先从 JSON 按 key 读取。
2. 若 key 缺失，使用 fallback。
3. 保证 UI 在配置不完整时仍可用。

## 6. 路径解析策略

`UiStyleService` 按以下候选顺序查找 `ui_styles.json`：

1. `./config/ui_styles.json`
2. `../config/ui_styles.json`
3. `../../config/ui_styles.json`
4. `应用目录/config/ui_styles.json`

## 7. 已接入模块

1. `MainWindow`
2. `ChatPanelWidget`
3. `ModelSettingsWidget`
4. `MetroMapWidget`（线路筛选与快速购票相关样式）
5. `StationPanelWidget`
6. `SettlementWidget`
7. `TextPromptWidget`
8. `RoutePlanWidget`

## 8. 样式 key 命名建议

1. 采用三段式：`模块.控件.状态`
2. 示例：
   - `settlement.pay_button`
   - `metro_map.quick_buy_button`
   - `model_settings.checkbox`

## 9. 扩展与维护建议

1. 新增控件前先定义 key，再写控件样式引用。
2. 尽量复用已有 key，避免同风格重复定义。
3. 每次 UI 改动同步更新 README/docs，保持文档一致。
