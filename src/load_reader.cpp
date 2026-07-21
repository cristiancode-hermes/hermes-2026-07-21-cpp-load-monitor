#include "loadmon/load_reader.hpp"
#include <fstream>
#include <sstream>
#include <charconv>
#include <chrono>
#include <ctime>

namespace loadmon {

LoadReader::LoadReader() = default;

// ── Parse /proc/loadavg ──
Result<LoadAvg> LoadReader::read_loadavg() const {
  std::ifstream f("/proc/loadavg");
  if (!f.is_open())
    return std::unexpected(Error{500, "Cannot open /proc/loadavg"});

  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  return parse_loadavg(content);
}

Result<LoadAvg> LoadReader::parse_loadavg(const std::string& content) {
  LoadAvg la{};
  // Format: "0.00 0.01 0.02 1/456 7890"
  std::istringstream ss(content);
  std::string procs_str;

  ss >> la.load_1 >> la.load_5 >> la.load_15 >> procs_str >> la.last_pid;

  if (ss.fail())
    return std::unexpected(Error{500, "Failed to parse /proc/loadavg"});

  auto slash_pos = procs_str.find('/');
  if (slash_pos != std::string::npos) {
    auto r = std::from_chars(procs_str.data(), procs_str.data() + slash_pos, la.running_procs);
    auto t = std::from_chars(procs_str.data() + slash_pos + 1,
                             procs_str.data() + procs_str.size(), la.total_procs);
    if (r.ec != std::errc{} || t.ec != std::errc{})
      return std::unexpected(Error{500, "Failed to parse process counts"});
  }

  return la;
}

// ── Parse /proc/stat ──
Result<SystemStat> LoadReader::read_stat() const {
  std::ifstream f("/proc/stat");
  if (!f.is_open())
    return std::unexpected(Error{500, "Cannot open /proc/stat"});

  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  return parse_stat(content);
}

Result<SystemStat> LoadReader::parse_stat(const std::string& content) {
  SystemStat stat{};
  std::istringstream ss(content);
  std::string line;

  while (std::getline(ss, line)) {
    if (line.starts_with("cpu")) {
      CpuCoreStat core{};
      std::istringstream ls(line);
      std::string label;
      ls >> label;
      core.name = label;

      uint64_t vals[8]{};
      int idx = 0;
      uint64_t v;
      while (idx < 8 && ls >> v) {
        vals[idx++] = v;
      }

      if (idx >= 1) core.user = vals[0];
      if (idx >= 2) core.nice = vals[1];
      if (idx >= 3) core.system = vals[2];
      if (idx >= 4) core.idle = vals[3];
      if (idx >= 5) core.iowait = vals[4];
      if (idx >= 6) core.irq = vals[5];
      if (idx >= 7) core.softirq = vals[6];
      if (idx >= 8) core.steal = vals[7];

      if (label == "cpu") {
        stat.cpu_total = core;
      } else {
        stat.cores.push_back(std::move(core));
      }
    } else if (line.starts_with("ctxt ")) {
      std::from_chars(line.data() + 5, line.data() + line.size(), stat.ctxt);
    } else if (line.starts_with("processes ")) {
      std::from_chars(line.data() + 10, line.data() + line.size(), stat.processes);
    } else if (line.starts_with("procs_running ")) {
      std::from_chars(line.data() + 14, line.data() + line.size(), stat.procs_running);
    } else if (line.starts_with("procs_blocked ")) {
      std::from_chars(line.data() + 14, line.data() + line.size(), stat.procs_blocked);
    } else if (line.starts_with("btime ")) {
      std::from_chars(line.data() + 6, line.data() + line.size(), stat.btime);
    }
  }

  return stat;
}

std::chrono::seconds LoadReader::uptime_seconds(uint64_t btime) {
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  auto uptime = static_cast<uint64_t>(now) - btime;
  return std::chrono::seconds(uptime);
}

} // namespace loadmon
