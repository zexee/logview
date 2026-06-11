# lv

**lv** is a fast C++20/ncurses TUI log viewer for large files with regex-based filtering.

**lv** 是一个基于 C++20/ncurses 的 TUI 大文件日志查看器，支持正则过滤。

---

## Features | 功能

- Open and browse multi-gigabyte log files instantly via mmap
- Rule-based filter pipeline (show/hide, literal/regex, OR segments)
- Background filter threads — UI stays responsive during filtering
- Incremental search (`/`) with regex, async background matching, `n`/`N` navigation
- Vim-like keybindings: `j`/`k`, `g`/`G`, `PgUp`/`PgDn`, `scrolloff=5`
- UTF-8 support with display-width-aware line wrapping
- `~` path expansion, `:w` save filtered output
- Help popup (`?`), filter window toggle (`Space`)

---

## Build | 编译

Requirements | 依赖: cmake, C++20 compiler, Boost.Regex (statically linked)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

ncurses is downloaded and built from source via `ExternalProject` — no system ncurses required.
ncurses 通过 ExternalProject 从源码下载编译，不需要系统安装 ncurses。

## Run | 运行

```bash
./build/lv <log_file> [rule_file]
```

## Tests | 测试

```bash
./build/test_filter_engine          # 119 unit tests (core)
./build/test_line_editor            # 17 unit tests (editor)
./build/perf_filter_engine <log> <rules>  # performance benchmark
```

## Rule Syntax | 规则语法

```
s  PATTERN       show lines matching PATTERN (literal)
h  PATTERN       hide lines matching PATTERN (literal)
s /PATTERN/      show lines matching regex PATTERN
h /PATTERN/      hide lines matching regex PATTERN
-s PATTERN       same as above but rule disabled
s A|B|C          OR: show lines matching A or B or C
s /[0-9]/|A      mixed regex + literal OR
```

Rules form a pipeline: each line passes through rules in order.

Rules file example | 规则文件示例：
```
s ERROR
h /DEBUG|TRACE/
- s auth
s /user=[0-9]+/
```

## Keybindings | 快捷键

### Log window | 日志窗口

| Key | Action |
|-----|--------|
| `j` / `Down` | next visible line |
| `k` / `Up` | previous visible line |
| `g` / `Home` | first visible line |
| `G` / `End` | last visible line |
| `PgDn` / `Ctrl-F` | page down |
| `PgUp` / `Ctrl-B` | page up |
| `Tab` | switch focus to rules |
| `Space` | show filters window (auto-focus) |
| `/` | incremental regex search |
| `n` / `N` | next / previous search match |
| `?` | help popup |

### Rules window | 过滤窗口

| Key | Action |
|-----|--------|
| `j` / `k` / `Up` / `Down` | navigate rules |
| `Enter` | edit rule |
| `a` / `i` / `A` / `I` | insert rule after / before / end / start |
| `x` / `d` | delete rule |
| `[` / `]` | move rule up / down |
| `-` | toggle rule enabled / disabled |
| `Space` / `Esc` | hide rules window |

### Global | 全局

| Key | Action |
|-----|--------|
| `:` | command mode |
| `Ctrl-D` / `:q` | quit |

## Commands | 命令

| Command | Alias | Description |
|---------|-------|-------------|
| `:o [file]` | `:open` | open log file (`:o` alone reloads) |
| `:w <file>` | `:write` | save filtered lines (refuses to overwrite source) |
| `:r <file>` | `:rules` | load rules file |
| `:wr [file]` | `:write-rules` | save rules file |
| `:q` | `:quit` | quit |

All file paths support `~` expansion.

## License | 许可

MIT
