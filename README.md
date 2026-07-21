# Load Monitor

Monitor de carga del sistema Linux en C++23 con TUI interactiva y salida JSON.

Lee estadísticas en tiempo real de `/proc/loadavg` y `/proc/stat` con visualización mediante **FTXUI** y barras de carga coloreadas.

## Características

- **Load Averages** (1/5/15 min) con barras de progreso coloreadas
- **Utilización por núcleo** CPU con delta entre lecturas
- **Estadísticas de procesos**: running, blocked, total creados
- **Context switches** del sistema
- **Uptime** del sistema
- **Modo TUI interactivo** con refresco periódico
- **Modo JSON** (`--json`) para integración con scripts
- **Modo CLI texto** (`--no-tui`) para terminales sin FTXUI

## Requisitos

- CMake 3.28+
- Compilador C++23 (GCC 14+, Clang 18+)
- Ninja (recomendado) o Make
- Opcional: FTXUI (se descarga automáticamente vía FetchContent)

## Compilación

```bash
# Debug con TUI (recomendado)
cmake --preset debug
cmake --build --preset debug

# CLI solo (sin FTXUI)
cmake --preset cli-only
cmake --build --preset cli-only
```

## Uso

```bash
# TUI interactiva (por defecto)
./build/debug/load-monitor

# Una salida JSON
./build/debug/load-monitor --json

# Salida CLI texto
./build/debug/load-monitor --no-tui

# Intervalo personalizado (ms)
./build/debug/load-monitor --interval 5000

# Ayuda
./build/debug/load-monitor --help
```

## Tests

```bash
cmake --build --preset debug
cd build/debug && ctest --output-on-failure
```

## Estructura

```
├── CMakeLists.txt          # Build principal (C++23, CMake 3.28+)
├── CMakePresets.json       # Debug (asan+ubsan), Release (LTO), CLI-only
├── Dockerfile              # Multi-stage build
├── include/loadmon/
│   ├── types.hpp           # Error + Result<T> con std::expected
│   └── load_reader.hpp     # Reader de /proc/loadavg y /proc/stat
├── src/
│   ├── main.cpp            # Entry point con CLI parser y TUI/CLI modos
│   └── load_reader.cpp     # Implementación del parser
├── tests/
│   ├── CMakeLists.txt      # Catch2 tests
│   └── test_main.cpp       # Unit, integration, benchmark
└── docs/
    └── architecture.md     # Diagrama de flujo y decisiones técnicas
```

## Tecnologías

- **C++23** — `std::expected`, `std::println`, `std::from_chars`
- **FTXUI** — Terminal UI interactiva
- **nlohmann_json** — Salida JSON (opcional)
- **spdlog** — Logging estructurado
- **Catch2** — Tests unitarios, integración y benchmarks
- **/proc** — Interfaz de estadísticas del kernel Linux

## Licencia

MIT
