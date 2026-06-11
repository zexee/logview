# lv 开发计划与当前进度

本文档记录开发计划和当前阶段状态。更完整的需求、架构和交互说明见 `docs/requirements.md`。

## 当前进度概览

已完成：

- 核心过滤模块。
- mmap 文件读取和行索引。
- 每规则中间 bit array 和 final bit array。
- 按行 pipeline 过滤语义。
- 单元测试和性能测试。
- ncurses TUI 基础框架。
- log/rules/editor 三段式界面。
- log 窗口真实显示、行号、长行换行、当前行高亮。
- final bitmap 过滤显示。
- 后台过滤线程和 UI 主线程分离。
- rules 窗口编辑、插入、删除、移动。
- 命令模式打开文件、载入/保存规则、保存过滤结果。
- 基于 `formw` 的底部行编辑器和历史记录。
- Vim 风格 log 浏览快捷键。

未完成或待优化：

- regex 匹配范围高亮。
- 过滤任务取消。
- 规则结果增量复用。
- 严格按屏幕换行行滚动。
- 独立 log view model/display buffer 抽象。
- 更完整的 TUI 自动化测试。

## 已完成阶段

### 阶段 1：规则过滤核心模块

状态：完成。

产物：

- `src/core/bit_array.*`
- `src/core/mmap_file.*`
- `src/core/line_index.*`
- `src/core/rule.*`
- `src/core/rule_set.*`
- `src/core/filter_engine.*`
- `src/core/filter_result.*`
- `tests/test_filter_engine.cpp`
- `tests/perf_filter_engine.cpp`

验收：

```bash
./build/test_filter_engine
./build/perf_filter_engine test_log.txt tests/perf_rules.txt
```

### 阶段 2：UI 基础框架

状态：完成。

产物：

- `src/ui/screen.*`
- `src/ui/app_ui.*`
- `src/ui/line_editor.*`

说明：

- `Screen` 管理 ncurses 生命周期。
- `AppUi` 管理窗口布局、输入分发、后台过滤。
- `LineEditor` 基于 `formw`，嵌入底部一行。

### 阶段 3：窗口渲染和显示 buffer

状态：MVP 完成。

已实现：

- log 文件内容显示。
- 行号。
- 长行换行。
- 当前行高亮。
- literal 命中高亮。
- 根据 final bitmap 过滤显示。
- rules 列表显示和滚动。

待优化：

- 严格按屏幕行滚动。
- regex 高亮。
- 独立 view model。

### 阶段 4：组合过滤和显示

状态：完成。

已实现：

- 启动后后台过滤。
- 规则变更后后台重新过滤。
- 过滤完成后替换 final bitmap。
- UI 空闲时不持续重绘，避免闪烁。

### 阶段 5：规则窗口

状态：完成。

已实现：

- `Enter` 编辑当前规则。
- `a` 当前规则后插入。
- `i` 当前规则前插入。
- `A` 末尾插入。
- `I` 开头插入。
- `x` 删除。
- `[` 上移。
- `]` 下移。

`i/I/a/A` 在 log 窗口也可用，语义和 rules 窗口一致。

### 阶段 6：命令模式和文件操作

状态：完成。

已实现：

- `:open <log_file>`
- `:rules <rule_file>`
- `:write-rules [rule_file]`
- `:save-filtered <output_file>`
- `:quit` / `:q`

## 后续路线

### P0：稳定性和正确性

- 增加 TUI 端到端测试，覆盖常用快捷键、命令和行编辑行为。
- 修复所有可复现显示问题。
- 确保窗口 resize 后布局和 form 子窗口始终正确。

### P1：过滤性能和交互体验

- 过滤任务支持取消。
- 规则变化时复用未变化规则的 bit array。
- 支持过滤中状态提示。
- 支持规则保存脏状态提示。

### P2：显示能力增强

- 抽出 `LogView` 或 `DisplayBuffer`，不要让 `AppUi` 继续膨胀。
- 按屏幕显示行精确滚动。
- regex 匹配范围高亮。
- 支持横向滚动模式，作为长行换行的可选替代。

### P3：规则语言扩展

- 大小写不敏感选项。
- prefix/suffix/contains 快捷语法。
- 规则启用/禁用。
- 规则分组和注释保留。

## 当前键盘约定

### Log 窗口

- `j/k` 或方向键：上下移动。
- `g` 或 Home：到首条可见行。
- `G` 或 End：到末条可见行。
- PageUp/PageDown 或 Ctrl-B/Ctrl-F：翻页。
- `i/I/a/A`：新增规则。

### Rules 窗口

- `j/k` 或方向键：上下选择。
- `Enter`：编辑。
- `i/I/a/A`：新增规则。
- `x`：删除。
- `[` / `]`：移动。

### 命令/编辑行

- `:`：进入命令模式。
- Enter：提交。
- Esc：取消。
- Up/Down：历史。
- Ctrl-U：清行。

## 交接备注

- `cats_tigerie.exe-20260604T091233.log` 当前是未跟踪本地文件，不属于项目源码。
- `build/` 被 `.gitignore` 忽略。
- core 层保持无 UI 依赖。
- UI 层依赖 `ncursesw` 和 `formw`。
