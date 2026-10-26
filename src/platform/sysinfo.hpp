#pragma once
// Read-only collection of host, software, power and thermal state (Jetson sysfs where available).
// Never changes device settings.
#include <string>

#include "platform/json.hpp"

namespace hexr {

std::string readFileTrimmed(const std::string& path);
std::string runCommand(const std::string& cmd);  // stdout, trimmed; empty on failure
std::string isoTimestampUtc();

JsonObject collectSystemInfo();  // static configuration: board, OS, L4T, compiler, power mode
JsonObject collectDynamicState(); // clocks, temperatures, load, memory (sample at run start/end)

}  // namespace hexr
