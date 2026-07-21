#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace loadmon {

// ── Load Averages (from /proc/loadavg) ──
struct LoadAvg {
  double load_1;
  double load_5;
  double load_15;
  int running_procs;
  int total_procs;
  int last_pid;
};

// ── Per-core CPU stats (from /proc/stat) ──
struct CpuCoreStat {
  std::string name; // "cpu0", "cpu1", ...
  uint64_t user;
  uint64_t nice;
  uint64_t system;
  uint64_t idle;
  uint64_t iowait;
  uint64_t irq;
  uint64_t softirq;
  uint64_t steal;

  [[nodiscard]] uint64_t total() const noexcept {
    return user + nice + system + idle + iowait + irq + softirq + steal;
  }

  [[nodiscard]] double utilization_pct(const CpuCoreStat& prev) const noexcept {
    auto delta_total = total() - prev.total();
    if (delta_total == 0) return 0.0;
    auto delta_idle = idle - prev.idle;
    return 100.0 * (1.0 - static_cast<double>(delta_idle) / delta_total);
  }
};

// ── System-wide /proc/stat summary ──
struct SystemStat {
  std::vector<CpuCoreStat> cores;
  CpuCoreStat cpu_total; // "cpu" aggregate
  uint64_t ctxt;         // context switches
  uint64_t processes;    // processes created
  uint64_t procs_running;
  uint64_t procs_blocked;
  uint64_t btime;        // boot time (epoch seconds)
};

// ── Load Reader ──
class LoadReader {
public:
  LoadReader();
  ~LoadReader() = default;

  // Non-copyable
  LoadReader(const LoadReader&) = delete;
  LoadReader& operator=(const LoadReader&) = delete;
  LoadReader(LoadReader&&) noexcept = default;
  LoadReader& operator=(LoadReader&&) noexcept = default;

  Result<LoadAvg> read_loadavg() const;
  Result<SystemStat> read_stat() const;

  // Convert boot time to readable uptime
  [[nodiscard]] static std::chrono::seconds uptime_seconds(uint64_t btime);

private:
  static Result<SystemStat> parse_stat(const std::string& content);
  static Result<LoadAvg> parse_loadavg(const std::string& content);
};

} // namespace loadmon
