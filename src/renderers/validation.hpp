#pragma once
#include <cstdint>

#include "geometry/grid.hpp"
#include "platform/cli.hpp"
#include "platform/json.hpp"
#include "vk/context.hpp"

namespace hexr {

// Compares GPU rasterization of the solid-fill mesh against an independent CPU reference
// (Grid::cellAt, i.e. axial cube rounding / floor) at every pixel centre.
//  * coverage pass: R32_SFLOAT target, additive blending -> number of fragments per pixel
//  * id pass:       R32_UINT target -> which cell wrote the pixel
// Pixels whose centre lies within `margin` pixels of any cell boundary are "ambiguous"
// (sub-pixel snapping may legitimately go either way) and only checked for overlaps.
struct ValidationReport {
  uint32_t width = 0, height = 0;
  uint64_t pixels = 0, deepInside = 0, deepOutside = 0, ambiguous = 0;
  uint64_t gaps = 0;            // deep-inside pixels with coverage 0
  uint64_t overlaps = 0;        // any pixel with coverage > 1
  uint64_t spurious = 0;        // deep-outside pixels with coverage > 0
  uint64_t idMismatches = 0;    // deep pixels whose id differs from the CPU reference
  uint64_t coveredPixels = 0;
  double expectedArea = 0;      // analytic fill area in pixels (cells * cell area)
  double margin = 0;
  bool pass() const { return gaps == 0 && overlaps == 0 && spurious == 0 && idMismatches == 0; }
  JsonObject toJson() const;
};

ValidationReport validateFill(VulkanContext& ctx, const Grid& grid, const Config& cfg, double margin = 0.02);
// Renders `rendered` but checks against `reference` (negative controls in tests).
ValidationReport validateFill(VulkanContext& ctx, const Grid& rendered, const Grid& reference, const Config& cfg,
                              double margin = 0.02);

// Target size used for rendering a grid: cfg.width/height or the grid bounds rounded up.
void renderTargetSize(const Grid& grid, const Config& cfg, uint32_t& w, uint32_t& h);

}  // namespace hexr
