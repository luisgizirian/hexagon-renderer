#include <cmath>
#include <limits>
#include <string>

#include "doctest.h"
#include "platform/json.hpp"

using namespace hexr;

TEST_CASE("string escaping") {
  CHECK(jsonEscape("plain") == "\"plain\"");
  CHECK(jsonEscape("a\"b\\c") == "\"a\\\"b\\\\c\"");
  CHECK(jsonEscape("l1\nl2\t") == "\"l1\\nl2\\t\"");
  CHECK(jsonEscape(std::string(1, '\x01')) == "\"\\u0001\"");
  CHECK(jsonEscape("µm") == "\"µm\"");  // UTF-8 passes through
}

TEST_CASE("numbers round-trip; non-finite becomes null") {
  CHECK(jsonNumber(0.0) == "0");
  CHECK(jsonNumber(1.5) == "1.5");
  for (double v : {0.1, 1.0 / 3.0, 6.02214076e23, -1e-300, 918000000.0})
    CHECK(std::strtod(jsonNumber(v).c_str(), nullptr) == v);
  CHECK(jsonNumber(std::nan("")) == "null");
  CHECK(jsonNumber(std::numeric_limits<double>::infinity()) == "null");
}

TEST_CASE("objects: nesting, types, insertion order, key overwrite") {
  JsonObject inner;
  inner.set("x", 1).set("ok", true);
  JsonObject o;
  o.set("name", "hex").set("n", 3ULL).set("neg", -2LL).set("child", inner).set("name", "rect");
  CHECK(o.str(false) == "{\"name\":\"rect\",\"n\":3,\"neg\":-2,\"child\":{\"x\":1,\"ok\":true}}");
  o.setRaw("child", "[1,2]");
  CHECK(o.str(false) == "{\"name\":\"rect\",\"n\":3,\"neg\":-2,\"child\":[1,2]}");
  CHECK(JsonObject().str() == "{}");
  const std::string pretty = o.str(true);
  CHECK(pretty.find("\n  \"name\": \"rect\"") != std::string::npos);
  JsonObject outer;
  outer.setRaw("m", "{\n  \"a\": 1\n}");
  CHECK(outer.str(true) == "{\n  \"m\": {\n    \"a\": 1\n  }\n}");
}
