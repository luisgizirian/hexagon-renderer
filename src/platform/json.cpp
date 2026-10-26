#include "platform/json.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hexr {

std::string jsonEscape(const std::string& s) {
  std::string out = "\"";
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += char(c);
        }
    }
  }
  return out + "\"";
}

std::string jsonNumber(double v) {
  if (!std::isfinite(v)) return "null";
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.17g", v);
  // Prefer a shorter representation when it round-trips exactly.
  for (int prec = 6; prec < 17; ++prec) {
    char shortBuf[32];
    std::snprintf(shortBuf, sizeof shortBuf, "%.*g", prec, v);
    if (std::strtod(shortBuf, nullptr) == v) return shortBuf;
  }
  return buf;
}

JsonObject::Field& JsonObject::slot(const std::string& key) {
  for (auto& f : fields_)
    if (f.key == key) {
      f.child.reset();
      return f;
    }
  fields_.push_back({key, {}, nullptr});
  return fields_.back();
}

JsonObject& JsonObject::set(const std::string& key, double v) { slot(key).raw = jsonNumber(v); return *this; }
JsonObject& JsonObject::set(const std::string& key, long long v) { slot(key).raw = std::to_string(v); return *this; }
JsonObject& JsonObject::set(const std::string& key, unsigned long long v) { slot(key).raw = std::to_string(v); return *this; }
JsonObject& JsonObject::set(const std::string& key, bool v) { slot(key).raw = v ? "true" : "false"; return *this; }
JsonObject& JsonObject::set(const std::string& key, const std::string& v) { slot(key).raw = jsonEscape(v); return *this; }
JsonObject& JsonObject::set(const std::string& key, const JsonObject& v) {
  slot(key).child = std::make_shared<JsonObject>(v);
  return *this;
}
JsonObject& JsonObject::setRaw(const std::string& key, std::string rawJson) {
  slot(key).raw = std::move(rawJson);
  return *this;
}

std::string JsonObject::str(bool pretty, int indent) const {
  if (fields_.empty()) return "{}";
  const std::string pad = pretty ? std::string(size_t(indent + 2), ' ') : "";
  const std::string nl = pretty ? "\n" : "";
  std::string out = "{" + nl;
  for (size_t i = 0; i < fields_.size(); ++i) {
    const auto& f = fields_[i];
    out += pad + jsonEscape(f.key) + (pretty ? ": " : ":");
    if (f.child) {
      out += f.child->str(pretty, indent + 2);
    } else if (pretty && f.raw.find('\n') != std::string::npos) {
      // Re-indent pre-rendered multi-line JSON so nested output stays aligned.
      for (char ch : f.raw) {
        out += ch;
        if (ch == '\n') out.append(size_t(indent + 2), ' ');
      }
    } else {
      out += f.raw;
    }
    if (i + 1 < fields_.size()) out += ",";
    out += nl;
  }
  return out + (pretty ? std::string(size_t(indent), ' ') : "") + "}";
}

}  // namespace hexr
