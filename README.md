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

Requirements | 依赖: cmake ≥ 3.16, C++20 compiler, Boost.Regex (statically linked).

TUI library is fetched automatically:
- Linux: ncurses 6.5 (widec), built from source via CMake `ExternalProject`
- Windows: PDCursesMod (win32 console, wide/UTF-8), fetched via `FetchContent`

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSVC + vcpkg)

```powershell
vcpkg install boost-regex:x64-windows
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

PDCursesMod is downloaded and built automatically. `/utf-8` is enabled so the
source and runtime strings stay UTF-8; `wmain` + `SetConsoleOutputCP(CP_UTF8)`
make non-ASCII file paths work end-to-end.

### Optional GUI binary (lv-gui)

There is also a cross-platform GUI binary that hosts the same TUI inside an
SDL2 window with an OpenGL context, plus a Dear ImGui menu bar at the top for
the most common commands. The same `lv_core` / `lv_ui` source is compiled
against PDCursesMod's GL backend instead of ncurses.

```bash
# One-time fetch of vendored sources (SDL2, SDL2_ttf, imgui, PDCursesMod)
./scripts/bootstrap_third_party.sh
# Optional: --proxy http://your-proxy:port

cmake -B build_gui -DCMAKE_BUILD_TYPE=Release -DLV_BUILD_GUI=ON
cmake --build build_gui -j --target lv-gui
./build_gui/lv-gui <log_file> [rule_file]
```

Dependencies (vendored in `third_party/`, built as static libs):
- SDL2, SDL2_ttf (with vendored FreeType)
- Dear ImGui (SDL2 + OpenGL3 backends)
- PDCursesMod GL backend (with a one-line patch in `third_party_patches/`
  that adds a `pdc_no_swap` flag so ImGui can draw on top of the TUI)

Both binaries can coexist; `lv` is the TUI build, `lv-gui` is the windowed one.

## Run | 运行

```bash
./build/lv <log_file> [rule_file]          # Linux
.\build\Release\lv.exe <log_file> [rule_file]   # Windows
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
sl N [M]         show line N to M (default to end)
hl N [M]         hide line N to M (default to end)
```

Line range examples | 行范围示例:
```
sl 5             lines 5 to end
sl 5 10          lines 5 to 10
hl 3 7           hide lines 3 to 7
sl -3            last 3 lines
hl -2 -1         hide last 2 lines
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
| `Mouse wheel` | scroll 3 lines (focused window) |
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
