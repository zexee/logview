# AGENTS.md

C++20/ncurses TUI log viewer for large files with rule-based filtering.

## Build & Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The TUI library is fetched automatically by CMake:
- **Linux**: ncurses 6.5 (widec) via `ExternalProject_Add` — no system ncurses needed.
- **Windows (MSVC)**: PDCursesMod via `FetchContent` (`PDC_WIDE=Y PDC_UTF8=Y`); vcpkg supplies Boost.Regex via `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`.

Boost.Regex is the only external runtime dependency (statically linked). `compile_commands.json` is exported for clangd. MSVC builds add `/utf-8 /W4 /permissive-`.

**No linter, formatter, typecheck** is configured. CI is GitHub Actions matrix (Linux + Windows) — see `.github/workflows/ci.yml`.

Run individual tests:

```bash
./build/test_filter_engine                              # core unit tests
./build/test_line_editor                                # line editor unit tests
./build/perf_filter_engine <log> <rules>                # benchmark
./build/lv test_log.txt tests/perf_rules.txt            # smoke run
```

## Architecture

CMake targets (see `CMakeLists.txt`):
- **`lv_core`** — `src/core/` — filtering, line indexing, mmap file, bit arrays, `path_util`. Must **not** depend on ncurses. Cross-platform: `mmap_file.cpp` uses `#ifdef _WIN32` to switch between POSIX `mmap` and Win32 `CreateFileMappingW`/`MapViewOfFile`.
- **`lv_ui`** — `src/ui/` — ncurses/PDCursesMod TUI: windows, line editor (no longer uses `formw`), input dispatch, background filter + search jobs.
- **`lv`** — `src/main.cpp` — wires core + ui together via `AppUi`. Entry point is `main` on Linux and `wmain` on Windows (wide argv → UTF-8 → shared `run(argc, argv)`).
- `test_filter_engine`, `test_line_editor`, `perf_filter_engine` — under `tests/`.

`src/input/` is empty and unused.

`docs/requirements.md` (Chinese) is the authoritative architecture handoff doc. `README.md` is the user-facing reference.

## Rule Syntax

Operators (see `src/core/rule_set.cpp:19` `parse_rule_operator`):

| Token | Meaning |
|-------|---------|
| `s` / `h` | show / hide |
| `si` / `hi` | case-insensitive show / hide |
| `sl N [M]` / `hl N [M]` | show / hide by 1-based line range; negative `N` counts from end; omit `M` to mean "to end" |

Pattern after the operator:
- Bare text → literal substring match.
- `/regex/` → Boost.Regex (ECMAScript). Mixed `/regex/|literal` OR is allowed (`|` separates segments).
- Quote a segment with `"..."` or `'...'` to preserve leading/trailing whitespace.
- `-` prefix on the operator disables the rule (e.g. `-s foo`).

`ss`/`hh` mentioned in `docs/requirements.md` are **not** implemented — use `s`/`h` with `/regex/` or bare literal instead.

Rules form a per-line pipeline: each line runs through all rules sequentially before moving to the next line. A rule only processes lines whose prior rule bit was `1`. A `1` bit means "passes this layer"; a `0` stops further processing of that line.

## Commands

Aliases (see `src/ui/app_ui.cpp:653` `handle_command`):

| Command | Aliases | Notes |
|---------|---------|-------|
| `:open [file]` | `:o`, `:e` | no arg reloads current file |
| `:write <file>` | `:w`, `:save-filtered` | saves final-bitmap-visible lines; refuses to overwrite source path |
| `:rules <file>` | `:r` | |
| `:write-rules [file]` | `:wr` | no arg writes back to current rule file |
| `:quit` | `:q` | |
| `:help` | `:h` | help popup (also `?`) |

All file paths support `~` expansion.

## Keybindings (Quick Reference)

Full tables in `README.md`:

- **Log window**: `j/k/Up/Down` navigate, `g/G/Home/End` jump, `PageUp/PageDown/Ctrl-B/Ctrl-F` page, `Space` toggle rules window, `/` incremental regex search (async, background), `n`/`N` next/prev match, `?` help, `Tab` switch focus.
- **Rules window**: same nav, `Enter` edit, `a/i/A/I` insert, `x`/`d` delete, `[`/`]` move, `-` toggle enabled, `Space`/`Esc` hide.
- **Global**: `:` command mode, `q` / `Ctrl-D` quit.

## Hard Constraints

- **core vs ui separation**: `lv_core` must never include ncurses/PDCursesMod headers. Keep it that way.
- **`LineIndex` holds mmap pointer**: changing the file (via `:open`) requires rebuilding `LineIndex`. Background filter threads receive a `LineIndex` snapshot to avoid dangling pointers.
- **`std::string_view` for all file data**: never copy lines. `LineIndex` returns views into the mmap'd region.
- **`active_filter_` owns the bitmap lifetime; `filter_bitmap_` is just a pointer into it** — don't let `filter_bitmap_` outlive the `FilterResult`.
- **`dirty_` flag**: the UI only redraws when dirty. Avoid calling refresh in a tight loop.
- **Path encoding**: file paths flow as UTF-8 `std::string` everywhere; `path_util::expand_path` returns `std::filesystem::path` and `path_util::to_utf8` renders back to UTF-8 bytes. `MMapFile::open` accepts UTF-8 on both platforms and converts to UTF-16 internally on Windows.
- **`mvwaddnstr` byte budget**: pass byte counts computed via `utf8_byte_at_column`, not raw column counts — PDCursesMod and ncurses disagree on whether `n` is bytes or characters, and Chinese paths can split a codepoint otherwise.

## Background Filtering & Search

Filters run on a background thread via `AppUi::start_filter_job()`:
- Copies a `LineIndex` + `RuleSet` snapshot and runs `FilterEngine`.
- No cancellation; old jobs are ignored via a **generation counter**.
- `:open` calls `join_filter_jobs()` before replacing mmap to avoid dangling data.
- `:save-filtered` waits for the latest job (`wait_for_filter_jobs()`) before writing output.

Incremental search (`/`) uses a separate background thread (`incsearch_thread_` / `search_thread_`) with its own generation counter and cancellation guard (`cancel_search_job`). It builds a `search_matches_` bitmap and is independent of the filter pipeline.

## Known Limitations (don't try to fix these without asking)

- Regex match highlighting for filter rules is not implemented (only literal rule highlighting and search-term highlighting work).
- Scrolling is per visible file line, not per wrapped screen line.
- No incremental filter reuse; changing one rule recomputes everything.
- No prefix/suffix/contains operator — only literal, regex (`/.../`), and line range.
- `LineEditor` is hand-written (no `formw`/`readline/libedit`); UTF-8 multi-byte input is assembled byte-by-byte from successive `getch()` results.
- No pty/screenshot end-to-end tests exist; tests are unit tests against `lv_core` and `lv_ui::LineEditor` only.
