#include "renderers/validation.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "renderers/mesh_renderer.hpp"
#include "vk/offscreen.hpp"

namespace hexr {

JsonObject ValidationReport::toJson() const {
  JsonObject o;
  o.set("pass", pass())
      .set("width", (unsigned long long)width)
      .set("height", (unsigned long long)height)
      .set("pixels", (unsigned long long)pixels)
      .set("deep_inside", (unsigned long long)deepInside)
      .set("deep_outside", (unsigned long long)deepOutside)
      .set("ambiguous", (unsigned long long)ambiguous)
      .set("gaps", (unsigned long long)gaps)
      .set("overlaps", (unsigned long long)overlaps)
      .set("spurious", (unsigned long long)spurious)
      .set("id_mismatches", (unsigned long long)idMismatches)
      .set("covered_pixels", (unsigned long long)coveredPixels)
      .set("expected_area_px", expectedArea)
      .set("margin_px", margin);
  return o;
}

void renderTargetSize(const Grid& grid, const Config& cfg, uint32_t& w, uint32_t& h) {
  const Bounds b = grid.bounds();
  w = cfg.width > 0 ? uint32_t(cfg.width) : uint32_t(std::max(1.0, std::ceil(b.maxX)));
  h = cfg.height > 0 ? uint32_t(cfg.height) : uint32_t(std::max(1.0, std::ceil(b.maxY)));
}

namespace {
template <typename T>
std::vector<T> renderPass(VulkanContext& ctx, const Grid& grid, const Config& cfg, OutputKind kind, VkFormat fmt,
                          uint32_t w, uint32_t h) {
  OffscreenTarget target(ctx, w, h, fmt);
  MeshRenderer r(ctx, fmt, kind);
  r.setScene(grid, cfg);
  VkCommandBuffer cb = ctx.beginOneShot();
  VkClearColorValue clear{};
  target.begin(cb, clear);
  r.record(cb, w, h);
  target.endAndReadback(cb);
  ctx.submitAndWait(cb);
  std::vector<T> out(size_t(w) * h);
  std::memcpy(out.data(), target.pixels(), out.size() * sizeof(T));
  return out;
}
}  // namespace

ValidationReport validateFill(VulkanContext& ctx, const Grid& grid, const Config& cfg, double margin) {
  return validateFill(ctx, grid, grid, cfg, margin);
}

ValidationReport validateFill(VulkanContext& ctx, const Grid& rendered, const Grid& grid, const Config& cfgIn,
                              double margin) {
  Config cfg = cfgIn;
  cfg.mode = DrawMode::Fill;
  ValidationReport rep;
  rep.margin = margin;
  renderTargetSize(grid, cfg, rep.width, rep.height);
  const uint32_t w = rep.width, h = rep.height;

  if (!ctx.formatSupports(VK_FORMAT_R32_SFLOAT, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT))
    throw std::runtime_error("R32_SFLOAT blending unsupported; coverage validation unavailable");
  const auto counts = renderPass<float>(ctx, rendered, cfg, OutputKind::Coverage, VK_FORMAT_R32_SFLOAT, w, h);
  const auto ids = renderPass<uint32_t>(ctx, rendered, cfg, OutputKind::CellId, VK_FORMAT_R32_UINT, w, h);

  rep.pixels = uint64_t(w) * h;
  rep.expectedArea = grid.cellCount() * grid.cellArea();
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      const size_t i = size_t(y) * w + x;
      const float count = counts[i];
      const uint32_t id = ids[i];
      if (count > 0.5f) rep.coveredPixels++;
      if (count > 1.5f) rep.overlaps++;
      const Vec2 p{x + 0.5, y + 0.5};
      const int cell = grid.cellAt(p);
      if (cell >= 0) {
        if (grid.distanceToCellBoundary(cell, p) <= margin) {
          rep.ambiguous++;
          continue;
        }
        rep.deepInside++;
        if (count < 0.5f) rep.gaps++;
        if (id != uint32_t(cell) + 1) rep.idMismatches++;
      } else {
        double d = INFINITY;
        for (int c : grid.cellsNear(p)) d = std::min(d, grid.distanceToCellBoundary(c, p));
        if (d <= margin) {
          rep.ambiguous++;
          continue;
        }
        rep.deepOutside++;
        if (count > 0.5f) rep.spurious++;
        if (id != 0) rep.idMismatches++;
      }
    }
  return rep;
}

}  // namespace hexr
