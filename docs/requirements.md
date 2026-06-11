# lv 需求、架构与当前状态

本文档是当前代码库的主要交接文档，描述 `lv` 的目标、已实现功能、模块边界、交互方式、测试方式和后续开发注意事项。

## 项目目标

`lv` 是一个 C++20/ncurses 编写的大文件日志查看器 TUI。核心目标：

- 高效打开和显示大日志文件。
- 使用规则集实时过滤日志。
- 每条规则生成独立中间 bit array，最终结果生成 final bit array。
- UI 主线程保持可操作，过滤在后台线程执行。
- 支持类似 Vim 的基础浏览和命令编辑体验。

## 构建与运行

依赖：

- C++20 编译器
- CMake
- `ncursesw`
- `formw`

构建：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

运行：

```bash
./build/lv <log_file> [rule_file]
```

示例：

```bash
./build/lv test_log.txt tests/perf_rules.txt
```

测试：

```bash
./build/test_filter_engine
./build/test_line_editor
./build/perf_filter_engine test_log.txt tests/perf_rules.txt
```

## 规则语法

规则文件每行一条规则，空行和 `#` 开头的行会被忽略。

```text
s REGEX       # show regex
h REGEX       # hide regex
ss LITERAL    # show literal
hh LITERAL    # hide literal
```

示例：

```text
ss ERROR
hh DEBUG
s user=[0-9]+
h TRACE|VERBOSE
```

规则按顺序组成 pipeline：

- 第一条规则处理所有行。
- 第 N 条规则只处理第 N-1 条规则 bit 为 `1` 的行。
- 每一行会一次性跑完整个规则链，并写入所有中间 bit array 和 final bit array。
- 过滤不是“先算完第 1 层再算第 2 层”，而是逐行处理。

bit 含义：

- `1`：该行通过当前规则层，或者最终可显示。
- `0`：该行未通过当前规则层，后续规则不再处理该行。

## 当前功能

### 文件和过滤

- 使用 `MMapFile` 通过 `mmap` 读取日志文件。
- 使用 `LineIndex` 预计算每行起止位置。
- 使用 `BitArray` 存储规则中间结果和最终显示结果。
- 使用 `FilterEngine` 按行执行规则 pipeline。
- 使用后台线程执行过滤，完成后替换 UI 当前 final bitmap。
- `:save-filtered` 会等待最新后台过滤完成后再写文件。

### Log 窗口

- 显示当前日志文件名。
- 显示日志内容。
- 显示行号。
- 长行按窗口宽度换行。
- 当前行高亮。
- 根据 final bit array 只显示可见行。
- literal 规则命中文本高亮。
- 控制字符显示为 `.`，tab 显示为空格。
- 去除左右和底部边框；log 与 rules 之间只保留一条水平分割线。

### Rules 窗口

- 显示当前规则列表。
- 当前规则高亮。
- 支持规则列表滚动。
- 支持规则编辑、插入、删除、移动。
- 修改规则后触发后台重新过滤。

### 底部命令/编辑行

- 非编辑状态左侧显示 `normal`。
- 状态信息右对齐显示。
- 编辑状态使用 `ncurses formw` 作为底层编辑控件。
- 支持左右移动、Home/End、插入、删除、Backspace、Insert 模式、`Ctrl-U` 清行。
- 支持历史记录，上下方向键切换历史。

## 键盘操作

### 全局

| 按键 | 行为 |
|------|------|
| `Tab` | 在 log/rules 窗口间切换焦点 |
| `:` | 进入命令模式 |
| `q` | 退出 |

### Log 窗口

| 按键 | 行为 |
|------|------|
| `j` / `Down` | 下一条可见日志行 |
| `k` / `Up` | 上一条可见日志行 |
| `g` / `Home` | 跳到第一条可见日志行 |
| `G` / `End` | 跳到最后一条可见日志行 |
| `PageDown` / `Ctrl-F` | 向下翻页 |
| `PageUp` / `Ctrl-B` | 向上翻页 |
| `a` | 在当前规则后插入新规则 |
| `i` | 在当前规则前插入新规则 |
| `A` | 在规则末尾插入新规则 |
| `I` | 在规则开头插入新规则 |

### Rules 窗口

| 按键 | 行为 |
|------|------|
| `j` / `Down` | 选择下一条规则 |
| `k` / `Up` | 选择上一条规则 |
| `Enter` | 编辑当前规则 |
| `a` | 在当前规则后插入新规则 |
| `i` | 在当前规则前插入新规则 |
| `A` | 在规则末尾插入新规则 |
| `I` | 在规则开头插入新规则 |
| `x` | 删除当前规则 |
| `[` | 当前规则上移 |
| `]` | 当前规则下移 |

### 命令模式

| 命令 | 行为 |
|------|------|
| `:open <log_file>` | 打开新的日志文件并重新过滤 |
| `:rules <rule_file>` | 载入规则文件并重新过滤 |
| `:write-rules [rule_file]` | 保存当前规则；省略路径时保存到当前规则文件 |
| `:save-filtered <output_file>` | 保存当前 final bit array 可见的日志行 |
| `:quit` / `:q` | 退出 |

## 当前架构

### 数据流

```text
log file
  -> MMapFile
  -> LineIndex
  -> FilterEngine + RuleSet
  -> FilterResult
       - layer bit arrays
       - final bit array
  -> AppUi log renderer
```

UI 数据流：

```text
Screen
  -> AppUi
       - log window
       - rules window
       - LineEditor
       - background filter jobs
```

### 模块清单

| 模块 | 文件 | 职责 |
|------|------|------|
| `BitArray` | `src/core/bit_array.*` | 紧凑位图，底层为 `std::vector<uint64_t>` |
| `MMapFile` | `src/core/mmap_file.*` | mmap 文件读取 |
| `LineIndex` | `src/core/line_index.*` | 行起止位置索引，返回 `std::string_view` |
| `Rule` | `src/core/rule.*` | 单条 show/hide + regex/literal 规则 |
| `RuleSet` | `src/core/rule_set.*` | 规则解析、保存、增删改移 |
| `FilterEngine` | `src/core/filter_engine.*` | 按行执行规则 pipeline |
| `FilterResult` | `src/core/filter_result.*` | 中间 layer bitmaps 和 final bitmap |
| `Screen` | `src/ui/screen.*` | ncurses 初始化和终端尺寸 |
| `LineEditor` | `src/ui/line_editor.*` | 基于 formw 的底部行编辑器 |
| `AppUi` | `src/ui/app_ui.*` | 窗口布局、输入分发、渲染、后台过滤任务 |
| `main` | `src/main.cpp` | 解析启动参数、打开文件、启动 UI |

## 后台过滤模型

`AppUi::start_filter_job()` 会复制当前 `RuleSet` 和 `LineIndex` 快照，并启动一个后台线程执行 `FilterEngine::run()`。

后台线程完成后：

- `poll_filter_jobs()` 在 UI 主循环中发现完成状态。
- 最新 generation 的 `FilterResult` 被设为 `active_filter_`。
- `filter_bitmap_` 指向 `active_filter_->final()`。
- log 窗口下一次重绘时只显示 final bit 为 `1` 的行。

注意：

- 当前实现没有取消旧过滤线程；旧线程完成后会被 generation 机制忽略。
- `:open` 会先 `join_filter_jobs()`，避免替换 mmap 后后台线程还持有旧 `LineIndex` 数据。
- `:save-filtered` 会先等待过滤完成，保证保存的是最新 final bitmap。

## 性能特征

- 文件内容不按行复制，显示和过滤使用 `std::string_view`。
- `LineIndex` 扫描文件一次生成行起点数组。
- 过滤复杂度约为 `O(可处理行数 * 规则数)`，但后续规则只处理上一层 bit 为 `1` 的行。
- 每个 bit array 内存约为 `line_count / 8` 字节。
- N 条规则的 `FilterResult` 约有 `N + 1` 个 bit array。

## 测试

### `test_filter_engine`

覆盖：

- bit array 基础行为。
- 行索引，包括空文件、无结尾换行、长行。
- 规则解析和非法输入。
- 规则保存/加载。
- 规则增删改移和插入。
- show/hide literal。
- show regex。
- 多层 pipeline 的中间 bit array 和 final bitmap。

### `test_line_editor`

覆盖：

- 插入和提交。
- 光标移动和 Backspace。
- 右键不能越过内容末尾。
- Delete/Home/End。
- Esc 取消。
- 历史记录。

### `perf_filter_engine`

用法：

```bash
./build/perf_filter_engine <log_file> <rule_file>
```

输出文件大小、行数、规则数量、可见行数、bitmap 内存、索引耗时、过滤耗时和吞吐。

## 已知限制

- regex 高亮范围未实现；当前只高亮 literal 规则的匹配文本。
- 滚动是按“可见文件行”移动，不是严格按换行后的屏幕行移动。
- 后台过滤不支持取消，只通过 generation 忽略旧结果。
- 过滤结果没有增量复用；规则变化后会重新计算完整结果。
- 规则语法不支持大小写选项、prefix/suffix、反向组合表达式等扩展。
- `LineEditor` 使用 `formw`，适合嵌入 ncurses 窗口；没有使用 readline/libedit，因为它们通常接管整个终端输入。
- 当前没有独立的 view model/display buffer 抽象，log 渲染和可见行导航仍在 `AppUi` 内。

## 后续建议

优先级较高：

1. 抽出 log view model，集中处理可见行、换行行、光标和滚动。
2. 增加过滤任务取消或协作式停止，避免快速改规则时后台堆积。
3. 支持规则结果复用，未变化规则不重复计算。
4. 支持 regex 匹配范围高亮。
5. 增加更多 TUI 行为测试，可考虑用 pty/screenshot 做端到端验证。

优先级中等：

1. 支持大小写不敏感规则。
2. 支持 prefix/suffix/contains 的更明确语法。
3. 支持命令补全和规则历史分类。
4. 支持状态栏展示当前行号、过滤数量、规则数量、过滤中状态。

## 开发注意事项

- 不要让 core 模块依赖 ncurses。
- `LineIndex` 持有 mmap 数据指针快照；替换文件时必须重建 `LineIndex`。
- 后台线程不能引用会被 UI 替换的 `MMapFile`；当前做法是复制 `LineIndex` 快照。
- `active_filter_` 持有 final bitmap 生命周期，`filter_bitmap_` 只是指向它内部数据。
- UI 空闲时不应持续重绘；当前使用 `dirty_` 标记避免闪烁。
- 非编辑状态下状态文字右对齐，左侧 `normal` 不应被覆盖。
