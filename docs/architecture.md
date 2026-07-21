# Architecture

## Data Flow

```
/proc/loadavg ──→ LoadReader ──→ LoadAvg struct ──→ CLI/JSON formatter
/proc/stat    ──→ LoadReader ──→ SystemStat struct ──→ FTXUI TUI renderer
```

## Parsing Pipeline

```
read file → parse line → extract fields → validate → return Result<T>
                                                    ↓
                                           std::unexpected(Error{code, msg})
```

## TUI Refresh Cycle

```
std::thread (background)
  │
  ├── sleep(interval_ms)
  ├── read /proc/loadavg + /proc/stat
  ├── save prev_stat ← curr_stat
  ├── update curr_stat ← fresh data
  ├── screen.PostEvent(Event::Custom)
  │
  ▼
Renderer lambda (main thread)
  ├── reads shared state (captured by ref)
  └── rebuilds DOM tree → re-render
```

## Error Handling

- `std::expected<T, Error>` for all I/O operations
- Error struct has code (HTTP-style) + message
- No exceptions in data path
- Constructor throws only on fatal DB-like errors

## CLI Parser

Simple argv loop (no CLI11 dependency):
- `--json` / `-j` : JSON one-shot output
- `--no-tui` : CLI text output
- `--interval MS` : refresh interval
- `--help` / `-h` : usage
