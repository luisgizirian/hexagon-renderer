// GPU tests: offscreen rendering compared against independent CPU references. Validation layers are
// enabled when HEX_VALIDATION=1 (set by CTest); any validation message fails the suite.
#include <memory>

#include "doctest.h"
#include "renderers/mesh_renderer.hpp"
#include "renderers/test_pattern.hpp"
#include "renderers/validation.hpp"
#include "vk/offscreen.hpp"

using namespace hexr;

static VulkanContext& gpu() {
  static std::unique_ptr<VulkanContext> ctx = std::make_unique<VulkanContext>(ContextOptions{});
  return *ctx;
}

static Config makeCfg(Shape s, MeshKind k, VertexLayout l, Submit sub, int cols, int rows, double size) {
  Config c;
  c.shape = s;
  c.mesh = k;
  c.layout = l;
  c.submit = sub;
  c.cols = cols;
  c.rows = rows;
  c.cellSize = size;
  return c;
}

TEST_CASE("test pattern renders exactly") {
  for (auto [w, h] : {std::pair{320u, 200u}, {1u, 1u}, {257u, 3u}}) {
    const auto px = renderTestPatternOffscreen(gpu(), w, h);
    CHECK(verifyTestPattern(px.data(), w, h) == 0);
  }
}

TEST_CASE("fill coverage: no gaps, overlaps or wrong cells across strategies") {
  struct Variant { Shape s; MeshKind k; };
  for (Variant v : {Variant{Shape::Rect, MeshKind::Rect2}, Variant{Shape::Hex, MeshKind::Hex4},
                    Variant{Shape::Hex, MeshKind::Hex6}})
    for (VertexLayout l : {VertexLayout::PerCell, VertexLayout::Shared})
      for (Submit sub : {Submit::Batched, Submit::PerCell})
        for (auto [cols, rows, size] : {std::tuple{1, 1, 9.0}, {7, 5, 6.3}, {13, 11, 3.7}, {40, 30, 2.25}}) {
          CAPTURE(toString(v.k));
          CAPTURE(toString(l));
          CAPTURE(toString(sub));
          CAPTURE(cols);
          CAPTURE(size);
          const Config cfg = makeCfg(v.s, v.k, l, sub, cols, rows, size);
          const ValidationReport r = validateFill(gpu(), Grid(v.s, cols, rows, size), cfg);
          CHECK(r.gaps == 0);
          CHECK(r.overlaps == 0);
          CHECK(r.spurious == 0);
          CHECK(r.idMismatches == 0);
          CHECK(r.deepInside > 0);
          // Covered pixel count matches the analytic area up to the boundary band.
          CHECK(double(r.coveredPixels) == doctest::Approx(r.expectedArea).epsilon(0.15));
        }
}

TEST_CASE("equal-area square grid also validates") {
  Config cfg = makeCfg(Shape::Rect, MeshKind::Rect2, VertexLayout::Shared, Submit::Batched, 17, 9, 4.0);
  cfg.equalArea = true;
  const ValidationReport r = validateFill(gpu(), Grid(Shape::Rect, 17, 9, cfg.effectiveCellSize()), cfg);
  CHECK(r.pass());
}

TEST_CASE("negative control: validator detects a wrong rendering") {
  const Config cfg = makeCfg(Shape::Hex, MeshKind::Hex4, VertexLayout::PerCell, Submit::Batched, 8, 6, 5.0);
  // Rendered grid is shifted relative to the reference (different cell size): must fail.
  const ValidationReport r =
      validateFill(gpu(), Grid(Shape::Hex, 8, 6, 5.0), Grid(Shape::Hex, 8, 6, 5.5), cfg);
  CHECK_FALSE(r.pass());
  CHECK(r.gaps + r.spurious + r.idMismatches > 0);
  // Rect rendered, hex reference: must fail.
  const Config rc = makeCfg(Shape::Rect, MeshKind::Rect2, VertexLayout::PerCell, Submit::Batched, 8, 6, 5.0);
  CHECK_FALSE(validateFill(gpu(), Grid(Shape::Rect, 8, 6, 5.0), Grid(Shape::Hex, 8, 6, 5.0), rc).pass());
}

TEST_CASE("draw statistics reflect the submission strategy") {
  OffscreenTarget target(gpu(), 64, 64, VK_FORMAT_R8G8B8A8_UNORM);
  MeshRenderer r(gpu(), VK_FORMAT_R8G8B8A8_UNORM, OutputKind::Color);
  Grid g(Shape::Hex, 6, 5, 4.0);
  Config cfg = makeCfg(Shape::Hex, MeshKind::Hex4, VertexLayout::PerCell, Submit::Batched, 6, 5, 4.0);
  r.setScene(g, cfg);
  CHECK(r.stats().drawCalls == 1);
  CHECK(r.stats().triangles == 30 * 4);
  CHECK(r.stats().vertices == 30 * 6);
  cfg.submit = Submit::PerCell;
  cfg.mesh = MeshKind::Hex6;
  r.setScene(g, cfg);
  CHECK(r.stats().drawCalls == 30);
  CHECK(r.stats().triangles == 30 * 6);
  cfg.submit = Submit::Batched;
  cfg.mode = DrawMode::FillOutline;
  r.setScene(g, cfg);
  CHECK(r.stats().drawCalls == 2);
  CHECK(r.stats().triangles == 30 * 6 + 2 * g.uniqueEdges().size());
  // Recording and submitting a fill+outline frame works.
  VkCommandBuffer cb = gpu().beginOneShot();
  target.begin(cb, {{1, 1, 1, 1}});
  r.record(cb, 64, 64);
  target.endAndReadback(cb);
  gpu().submitAndWait(cb);
}

TEST_CASE("larger grid validates (shared, batched)") {
  const Config cfg =
      makeCfg(Shape::Hex, MeshKind::Hex4, VertexLayout::Shared, Submit::Batched, 300, 200, 3.0);
  const ValidationReport r = validateFill(gpu(), Grid(Shape::Hex, 300, 200, 3.0), cfg);
  CHECK(r.pass());
  CHECK(r.width > 1500);
}

// Runs last (doctest orders by file/line): no validation-layer messages during the whole suite.
TEST_CASE("zz: no validation layer messages") {
  INFO("validation enabled: " << gpu().validationEnabled());
  CHECK(gpu().validationMessages() == 0);
}
