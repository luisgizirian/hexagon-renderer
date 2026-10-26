#include "platform/cli.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string_view>

#include "ink_model/ink_model.hpp"

namespace hexr {

const char* toString(Command c) {
  switch (c) {
    case Command::Render: return "render";
    case Command::Info: return "info";
    case Command::Ink: return "ink";
    case Command::Svg: return "svg";
    case Command::Validate: return "validate";
    case Command::Selftest: return "selftest";
    case Command::Help: return "help";
  }
  return "?";
}
const char* toString(DrawMode m) {
  return m == DrawMode::Fill ? "fill" : m == DrawMode::Outline ? "outline" : "fill+outline";
}
const char* toString(Submit s) { return s == Submit::Batched ? "batched" : "per-cell"; }

double Config::effectiveCellSize() const {
  return (shape == Shape::Rect && equalArea) ? equalAreaSquareSide(cellSize) : cellSize;
}

std::string usage() {
  return R"(Usage: hexbench [command] [options]

Commands:
  render     Render a grid offscreen (default). Writes a PPM image with --out.
  validate   Render and verify coverage/ids against an independent CPU reference.
  selftest   Render a procedural test pattern offscreen and verify every pixel.
  info       Print device, software, power and thermal information as JSON.
  ink        Print the geometric ink/space model for the grid (and an equal-area counterpart).
  svg        Export the grid as SVG (--out file).
  help       Show this text.

Grid / rendering options:
  --shape rect|hex            Cell shape (default hex)
  --mesh rect2|hex4|hex6      Triangulation (default rect2 for rect, hex4 for hex)
  --layout per-cell|shared    Vertex layout (default per-cell)
  --submit batched|per-cell   One draw for all cells, or one draw per cell (default batched)
  --mode fill|outline|fill+outline   (default fill)
  --cols N --rows N           Grid dimensions (default 64x64)
  --cell-size X               Hex side / square side in world units = pixels (default 8)
  --equal-area                For rect: use the square with the same area as a hex of --cell-size
  --width W --height H        Render target size (default: fit grid bounds)
  --stroke X                  Outline width in world units (default 1)
  --warmup N --iterations N   Benchmark repetitions (used from Phase 4)
  --frames N                  hexview: exit after N frames (default: run until closed)
  --out PATH                  Output file
  --format json|csv           Text output format (default json; csv: ink only)
  --validation                Enable Vulkan validation layers

Ink / page options (ink, svg):
  --page-width MM --page-height MM --page-margin MM   (default A4, 10 mm margin)
  --fill-ink X --stroke-ink X Relative ink per unit area (default 1)
)";
}

namespace {

bool parseInt(std::string_view s, int& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  errno = 0;
  const long v = std::strtol(std::string(s).c_str(), &end, 10);
  if (errno || *end || v < INT32_MIN || v > INT32_MAX) return false;
  out = int(v);
  return true;
}

bool parseDouble(std::string_view s, double& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  errno = 0;
  const double v = std::strtod(std::string(s).c_str(), &end);
  if (errno || *end || !std::isfinite(v)) return false;
  out = v;
  return true;
}

}  // namespace

ParseResult parseArgs(int argc, const char* const* argv) {
  ParseResult r;
  Config& c = r.cfg;
  auto fail = [&](std::string msg) {
    r.ok = false;
    r.error = std::move(msg);
    return r;
  };

  int i = 1;
  if (i < argc && argv[i][0] != '-') {
    const std::string_view cmd = argv[i++];
    if (cmd == "render") c.command = Command::Render;
    else if (cmd == "validate") c.command = Command::Validate;
    else if (cmd == "info") c.command = Command::Info;
    else if (cmd == "ink") c.command = Command::Ink;
    else if (cmd == "svg") c.command = Command::Svg;
    else if (cmd == "selftest") c.command = Command::Selftest;
    else if (cmd == "help") c.command = Command::Help;
    else return fail("unknown command: " + std::string(cmd));
  }

  for (; i < argc; ++i) {
    const std::string_view a = argv[i];
    auto value = [&](std::string_view& v) {
      if (i + 1 >= argc) return false;
      v = argv[++i];
      return true;
    };
    std::string_view v;
    auto needInt = [&](int& dst) {
      return value(v) && parseInt(v, dst);
    };
    auto needDouble = [&](double& dst) {
      return value(v) && parseDouble(v, dst);
    };
    const std::string bad = "invalid or missing value for " + std::string(a);

    if (a == "-h" || a == "--help") { c.command = Command::Help; }
    else if (a == "--shape") {
      if (!value(v)) return fail(bad);
      if (v == "rect") c.shape = Shape::Rect;
      else if (v == "hex") c.shape = Shape::Hex;
      else return fail("--shape must be rect or hex");
    } else if (a == "--mesh") {
      if (!value(v)) return fail(bad);
      if (v == "rect2") c.mesh = MeshKind::Rect2;
      else if (v == "hex4") c.mesh = MeshKind::Hex4;
      else if (v == "hex6") c.mesh = MeshKind::Hex6;
      else return fail("--mesh must be rect2, hex4 or hex6");
      c.meshExplicit = true;
    } else if (a == "--layout") {
      if (!value(v)) return fail(bad);
      if (v == "per-cell") c.layout = VertexLayout::PerCell;
      else if (v == "shared") c.layout = VertexLayout::Shared;
      else return fail("--layout must be per-cell or shared");
    } else if (a == "--submit") {
      if (!value(v)) return fail(bad);
      if (v == "batched") c.submit = Submit::Batched;
      else if (v == "per-cell") c.submit = Submit::PerCell;
      else return fail("--submit must be batched or per-cell");
    } else if (a == "--mode") {
      if (!value(v)) return fail(bad);
      if (v == "fill") c.mode = DrawMode::Fill;
      else if (v == "outline") c.mode = DrawMode::Outline;
      else if (v == "fill+outline") c.mode = DrawMode::FillOutline;
      else return fail("--mode must be fill, outline or fill+outline");
    } else if (a == "--format") {
      if (!value(v)) return fail(bad);
      if (v != "json" && v != "csv") return fail("--format must be json or csv");
      c.format = std::string(v);
    } else if (a == "--out") {
      if (!value(v)) return fail(bad);
      c.out = std::string(v);
    } else if (a == "--cols") { if (!needInt(c.cols)) return fail(bad); }
    else if (a == "--rows") { if (!needInt(c.rows)) return fail(bad); }
    else if (a == "--width") { if (!needInt(c.width)) return fail(bad); }
    else if (a == "--height") { if (!needInt(c.height)) return fail(bad); }
    else if (a == "--warmup") { if (!needInt(c.warmup)) return fail(bad); }
    else if (a == "--iterations") { if (!needInt(c.iterations)) return fail(bad); }
    else if (a == "--frames") { if (!needInt(c.frames)) return fail(bad); }
    else if (a == "--cell-size") { if (!needDouble(c.cellSize)) return fail(bad); }
    else if (a == "--stroke") { if (!needDouble(c.stroke)) return fail(bad); }
    else if (a == "--page-width") { if (!needDouble(c.pageWidth)) return fail(bad); }
    else if (a == "--page-height") { if (!needDouble(c.pageHeight)) return fail(bad); }
    else if (a == "--page-margin") { if (!needDouble(c.pageMargin)) return fail(bad); }
    else if (a == "--fill-ink") { if (!needDouble(c.fillInk)) return fail(bad); }
    else if (a == "--stroke-ink") { if (!needDouble(c.strokeInk)) return fail(bad); }
    else if (a == "--equal-area") { c.equalArea = true; }
    else if (a == "--validation") { c.validationLayers = true; }
    else return fail("unknown option: " + std::string(a));
  }

  if (!c.meshExplicit) c.mesh = defaultMesh(c.shape);
  if (!meshMatchesShape(c.mesh, c.shape)) return fail("--mesh does not match --shape");
  if (c.cols < 1 || c.rows < 1) return fail("--cols and --rows must be >= 1");
  if ((long long)c.cols * c.rows > (1LL << 26)) return fail("grid too large (max 2^26 cells)");
  if (!(c.cellSize > 0)) return fail("--cell-size must be > 0");
  if (!(c.stroke > 0)) return fail("--stroke must be > 0");
  if (c.width < 0 || c.height < 0 || c.width > 16384 || c.height > 16384)
    return fail("--width/--height must be in [0, 16384]");
  if (c.frames < 0) return fail("--frames must be >= 0");
  if (c.warmup < 0 || c.iterations < 1) return fail("--warmup must be >= 0 and --iterations >= 1");
  if (!(c.pageWidth > 0) || !(c.pageHeight > 0) || c.pageMargin < 0 ||
      2 * c.pageMargin >= c.pageWidth || 2 * c.pageMargin >= c.pageHeight)
    return fail("invalid page dimensions");
  if (c.fillInk < 0 || c.strokeInk < 0) return fail("ink densities must be >= 0");
  if (c.format == "csv" && c.command != Command::Ink) return fail("--format csv is currently only supported by ink");
  if (c.equalArea && c.shape != Shape::Rect) return fail("--equal-area only applies to --shape rect");
  r.ok = true;
  return r;
}

JsonObject configToJson(const Config& c) {
  JsonObject o;
  o.set("command", toString(c.command))
      .set("shape", toString(c.shape))
      .set("mesh", toString(c.mesh))
      .set("layout", toString(c.layout))
      .set("submit", toString(c.submit))
      .set("mode", toString(c.mode))
      .set("cols", c.cols)
      .set("rows", c.rows)
      .set("cell_size", c.cellSize)
      .set("equal_area", c.equalArea)
      .set("effective_cell_size", c.effectiveCellSize())
      .set("width", c.width)
      .set("height", c.height)
      .set("stroke", c.stroke)
      .set("warmup", c.warmup)
      .set("iterations", c.iterations);
  return o;
}

}  // namespace hexr
