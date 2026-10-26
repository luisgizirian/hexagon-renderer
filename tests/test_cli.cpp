#include <initializer_list>
#include <string>
#include <vector>

#include "doctest.h"
#include "platform/cli.hpp"

using namespace hexr;

static ParseResult parse(std::initializer_list<const char*> args) {
  std::vector<const char*> v{"hexbench"};
  v.insert(v.end(), args.begin(), args.end());
  return parseArgs(int(v.size()), v.data());
}

TEST_CASE("defaults") {
  const ParseResult r = parse({});
  REQUIRE(r.ok);
  CHECK((r.cfg.command == Command::Render));
  CHECK((r.cfg.shape == Shape::Hex));
  CHECK((r.cfg.mesh == MeshKind::Hex4));
  CHECK((r.cfg.layout == VertexLayout::PerCell));
  CHECK((r.cfg.submit == Submit::Batched));
  CHECK((r.cfg.mode == DrawMode::Fill));
  CHECK(r.cfg.cols == 64);
  CHECK(r.cfg.rows == 64);
  CHECK_FALSE(r.cfg.validationLayers);
}

TEST_CASE("valid full parse") {
  const ParseResult r =
      parse({"ink", "--shape", "rect", "--layout", "shared", "--submit", "per-cell", "--mode",
             "fill+outline", "--cols", "13", "--rows", "7", "--cell-size", "5.5", "--equal-area",
             "--stroke", "0.75", "--width", "640", "--height", "480", "--format", "csv", "--out",
             "x.csv", "--validation", "--warmup", "0", "--iterations", "3"});
  REQUIRE_MESSAGE(r.ok, r.error);
  const Config& c = r.cfg;
  CHECK((c.command == Command::Ink));
  CHECK((c.shape == Shape::Rect));
  CHECK((c.mesh == MeshKind::Rect2));  // default mesh follows shape
  CHECK((c.layout == VertexLayout::Shared));
  CHECK((c.submit == Submit::PerCell));
  CHECK((c.mode == DrawMode::FillOutline));
  CHECK(c.cols == 13);
  CHECK(c.rows == 7);
  CHECK(c.cellSize == 5.5);
  CHECK(c.equalArea);
  CHECK(c.effectiveCellSize() > c.cellSize);
  CHECK(c.stroke == 0.75);
  CHECK(c.width == 640);
  CHECK(c.height == 480);
  CHECK(c.format == "csv");
  CHECK(c.out == "x.csv");
  CHECK(c.validationLayers);
  CHECK(c.warmup == 0);
  CHECK(c.iterations == 3);
  const std::string j = configToJson(c).str(false);
  CHECK(j.find("\"shape\":\"rect\"") != std::string::npos);
  CHECK(j.find("\"cols\":13") != std::string::npos);
}

TEST_CASE("explicit hex6 mesh") {
  const ParseResult r = parse({"--mesh", "hex6"});
  REQUIRE(r.ok);
  CHECK((r.cfg.mesh == MeshKind::Hex6));
}

TEST_CASE("errors are rejected with a message") {
  const std::vector<std::vector<const char*>> bad = {
      {"bogus"},
      {"--nope"},
      {"--shape", "triangle"},
      {"--shape"},
      {"--mesh", "hex4", "--shape", "rect"},
      {"--cols", "0"},
      {"--rows", "-3"},
      {"--cols", "12abc"},
      {"--cols", "99999", "--rows", "99999"},
      {"--cell-size", "0"},
      {"--cell-size", "nan"},
      {"--stroke", "-1"},
      {"--width", "20000"},
      {"--iterations", "0"},
      {"--frames", "-1"},
      {"--format", "xml"},
      {"render", "--format", "csv"},
      {"--layout", "weird"},
      {"--submit", "sometimes"},
      {"--mode", "dots"},
      {"--page-margin", "200"},
      {"--fill-ink", "-0.1"},
      {"--equal-area"},  // hex is the default shape
  };
  for (const auto& args : bad) {
    std::vector<const char*> v{"hexbench"};
    v.insert(v.end(), args.begin(), args.end());
    const ParseResult r = parseArgs(int(v.size()), v.data());
    CAPTURE(args[0]);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
  }
}

TEST_CASE("usage mentions every command") {
  const std::string u = usage();
  for (const char* cmd : {"render", "validate", "selftest", "info", "ink", "svg"})
    CHECK(u.find(cmd) != std::string::npos);
}
