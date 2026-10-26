#pragma once
// Minimal ordered JSON object builder (no external dependency).
#include <memory>
#include <string>
#include <vector>

namespace hexr {

std::string jsonEscape(const std::string& s);
std::string jsonNumber(double v);  // round-trip precision; NaN/Inf -> null

class JsonObject {
 public:
  JsonObject& set(const std::string& key, double v);
  JsonObject& set(const std::string& key, int v) { return set(key, (long long)v); }
  JsonObject& set(const std::string& key, long v) { return set(key, (long long)v); }
  JsonObject& set(const std::string& key, long long v);
  JsonObject& set(const std::string& key, unsigned v) { return set(key, (unsigned long long)v); }
  JsonObject& set(const std::string& key, unsigned long v) { return set(key, (unsigned long long)v); }
  JsonObject& set(const std::string& key, unsigned long long v);
  JsonObject& set(const std::string& key, bool v);
  JsonObject& set(const std::string& key, const std::string& v);
  JsonObject& set(const std::string& key, const char* v) { return set(key, std::string(v)); }
  JsonObject& set(const std::string& key, const JsonObject& v);
  JsonObject& setRaw(const std::string& key, std::string rawJson);
  std::string str(bool pretty = true, int indent = 0) const;
  bool empty() const { return fields_.empty(); }

 private:
  struct Field {
    std::string key;
    std::string raw;                    // used when child is null
    std::shared_ptr<JsonObject> child;  // nested object
  };
  Field& slot(const std::string& key);
  std::vector<Field> fields_;
};

}  // namespace hexr
