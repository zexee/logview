# lv — 需求与开发计划文档

## 项目概述

`lv` 是一个基于 ncurses 的大文件日志查看器 TUI，用 C++20 编写。核心目标是支持数十万到数百万行日志文件的高效查看和实时过滤。

### 技术栈
- C++20
- ncurses (TUI)
- POSIX mmap (大文件内存映射)
- CMake 构建系统
- POSIX 线程 (std::async/std::future)

### 构建命令
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 当前架构

### 数据流

```
文件 → MMapFile (mmap) → LineIndex (O(log N) 偏移查找)
                                     ↓
                          FilterChain (多个 Filter)
                                     ↓
                    AsyncFilterRunner (多线程异步计算)
                                     ↓
                         FilterBitmap (每过滤器一个)
                                     ↓
                       combined bitmap (AND 所有过滤器)
                                     ↓
                       DisplayBuffer (rank/select 结构)
                                     ↓
                         Renderer (ncurses 渲染)
                                     ↓
                              Screen (终端)
```

### 核心组件

| 组件 | 文件 | 职责 |
|------|------|------|
| `MMapFile` | `src/core/mmap_file.h/.cpp` | mmap 文件，提供 `data()` 和 `size()` |
| `LineIndex` | `src/core/line_index.h/.cpp` | 块索引（每10000行一个块），`get_offset(n)` O(log N) |
| `Filter` | `src/filter/filter.h/.cpp` | 基类，`matches()` 虚函数 + `compute_batch()` 批量计算 |
| `RegexFilter` | `src/filter/regex_filter.h/.cpp` | 正则表达式过滤器实现 |
| `FilterChain` | `src/filter/filter_chain.h/.cpp` | 过滤器列表，顺序执行 |
| `FilterBitmap` | `src/display/filter_bitmap.h/.cpp` | 每过滤器位图，`std::vector<uint64_t>` |
| `AsyncFilterRunner` | `src/display/async_filter.h/.cpp` | 多线程异步过滤计算 |
| `DisplayBuffer` | `src/display/display_buffer.h/.cpp` | 块索引位图 + rank/select 操作 |
| `Renderer` | `src/ui/renderer.h/.cpp` | ncurses 渲染，使用 LineIndex 获取行内容 |
| `Screen` | `src/ui/screen.h/.cpp` | ncurses 窗口管理 |
| `App` | `src/app/app.h/.cpp` | 主循环，输入处理，协调各组件 |

## 需求详情

### 需求 1：O(N) 时间/空间复杂度

**问题**：旧实现中 `MMapFile::get_line(n)` 从头扫描到第 n 行，复杂度 O(N)。配合 `FilterChain::passes()` 对每行调用，rebuild 总复杂度 O(N²)。

**要求**：
- `get_line(n)` 必须 O(log N) 或 O(1)
- 全量过滤计算必须 O(N)
- 空间复杂度不超过 O(N)

**当前状态**：
- ✅ `LineIndex::get_offset(n)` 已实现 O(log N) 查找
- ✅ `Filter::compute_batch()` 使用 LineIndex，O(N) 全量计算
- ✅ `DisplayBuffer` 使用块索引位图，rank/select O(blocks) ≈ O(N/BLOCK_BITS)

### 需求 2：显示与过滤分离线程

**问题**：旧实现中过滤计算在主线程同步执行，阻塞 UI。

**要求**：
- 过滤计算在后台线程完成
- 支持多线程并行计算不同过滤器
- 主线程只负责 UI 渲染，不阻塞

**当前状态**：
- ✅ `AsyncFilterRunner` 已实现
- ✅ 每个过滤器独立线程 (`std::async`)
- ✅ 合并线程在所有过滤器完成后 AND 所有位图
- ⚠️ 合并线程等待所有过滤器完成才开始 AND，不够渐进

### 需求 3：每过滤器独立 bit array

**问题**：旧实现只有一个合并后的位图，无法重用单个过滤器结果。

**要求**：
- 每个过滤器保存独立的 `std::vector<uint64_t>` 位图
- 最终结果是所有位图的 AND
- 修改一个过滤器时，只重算该过滤器，其他重用

**当前状态**：
- ✅ `FilterBitmap` 类已实现
- ✅ `AsyncFilterRunner` 存储 `per_filter_bitmaps_`
- ✅ 合并时 AND 所有 per-filter bitmaps
- ❌ 修改过滤器时的增量重用未实现（每次全量重算所有过滤器）

### 需求 4：显示优先策略（最关键的未完成需求）

**问题**：当前添加过滤器后，`display_buf_.clear()` 立即清零所有位，但异步计算需要时间。在此期间 UI 显示全空（全是 `~`）。

**要求**：
- 每次修改过滤器列表，立即重算当前可见区域的行（约 20 行）
- 计算完可见区域后立即刷新显示
- 后台继续计算剩余行
- 如果用户滚动到未计算区域，显示之前状态或提示计算中

**当前状态**：
- ❌ 未实现
- 当前流程：`display_buf_.clear()` → `async_filter_.start()` → UI 全空 → 等异步完成
- 目标流程：计算可见区域（同步）→ 刷新显示 → 计算剩余区域（异步）→ 逐步更新

### 需求 5：重用正则过滤结果

**问题**：修改过滤器列表时（如插入一个新过滤器），前面已有的过滤器结果应该可以重用。

**要求**：
- 添加/删除/移动过滤器时，未改变的过滤器位图不重新计算
- 只有新增或修改的过滤器需要重新计算
- 必要时（如文件内容变化）才重算全部

**当前状态**：
- ❌ 未实现
- `AsyncFilterRunner::start()` 每次都重新分配所有 per-filter bitmaps

## 已知 Bug

### Bug 1：添加过滤器后显示 0 行（P0 - 阻塞）

**现象**：执行 `:s ERROR` 后，状态栏显示 "0/100000 | 1 filter"，内容区域全为 `~`。

**根因**：
1. 所有 filter 变更路径（`handle_command`, `handle_filter_panel`, `handle_filter_insert`）都调用 `display_buf_.clear(); rebuild_cache();`
2. `clear()` 立即将所有位设为 0
3. `rebuild_cache()` → `async_filter_.start()` 启动后台线程，设置 `computing_ = true`
4. `refresh_display()` 检测到 `is_computing() == true`，跳过分步同步
5. 渲染时 `display_buf_` 全空 → 全 `~`

**修复方案**（显示优先）：
1. 在 `App::rebuild_cache()` 或 filter handler 中，先同步计算当前可见区域（scroll_top_ 到 scroll_top_ + 可见行数）
2. 将可见区域的过滤结果立即写入 `display_buf_`
3. 调用 `refresh_display()` 刷新
4. 再启动 `async_filter_.start()` 计算剩余区域（跳过已计算的可见区域）
5. `refresh_display()` 中，当 `is_computing() == true` 但有部分 combined bitmap 数据时，同步已完成的部分

**涉及文件**：
- `src/app/app.cpp` — `rebuild_cache()`, filter handlers
- `src/display/async_filter.h/.cpp` — 需要支持从指定行开始计算
- `src/app/app.h` — 可能需要新增成员

### Bug 2：清除过滤器 (`:nofilter`) 同样显示 0 行

**现象**：执行 `:nofilter` 后，同样显示全空。

**根因**：同 Bug 1 — `display_buf_.clear()` → `rebuild_cache()` → `async_filter_.start()`。但当 `chain_.size() == 0` 时，`start()` 会立即设置 combined 为全 1 并返回（同步路径）。不过 `display_buf_` 仍然被 clear 了。

**修复方案**：`:nofilter` 不需要走异步路径，直接同步设置所有行为可见。

## 开发计划（建议顺序）

### Phase 1：修复显示阻塞（P0）

**目标**：添加过滤器后立即显示可见区域内容，不再全空。

**步骤**：

1. **修改 `AsyncFilterRunner`**：
   - `start()` 接受 `skip_lines` 参数，跳过前 N 行的计算
   - 或者新增 `compute_range(start, count)` 方法
   - 支持渐进式更新：`computed_up_to_` 在计算过程中持续更新

2. **修改 `App::rebuild_cache()`**：
   - 计算当前可见区域范围：`start = scroll_top_` 对应的物理行, `count = 可见行数`
   - 同步计算可见区域的过滤结果（单线程，快速）
   - 将结果写入 `display_buf_`（只更新可见区域的 bits）
   - 启动 `async_filter_.start()` 计算剩余区域

3. **修改 `refresh_display()`**：
   - 当 `is_computing() == true` 且 combined bitmap 有部分数据时，同步可用部分
   - 使用 `set_from_bitmap()` 只更新已计算区域，保留未计算区域的旧状态

4. **修复 `:nofilter`**：
   - 不经过异步路径，直接同步设置所有行为可见

### Phase 2：增量过滤器重用（P1）

**目标**：添加新过滤器时，不重算已有过滤器。

**步骤**：

1. 在 `AsyncFilterRunner` 中保存过滤器的唯一标识（如 pattern + mode 的 hash）
2. 当过滤器列表变更时，对比新旧列表
3. 只重算新增/修改的过滤器
4. 重用的过滤器直接复制旧位图

### Phase 3：渐进式合并（P1）

**目标**：不再等待所有过滤器完成才合并，而是渐进更新 combined bitmap。

**步骤**：

1. 移除单独的 combiner 线程
2. 在每个过滤器线程完成后，将其位图 AND 到 combined bitmap
3. 更新 `computed_up_to_` 标记
4. 主线程可以随时读取 combined bitmap 的部分结果

### Phase 4：滚动到未计算区域的处理（P2）

**目标**：用户滚动到未计算区域时，体验平滑。

**步骤**：

1. 检测滚动目标是否在已计算区域内
2. 如果不在，可以：同步计算目标区域 → 滚动
3. 或者：显示旧状态，后台计算完成后再更新

## 关键代码路径

### 添加过滤器的调用链（当前，有 Bug）

```
handle_command() / handle_filter_panel()
  → filter_chain_.add(...)
  → display_buf_.clear()          ← 立即清空显示
  → rebuild_cache()
    → async_filter_.start(...)    ← 启动后台，computing = true
  → refresh_display()
    → is_computing() == true      ← 跳过分步同步
    → renderer_->render(...)      ← 渲染空 buffer → 全 ~
```

### 添加过滤器的调用链（目标）

```
handle_command() / handle_filter_panel()
  → filter_chain_.add(...)
  → rebuild_cache_with_priority()  ← 新函数
    1. 同步计算可见区域 → 更新 display_buf_ 可见部分
    2. 启动 async_filter_.start(skip_visible) → 后台计算剩余
  → refresh_display()
    → sync_partial_if_available() ← 同步可见区域的过滤结果
    → renderer_->render(...)      ← 显示可见区域内容 ✅
  → 后台计算完成
  → refresh_display() (下一帧)
    → sync_full_if_done()         ← 同步完整结果
    → renderer_->render(...)      ← 显示完整过滤结果 ✅
```

## 内存估算

- 100K 行 × 1 bit/行 = 12.5 KB per bitmap
- 3 个过滤器 = 37.5 KB per-filter bitmaps
- DisplayBuffer ≈ 12.5 KB
- 总计 < 100 KB，可忽略

## 文件清单

### 已实现（需要修改）
- `src/app/app.h` — 添加新方法声明
- `src/app/app.cpp` — 修复 filter 变更流程
- `src/display/async_filter.h` — 支持 skip_lines 参数
- `src/display/async_filter.cpp` — 实现 skip_lines + 渐进更新
- `src/display/display_buffer.h/.cpp` — 可能需要部分更新方法

### 已完成（不需修改）
- `src/core/mmap_file.h/.cpp`
- `src/core/line_index.h/.cpp`
- `src/core/types.h`
- `src/filter/filter.h/.cpp`
- `src/filter/regex_filter.h/.cpp`
- `src/filter/filter_chain.h/.cpp`
- `src/filter/filter_parser.h/.cpp`
- `src/filter/filter_io.h/.cpp`
- `src/display/filter_bitmap.h/.cpp`
- `src/display/display_buffer.h/.cpp` (set_from_bitmap 已实现)
- `src/ui/renderer.h/.cpp`
- `src/ui/screen.h/.cpp`
- `src/ui/status_bar.h/.cpp`
- `src/ui/filter_panel.h/.cpp`
- `CMakeLists.txt`
