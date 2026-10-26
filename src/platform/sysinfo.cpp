#include "platform/sysinfo.hpp"

#include <sys/utsname.h>

#include <array>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace hexr {

namespace fs = std::filesystem;

namespace {
std::string trim(std::string s) {
  const char* ws = " \t\r\n";
  s.erase(0, s.find_first_not_of(ws));
  const auto e = s.find_last_not_of(ws);
  s.erase(e == std::string::npos ? 0 : e + 1);
  return s;
}

long long meminfoKb(const std::string& key) {
  std::ifstream f("/proc/meminfo");
  std::string k;
  long long v;
  std::string unit;
  while (f >> k >> v >> unit)
    if (k == key + ":") return v;
  return -1;
}
}  // namespace

std::string readFileTrimmed(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return {};
  std::stringstream ss;
  ss << f.rdbuf();
  std::string s = ss.str();
  for (char& c : s)
    if (c == '\0') c = ' ';
  return trim(s);
}

std::string runCommand(const std::string& cmd) {
  FILE* p = popen((cmd + " 2>/dev/null").c_str(), "r");
  if (!p) return {};
  std::string out;
  std::array<char, 256> buf{};
  while (fgets(buf.data(), int(buf.size()), p)) out += buf.data();
  pclose(p);
  return trim(out);
}

std::string isoTimestampUtc() {
  std::time_t t = std::time(nullptr);
  std::tm tm{};
  gmtime_r(&t, &tm);
  char b[32];
  std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &tm);
  return b;
}

JsonObject collectSystemInfo() {
  JsonObject o;
  o.set("board_model", readFileTrimmed("/proc/device-tree/model"));
  const std::string l4t = readFileTrimmed("/etc/nv_tegra_release");
  o.set("l4t_release", l4t.substr(0, l4t.find('\n')));
  std::string osName;
  {
    std::ifstream f("/etc/os-release");
    std::string line;
    while (std::getline(f, line))
      if (line.rfind("PRETTY_NAME=", 0) == 0) osName = line.substr(13, line.size() - 14);
  }
  o.set("os", osName);
  utsname u{};
  if (uname(&u) == 0) o.set("kernel", std::string(u.release)).set("arch", std::string(u.machine));
  o.set("cpu_count", (long long)std::thread::hardware_concurrency());
  o.set("mem_total_kb", meminfoKb("MemTotal"));
  o.set("compiler", std::string(__VERSION__));
#ifdef NDEBUG
  o.set("build_type", "release");
#else
  o.set("build_type", "debug");
#endif
  const std::string pm = runCommand("nvpmodel -q");
  o.set("nvpmodel", pm.substr(0, pm.find('\n')));
  o.set("cuda_toolkit", runCommand("/usr/local/cuda/bin/nvcc --version | tail -1"));
  return o;
}

JsonObject collectDynamicState() {
  JsonObject o;
  o.set("timestamp_utc", isoTimestampUtc());
  o.set("loadavg", readFileTrimmed("/proc/loadavg"));
  o.set("mem_available_kb", meminfoKb("MemAvailable"));
  JsonObject cpu;
  for (int i = 0;; ++i) {
    const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(i) + "/cpufreq/";
    if (!fs::exists(base)) break;
    cpu.set("cpu" + std::to_string(i) + "_khz", readFileTrimmed(base + "scaling_cur_freq"));
  }
  o.set("cpu_freq", cpu);
  JsonObject gpu;
  const std::string g = "/sys/class/devfreq/17000000.gpu/";
  if (fs::exists(g)) {
    gpu.set("cur_hz", readFileTrimmed(g + "cur_freq"))
        .set("min_hz", readFileTrimmed(g + "min_freq"))
        .set("max_hz", readFileTrimmed(g + "max_freq"))
        .set("governor", readFileTrimmed(g + "governor"));
  }
  o.set("gpu_freq", gpu);
  JsonObject thermal;
  for (int i = 0;; ++i) {
    const std::string base = "/sys/devices/virtual/thermal/thermal_zone" + std::to_string(i) + "/";
    if (!fs::exists(base)) break;
    const std::string temp = readFileTrimmed(base + "temp");
    if (!temp.empty()) thermal.set(readFileTrimmed(base + "type") + "_mC", temp);
  }
  o.set("thermal", thermal);
  return o;
}

}  // namespace hexr
