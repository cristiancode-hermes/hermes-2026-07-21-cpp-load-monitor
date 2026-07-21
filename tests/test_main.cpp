#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "loadmon/load_reader.hpp"
#include "loadmon/types.hpp"

#include <fstream>

using namespace loadmon;

// ── Unit Tests ──

TEST_CASE("Parse /proc/loadavg", "[loadavg]") {
  SECTION("Read real /proc/loadavg") {
    LoadReader reader;
    auto result = reader.read_loadavg();
    REQUIRE(result.has_value());
    CHECK(result->load_1 >= 0.0);
    CHECK(result->load_5 >= 0.0);
    CHECK(result->load_15 >= 0.0);
    CHECK(result->running_procs >= 0);
    CHECK(result->total_procs > 0);
    CHECK(result->last_pid > 0);
  }
}

TEST_CASE("Parse /proc/stat - CPU cores via real file", "[stat][cpu]") {
  LoadReader reader;
  auto result = reader.read_stat();
  REQUIRE(result.has_value());
  CHECK(result->cores.size() >= 1);
  CHECK(result->cpu_total.total() > 0);
}

TEST_CASE("CpuCoreStat total calculation", "[stat][core]") {
  CpuCoreStat c;
  c.user = 1000;
  c.nice = 100;
  c.system = 500;
  c.idle = 8000;
  c.iowait = 50;
  c.irq = 10;
  c.softirq = 20;
  c.steal = 5;

  CHECK(c.total() == 1000 + 100 + 500 + 8000 + 50 + 10 + 20 + 5);
}

TEST_CASE("CpuCoreStat utilization_pct", "[stat][util]") {
  CpuCoreStat prev, curr;
  prev.user = 1000; prev.nice = 100; prev.system = 500; prev.idle = 8000;
  prev.iowait = 50; prev.irq = 10; prev.softirq = 20; prev.steal = 5;

  curr.user = 1100; curr.nice = 110; curr.system = 550; curr.idle = 8060;
  curr.iowait = 55; curr.irq = 11; curr.softirq = 22; curr.steal = 6;

  // delta_total = 1914 - 1685 = 229, delta_idle = 8060-8000 = 60
  // utilization = 100 * (1 - 60/229) = 100 * (1 - 0.262) = 73.8%
  double pct = curr.utilization_pct(prev);
  CHECK(pct > 73.0);
  CHECK(pct < 74.0);
}

TEST_CASE("CpuCoreStat utilization_pct - same values", "[stat][util]") {
  CpuCoreStat a{}, b{};
  // Both have the same values, so delta_total = 0
  a.user = 100; a.nice = 10; a.system = 50; a.idle = 1000;
  b.user = 100; b.nice = 10; b.system = 50; b.idle = 1000;
  CHECK(b.utilization_pct(a) == 0.0);
}

TEST_CASE("Uptime calculation", "[uptime]") {
  // btime epoch around now should produce reasonable uptime
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  auto btime = static_cast<uint64_t>(now) - 3600; // 1 hour ago
  auto uptime = LoadReader::uptime_seconds(btime);
  CHECK(uptime.count() >= 3599);
  CHECK(uptime.count() <= 3601);
}

TEST_CASE("Real /proc/loadavg access", "[integration][loadavg]") {
  LoadReader reader;
  auto result = reader.read_loadavg();
  REQUIRE(result.has_value());
  CHECK(result->load_1 >= 0.0);
  CHECK(result->load_5 >= 0.0);
  CHECK(result->load_15 >= 0.0);
  CHECK(result->total_procs > 0);
  CHECK(result->last_pid > 0);

  // Load averages shouldn't be absurdly high on typical systems
  // (could be high on loaded systems, but should be reasonable)
  CHECK(result->load_15 < 1000.0);
}

TEST_CASE("Real /proc/stat access", "[integration][stat]") {
  LoadReader reader;
  auto result = reader.read_stat();
  REQUIRE(result.has_value());

  // Should have at least 1 core
  CHECK(result->cores.size() >= 1);
  CHECK(result->cpu_total.total() > 0);

  // Context switches should be > 0 on any running system
  CHECK(result->ctxt > 0);
  CHECK(result->processes > 0);

  // btime should be reasonable (Unix epoch > 1700000000)
  CHECK(result->btime > 1700000000ULL);
}

// ── Benchmarks ──
TEST_CASE("Loadavg read performance", "[benchmark]") {
  LoadReader reader;
  BENCHMARK("Read /proc/loadavg") {
    return reader.read_loadavg();
  };
}

TEST_CASE("Stat read performance", "[benchmark]") {
  LoadReader reader;
  BENCHMARK("Read /proc/stat") {
    return reader.read_stat();
  };
}
