// hexbench: headless command-line runner (render, validate, info, ink, svg, selftest).
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "geometry/grid.hpp"
#include "ink_model/ink_model.hpp"
#include "ink_model/svg.hpp"
#include "platform/cli.hpp"
#include "platform/sysinfo.hpp"
#include "renderers/mesh_renderer.hpp"
#include "renderers/test_pattern.hpp"
#include "renderers/validation.hpp"
#include "vk/offscreen.hpp"

using namespace hexr;

namespace {

bool emit(const std::string& text, const std::string& out) {
  if (out.empty() || out == "-") {
    std::fputs(text.c_str(), stdout);
    std::fputc('\n', stdout);
    return true;
  }
  if (!writeTextFile(out, text + "\n")) {
    std::fprintf(stderr, "error: cannot write %s\n", out.c_str());
    return false;
  }
  return true;
}

ContextOptions contextOptions(const Config& cfg) {
  ContextOptions o;
  o.validation = cfg.validationLayers;
  return o;
}

int cmdInfo(const Config& cfg) {
  JsonObject o;
  o.set("system", collectSystemInfo());
  o.set("state", collectDynamicState());
  try {
    VulkanContext ctx(contextOptions(cfg));
    o.set("vulkan", ctx.describe());
  } catch (const std::exception& e) {
    o.set("vulkan_error", std::string(e.what()));
  }
  return emit(o.str(), cfg.out) ? 0 : 1;
}

int cmdInk(const Config& cfg) {
  // Compare a hex grid against the equal-area square grid with identical cols x rows
  // (same cell count, same filled area), plus a fixed-page layout comparison.
  const double a = cfg.shape == Shape::Hex ? cfg.cellSize
                   : cfg.equalArea        ? cfg.cellSize
                                          : equalAreaHexSide(cfg.cellSize);
  const double s = equalAreaSquareSide(a);
  InkAssumptions ink{cfg.stroke, cfg.fillInk, cfg.strokeInk};
  const Grid hexGrid(Shape::Hex, cfg.cols, cfg.rows, a);
  const Grid rectGrid(Shape::Rect, cfg.cols, cfg.rows, s);
  const LayoutMetrics mh = computeMetrics(hexGrid, ink), mr = computeMetrics(rectGrid, ink);
  if (cfg.format == "csv")
    return emit(metricsCsvHeader() + "\n" + metricsCsvRow(mh, ink) + "\n" + metricsCsvRow(mr, ink), cfg.out) ? 0 : 1;

  JsonObject ratios;
  ratios.set("cell_perimeter_hex_over_rect", mh.cellPerimeter / mr.cellPerimeter)
      .set("unique_length_hex_over_rect", mh.uniqueLength / mr.uniqueLength)
      .set("ink_outline_hex_over_rect", mh.inkOutlineOnly / mr.inkOutlineOnly)
      .set("ink_fill_hex_over_rect", mh.inkFillOnly / mr.inkFillOnly)
      .set("ink_fill_and_outline_hex_over_rect", mh.inkFillAndOutline / mr.inkFillAndOutline)
      .set("bounds_area_hex_over_rect", mh.boundsArea / mr.boundsArea);

  const PageSpec page{cfg.pageWidth, cfg.pageHeight, cfg.pageMargin};
  JsonObject pageJson;
  pageJson.set("width", page.width).set("height", page.height).set("margin", page.margin);
  for (int k = 0; k < 2; ++k) {
    const Shape shape = k == 0 ? Shape::Hex : Shape::Rect;
    const double side = k == 0 ? a : s;
    int c = 0, r = 0;
    JsonObject pj;
    if (fitToPage(shape, side, page, c, r)) {
      const LayoutMetrics m = computeMetrics(Grid(shape, c, r, side), ink);
      const double usable = page.usableWidth() * page.usableHeight();
      pj.set("cols", c).set("rows", r).set("cells", m.cells).set("fill_area", m.totalFillArea)
          .set("usable_area", usable).set("utilization", m.totalFillArea / usable)
          .set("whitespace", usable - m.totalFillArea).set("unique_length", m.uniqueLength)
          .set("ink_fill_only", m.inkFillOnly).set("ink_outline_only", m.inkOutlineOnly)
          .set("ink_fill_and_outline", m.inkFillAndOutline);
    } else {
      pj.set("fits", false);
    }
    pageJson.set(k == 0 ? "hex" : "rect", pj);
  }

  JsonObject o;
  o.set("hex_side", a).set("equal_area_square_side", s)
      .set("theoretical_equal_area_perimeter_ratio", equalAreaPerimeterRatio())
      .setRaw("hex", metricsToJson(mh, ink))
      .setRaw("rect_equal_area", metricsToJson(mr, ink))
      .set("ratios", ratios)
      .set("page_fit", pageJson)
      .set("note", "Geometric estimates only; not measured printer ink. See docs/INK_MODEL.md.");
  return emit(o.str(), cfg.out) ? 0 : 1;
}

int cmdSvg(const Config& cfg) {
  if (cfg.out.empty() || cfg.out == "-") {
    std::fprintf(stderr, "error: svg requires --out FILE\n");
    return 2;
  }
  const Grid grid(cfg.shape, cfg.cols, cfg.rows, cfg.effectiveCellSize());
  SvgOptions o;
  o.fill = cfg.mode != DrawMode::Outline;
  o.outline = cfg.mode != DrawMode::Fill;
  o.strokeWidth = cfg.stroke;
  o.margin = cfg.stroke;
  if (!writeTextFile(cfg.out, gridToSvg(grid, o))) return 1;
  std::fprintf(stderr, "wrote %s\n", cfg.out.c_str());
  return 0;
}

int cmdSelftest(const Config& cfg) {
  VulkanContext ctx(contextOptions(cfg));
  const uint32_t w = cfg.width ? uint32_t(cfg.width) : 320, h = cfg.height ? uint32_t(cfg.height) : 200;
  const auto px = renderTestPatternOffscreen(ctx, w, h);
  const uint64_t bad = verifyTestPattern(px.data(), w, h);
  if (!cfg.out.empty() && !writePPM(cfg.out, px.data(), w, h)) return 1;
  JsonObject o;
  o.set("width", (unsigned long long)w).set("height", (unsigned long long)h)
      .set("mismatched_pixels", (unsigned long long)bad)
      .set("validation_layers", ctx.validationEnabled())
      .set("validation_messages", (unsigned long long)ctx.validationMessages())
      .set("pass", bad == 0 && ctx.validationMessages() == 0);
  emit(o.str(), "");
  return bad == 0 && ctx.validationMessages() == 0 ? 0 : 1;
}

int cmdRender(const Config& cfg) {
  VulkanContext ctx(contextOptions(cfg));
  const Grid grid(cfg.shape, cfg.cols, cfg.rows, cfg.effectiveCellSize());
  uint32_t w, h;
  renderTargetSize(grid, cfg, w, h);
  // Outer strokes extend stroke/2 beyond the cell bounds: pad auto-sized targets so they are not clipped.
  double pad = 0;
  if (cfg.mode != DrawMode::Fill) {
    pad = std::ceil(cfg.stroke / 2);
    if (cfg.width == 0) w += uint32_t(2 * pad);
    if (cfg.height == 0) h += uint32_t(2 * pad);
  }
  OffscreenTarget target(ctx, w, h, VK_FORMAT_R8G8B8A8_UNORM);
  MeshRenderer r(ctx, VK_FORMAT_R8G8B8A8_UNORM, OutputKind::Color);
  r.setScene(grid, cfg);
  VkCommandBuffer cb = ctx.beginOneShot();
  target.begin(cb, {{1.0f, 1.0f, 1.0f, 1.0f}});
  r.record(cb, w, h, -pad, -pad);
  target.endAndReadback(cb);
  ctx.submitAndWait(cb);
  if (!cfg.out.empty() && !writePPM(cfg.out, static_cast<const uint8_t*>(target.pixels()), w, h)) {
    std::fprintf(stderr, "error: cannot write %s\n", cfg.out.c_str());
    return 1;
  }
  JsonObject o;
  o.set("config", configToJson(cfg))
      .set("target_width", (unsigned long long)w)
      .set("target_height", (unsigned long long)h)
      .set("cells", (long long)grid.cellCount())
      .set("stats", r.stats().toJson())
      .set("validation_messages", (unsigned long long)ctx.validationMessages());
  emit(o.str(), "");
  return ctx.validationMessages() == 0 ? 0 : 1;
}

int cmdValidate(const Config& cfg) {
  VulkanContext ctx(contextOptions(cfg));
  const Grid grid(cfg.shape, cfg.cols, cfg.rows, cfg.effectiveCellSize());
  const ValidationReport rep = validateFill(ctx, grid, cfg);
  JsonObject o;
  o.set("config", configToJson(cfg))
      .set("report", rep.toJson())
      .set("validation_messages", (unsigned long long)ctx.validationMessages());
  emit(o.str(), cfg.out);
  return rep.pass() && ctx.validationMessages() == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  const ParseResult pr = parseArgs(argc, argv);
  if (!pr.ok) {
    std::fprintf(stderr, "error: %s\n\n%s", pr.error.c_str(), usage().c_str());
    return 2;
  }
  const Config& cfg = pr.cfg;
  try {
    switch (cfg.command) {
      case Command::Help: std::fputs(usage().c_str(), stdout); return 0;
      case Command::Info: return cmdInfo(cfg);
      case Command::Ink: return cmdInk(cfg);
      case Command::Svg: return cmdSvg(cfg);
      case Command::Selftest: return cmdSelftest(cfg);
      case Command::Render: return cmdRender(cfg);
      case Command::Validate: return cmdValidate(cfg);
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
  return 0;
}
