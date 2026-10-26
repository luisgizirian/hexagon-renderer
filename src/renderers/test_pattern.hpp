#pragma once
#include <cstdint>
#include <vector>

#include "vk/pipeline.hpp"

namespace hexr {

// Phase-2 vertical slice: a full-screen procedural pattern with a CPU-computable expected value.
class TestPattern {
 public:
  TestPattern(VulkanContext& ctx, VkFormat format);
  ~TestPattern();
  void record(VkCommandBuffer cb, uint32_t w, uint32_t h);

 private:
  VulkanContext& ctx_;
  Pipeline pipe_;
};

// Expected RGBA8 for pixel (x, y) of a w x h target (UNORM rounding), see test_pattern.frag.
void testPatternExpected(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t out[4]);
// Number of pixels whose channels differ from the expectation by more than `tolerance`.
uint64_t verifyTestPattern(const uint8_t* rgba, uint32_t w, uint32_t h, int tolerance = 1);

// Renders the pattern offscreen and returns the RGBA8 pixels.
std::vector<uint8_t> renderTestPatternOffscreen(VulkanContext& ctx, uint32_t w, uint32_t h);

}  // namespace hexr
