cmake_minimum_required(VERSION 3.28)
project(load-monitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──
option(BUILD_TESTS "Build tests" ON)
option(LOADMON_USE_TUI "Build with FTXUI TUI support" ON)
option(LOADMON_USE_JSON "Build with nlohmann_json support" ON)

# ── Dependencies ──
include(FetchContent)

# nlohmann_json (for JSON output mode)
if(LOADMON_USE_JSON)
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# FTXUI (for TUI mode)
if(LOADMON_USE_TUI)
  FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
    SYSTEM
  )
  FetchContent_MakeAvailable(ftxui)
endif()

# spdlog (header-only logging)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
)
FetchContent_MakeAvailable(spdlog)

# ── Library (all source except main) ──
file(GLOB_RECURSE LIB_SRCS src/*.cpp)
list(FILTER LIB_SRCS EXCLUDE REGEX "main\\.cpp$")
file(GLOB_RECURSE HEADERS include/*.hpp)

add_library(loadmon-lib STATIC ${LIB_SRCS} ${HEADERS})
target_include_directories(loadmon-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(loadmon-lib PUBLIC spdlog::spdlog)

if(LOADMON_USE_JSON)
  target_link_libraries(loadmon-lib PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(loadmon-lib PUBLIC LOADMON_USE_JSON)
endif()

# ── Executable ──
add_executable(load-monitor src/main.cpp)
target_include_directories(load-monitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(load-monitor PRIVATE loadmon-lib)

if(LOADMON_USE_TUI)
  target_link_libraries(load-monitor PRIVATE ftxui::screen ftxui::dom ftxui::component)
  target_compile_definitions(load-monitor PRIVATE LOADMON_USE_TUI)
endif()

if(LOADMON_USE_JSON)
  target_link_libraries(load-monitor PRIVATE nlohmann_json::nlohmann_json)
endif()

# ── Tests ──
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ──
install(TARGETS load-monitor DESTINATION bin)
