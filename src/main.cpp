#include "loadmon/load_reader.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <vector>

#ifdef LOADMON_USE_TUI
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#endif

using namespace loadmon;
using namespace std::chrono_literals;

// ── CLI flags ──
struct CliOptions {
  bool tui_mode = true;
  bool json_mode = false;
  int interval_ms = 2000;
};

static std::atomic<bool> g_running{true};

void handle_signal(int) {
  g_running = false;
}

// ── Formatters ──
static std::string fmt_duration(std::chrono::seconds s) {
  auto d = s.count();
  int days = static_cast<int>(d / 86400);
  int hours = static_cast<int>((d % 86400) / 3600);
  int mins = static_cast<int>((d % 3600) / 60);
  int secs = static_cast<int>(d % 60);
  char buf[32];
  if (days > 0)
    std::snprintf(buf, sizeof(buf), "%dd %02d:%02d:%02d", days, hours, mins, secs);
  else
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, mins, secs);
  return buf;
}

static std::string fmt_pct(double pct) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%5.1f%%", pct);
  return buf;
}

static std::string fmt_bar(double pct, int width = 20) {
  int filled = std::clamp(static_cast<int>(std::round(pct / 100.0 * width)), 0, width);
  return std::string(filled, '|') + std::string(width - filled, '.');
}

// ── JSON output ──
static void print_json(const LoadAvg& la, const SystemStat& ss) {
  auto uptime = LoadReader::uptime_seconds(ss.btime);
  // Use snprintf for safe formatting
  std::cout << "{\n";
  std::cout << "  \"loadavg\": {\n";
  std::cout << "    \"load_1\": " << la.load_1 << ",\n";
  std::cout << "    \"load_5\": " << la.load_5 << ",\n";
  std::cout << "    \"load_15\": " << la.load_15 << ",\n";
  std::cout << "    \"running_procs\": " << la.running_procs << ",\n";
  std::cout << "    \"total_procs\": " << la.total_procs << "\n";
  std::cout << "  },\n";
  std::cout << "  \"system\": {\n";
  std::cout << "    \"uptime_seconds\": " << uptime.count() << ",\n";
  std::cout << "    \"context_switches\": " << ss.ctxt << ",\n";
  std::cout << "    \"processes_total\": " << ss.processes << ",\n";
  std::cout << "    \"procs_running\": " << ss.procs_running << ",\n";
  std::cout << "    \"procs_blocked\": " << ss.procs_blocked << ",\n";
  std::cout << "    \"cores\": " << ss.cores.size() << "\n";
  std::cout << "  }\n";
  std::cout << "}\n";
}

// ── TUI mode ──
#ifdef LOADMON_USE_TUI
static void run_tui(CliOptions opts) {
  using namespace ftxui;

  auto screen = ScreenInteractive::Fullscreen();
  LoadReader reader;

  // Shared state
  struct {
    LoadAvg la{};
    SystemStat prev_stat{};
    SystemStat curr_stat{};
    bool has_data = false;
  } state;

  // Initial read
  {
    auto la_res = reader.read_loadavg();
    auto stat_res = reader.read_stat();
    if (la_res && stat_res) {
      state.la = *la_res;
      state.prev_stat = *stat_res;
      std::this_thread::sleep_for(100ms);
      auto stat2 = reader.read_stat();
      if (stat2) state.curr_stat = *stat2;
      state.has_data = true;
    }
  }

  // Renderer
  auto renderer = Renderer([&] {
    if (!state.has_data) {
      return text(" Loading system data... ") | bold | center;
    }

    Elements lines;

    // ── Header ──
    auto uptime = LoadReader::uptime_seconds(state.curr_stat.btime);
    lines.push_back(text(" System Load Monitor ") | bold | color(Color::Cyan));
    lines.push_back(text(" Uptime: " + fmt_duration(uptime)) | color(Color::GrayDark));
    lines.push_back(separator());

    // ── Load Averages ──
    double ncpu = static_cast<double>(state.curr_stat.cores.size());
    double pct_1 = (ncpu > 0) ? std::min(100.0, state.la.load_1 / ncpu * 100.0) : 0.0;
    double pct_5 = (ncpu > 0) ? std::min(100.0, state.la.load_5 / ncpu * 100.0) : 0.0;
    double pct_15 = (ncpu > 0) ? std::min(100.0, state.la.load_15 / ncpu * 100.0) : 0.0;

    auto load_color = [](double pct) -> Decorator {
      return (pct > 80.0) ? color(Color::Red) :
             (pct > 50.0) ? color(Color::Yellow) : color(Color::Green);
    };

    lines.push_back(text(" Load Averages:"));
    lines.push_back(hbox(Elements{
      text("   1 min: ") | size(WIDTH, GREATER_THAN, 12),
      text(fmt_bar(pct_1)) | load_color(pct_1),
      text(" " + fmt_pct(state.la.load_1)),
    }));
    lines.push_back(hbox(Elements{
      text("   5 min: ") | size(WIDTH, GREATER_THAN, 12),
      text(fmt_bar(pct_5)) | load_color(pct_5),
      text(" " + fmt_pct(state.la.load_5)),
    }));
    lines.push_back(hbox(Elements{
      text("  15 min: ") | size(WIDTH, GREATER_THAN, 12),
      text(fmt_bar(pct_15)) | load_color(pct_15),
      text(" " + fmt_pct(state.la.load_15)),
    }));

    lines.push_back(separator());

    // ── Process Info ──
    lines.push_back(text(" Processes:"));
    lines.push_back(text("   Running: " + std::to_string(state.la.running_procs) +
                         " / Total: " + std::to_string(state.la.total_procs)));
    lines.push_back(text("   Procs running: " + std::to_string(state.curr_stat.procs_running) +
                         " | Blocked: " + std::to_string(state.curr_stat.procs_blocked)));
    lines.push_back(text("   Total created: " + std::to_string(state.curr_stat.processes)));

    lines.push_back(separator());

    // ── Per-core Utilization ──
    lines.push_back(text(" Per-Core Utilization:"));
    for (size_t i = 0; i < state.curr_stat.cores.size(); ++i) {
      const auto& cur = state.curr_stat.cores[i];
      const auto& prev = (i < state.prev_stat.cores.size())
                           ? state.prev_stat.cores[i] : cur;
      double pct = cur.utilization_pct(prev);

      char core_label[16];
      std::snprintf(core_label, sizeof(core_label), "  %-4s", cur.name.c_str());

      lines.push_back(hbox(Elements{
        text(core_label) | size(WIDTH, GREATER_THAN, 8),
        text(fmt_bar(pct)) | load_color(pct),
        text(" " + fmt_pct(pct)),
      }));
    }

    lines.push_back(separator());

    // ── Context Switches ──
    lines.push_back(text(" Context switches: " + std::to_string(state.curr_stat.ctxt)));

    lines.push_back(separator());
    lines.push_back(text(" Ctrl+C to exit") | color(Color::GrayDark));

    return window(text(" Load Monitor ") | bold | color(Color::Cyan),
                  vbox(std::move(lines))) | border;
  });

  // Background refresh thread
  std::thread refresh_thread([&] {
    while (g_running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(opts.interval_ms));

      LoadReader fresh_reader;
      auto la_res = fresh_reader.read_loadavg();
      auto stat_res = fresh_reader.read_stat();
      if (la_res && stat_res) {
        state.prev_stat = state.curr_stat;
        state.la = *la_res;
        state.curr_stat = *stat_res;
        state.has_data = true;
        screen.PostEvent(Event::Custom);
      }
    }
  });

  screen.Loop(renderer);
  g_running = false;
  if (refresh_thread.joinable()) refresh_thread.join();
}
#endif

// ── CLI mode (text output, one-shot) ──
static void run_cli_once(CliOptions opts) {
  LoadReader reader;

  auto la_res = reader.read_loadavg();
  auto stat_res = reader.read_stat();

  if (!la_res) {
    std::cerr << "Error reading loadavg: " << la_res.error().message << std::endl;
    return;
  }
  if (!stat_res) {
    std::cerr << "Error reading stat: " << stat_res.error().message << std::endl;
    return;
  }

  if (opts.json_mode) {
    print_json(*la_res, *stat_res);
    return;
  }

  const auto& la = *la_res;
  const auto& ss = *stat_res;
  auto uptime = LoadReader::uptime_seconds(ss.btime);
  double ncpu = static_cast<double>(ss.cores.size());

  std::println(std::cout, "╔══════════════════════════════════════╗");
  std::println(std::cout, "║     System Load Monitor v1.0        ║");
  std::println(std::cout, "╚══════════════════════════════════════╝");
  std::println(std::cout, "");
  std::println(std::cout, "  Uptime:  {}", fmt_duration(uptime));
  std::println(std::cout, "  Cores:   {}", ss.cores.size());
  std::println(std::cout, "");
  std::println(std::cout, "  ── Load Averages ──");
  std::println(std::cout, "    1 min:  {:.2f}  [{:3.0f}%]", la.load_1, std::min(100.0, la.load_1 / ncpu * 100.0));
  std::println(std::cout, "    5 min:  {:.2f}  [{:3.0f}%]", la.load_5, std::min(100.0, la.load_5 / ncpu * 100.0));
  std::println(std::cout, "    15 min: {:.2f}  [{:3.0f}%]", la.load_15, std::min(100.0, la.load_15 / ncpu * 100.0));
  std::println(std::cout, "");
  std::println(std::cout, "  ── Processes ──");
  std::println(std::cout, "    Running: {} / {}", la.running_procs, la.total_procs);
  std::println(std::cout, "    Blocked: {}  |  Created: {}", ss.procs_blocked, ss.processes);
  std::println(std::cout, "");
  std::println(std::cout, "  ── System ──");
  std::println(std::cout, "    Context switches: {}", ss.ctxt);

  // Per-core in one-shot (no delta)
  if (!ss.cores.empty()) {
    std::println(std::cout, "");
    std::println(std::cout, "  ── Per-Core (total ticks) ──");
    for (const auto& core : ss.cores) {
      std::println(std::cout, "    {:4s}: user={:>8}  sys={:>8}  idle={:>8}  total={:>10}",
                   core.name, core.user, core.system, core.idle, core.total());
    }
  }
}

// ── Entry point ──
int main(int argc, char** argv) {
  CliOptions opts;

  // Simple CLI parser
  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg == "--json" || arg == "-j") {
      opts.json_mode = true;
      opts.tui_mode = false;
    } else if (arg == "--no-tui") {
      opts.tui_mode = false;
    } else if (arg == "--interval" && i + 1 < argc) {
      opts.interval_ms = std::stoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      std::println(std::cout, "Usage: load-monitor [--json|-j] [--no-tui] [--interval MS]");
      std::println(std::cout, "");
      std::println(std::cout, "  --json, -j       Output JSON (one-shot)");
      std::println(std::cout, "  --no-tui         CLI text output (one-shot)");
      std::println(std::cout, "  --interval MS    Refresh interval in ms (default: 2000)");
      std::println(std::cout, "  --help, -h       Show this help");
      return 0;
    }
  }

  // Register signal handler
  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  if (opts.tui_mode) {
#ifdef LOADMON_USE_TUI
    run_tui(opts);
#else
    std::cerr << "TUI mode not available (built without FTXUI). Use --no-tui or --json." << std::endl;
    run_cli_once(opts);
#endif
  } else {
    run_cli_once(opts);
  }

  return 0;
}
