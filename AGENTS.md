# AGENTS.md

C++20/ncurses TUI log viewer for large files with rule-based filtering.

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**No linter, formatter, typecheck, or CI** is configured in this repo.

Run individual tests:

```bash
./build/test_filter_engine          # unit tests for core
./build/test_line_editor            # unit tests for line editor
./build/perf_filter_engine <log> <rules>  # performance benchmark
```

## Architecture

Three CMake targets (see `CMakeLists.txt`):
- **`lv_core`** — `src/core/` — filtering, line indexing, mmap file, bit arrays. Must **not** depend on ncurses.
- **`lv_ui`** — `src/ui/` — ncurses TUI: windows, line editor (formw), input dispatch, background filter jobs.
- **`lv`** — `src/main.cpp` — wires core + ui together via `AppUi`.

`src/input/` is empty (unused).

Full architecture and dev notes are in `docs/requirements.md`.

## Hard Constraints

- **core vs ui separation**: `lv_core` must never include ncurses headers. Keep it that way.
- **`LineIndex` holds mmap pointer**: changing the file (via `:open`) requires rebuilding `LineIndex`. The current code copies `LineIndex` snapshot into background filter threads to avoid dangling pointers.
- **`std::string_view` for all file data**: never copy lines. `LineIndex` returns views into the mmap'd region.
- **`active_filter_` owns the bitmap lifetime; `filter_bitmap_` is just a pointer into it** — don't let `filter_bitmap_` outlive the `FilterResult`.
- **`dirty_` flag**: the UI only redraws when dirty. Avoid calling refresh in a tight loop.

## Background Filtering

Filters run on a background thread via `AppUi::start_filter_job()`:
- Copies a `LineIndex` + `RuleSet` snapshot and runs `FilterEngine`.
- No cancellation; old jobs are ignored via a **generation counter**.
- `:open` calls `join_filter_jobs()` before replacing mmap to avoid dangling data.
- `:save-filtered` waits for the latest job before writing output.

## Rules & Pipeline

Rules are line-based (`s`/`h` for regex, `ss`/`hh` for literal). Pipeline is per-line (not per-layer): each line runs through all rules sequentially before moving to the next line. A rule only processes lines whose prior rule bit was `1`.

## Known Limitations (don't try to fix these without asking)

- Regex match highlighting is not implemented (only literal highlighting works).
- Scrolling is per visible file line, not per wrapped screen line.
- No incremental filter reuse; changing one rule recomputes everything.
- No case-insensitive rule flag, no prefix/suffix syntax.
- `LineEditor` uses `formw` intentionally (not readline/libedit) because it embeds in the ncurses window.
- No pty/screenshot end-to-end tests exist.

## Keybinding Reference

Quick lookup for development — full table in `docs/requirements.md`:
- **Log window**: `j/k/Up/Down` navigate, `g/G/Home/End` jump, `PageUp/PageDown/Ctrl-B/Ctrl-F` page.
- **Rules window**: same nav, `Enter` edit, `a/i/A/I` insert rule, `x` delete, `[`/`]` move.
- **Global**: `Tab` switch focus, `:` command mode, `q` quit.
- **Commands**: `:open`, `:rules`, `:write-rules`, `:save-filtered`, `:quit`/`:q`.
