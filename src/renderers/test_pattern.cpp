#include "renderers/test_pattern.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "test_pattern_frag.h"
#include "test_pattern_vert.h"
#include "vk/offscreen.hpp"

namespace hexr {

TestPattern::TestPattern(VulkanContext& ctx, VkFormat format) : ctx_(ctx) {
  PipelineDesc d{HEX_SPV(spv_test_pattern_vert), HEX_SPV(spv_test_pattern_frag)};
  d.colorFormat = format;
  d.pushConstantBytes = 8;
  pipe_ = createPipeline(ctx, d);
}

TestPattern::~TestPattern() { destroy(ctx_, pipe_); }

void TestPattern::record(VkCommandBuffer cb, uint32_t w, uint32_t h) {
  const float size[2] = {float(w), float(h)};
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe_.pipeline);
  vkCmdPushConstants(cb, pipe_.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, size);
  vkCmdDraw(cb, 3, 1, 0, 0);
}

void testPatternExpected(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t out[4]) {
  const float fx = (float(x) + 0.5f) / float(w), fy = (float(y) + 0.5f) / float(h);
  out[0] = uint8_t(std::lround(fx * 255.0f));
  out[1] = uint8_t(std::lround(fy * 255.0f));
  out[2] = (((x >> 4) + (y >> 4)) & 1) ? 255 : 0;
  out[3] = 255;
}

uint64_t verifyTestPattern(const uint8_t* rgba, uint32_t w, uint32_t h, int tolerance) {
  uint64_t bad = 0;
  uint8_t e[4];
  for (uint32_t y = 0; y < h; ++y)
    for (uint32_t x = 0; x < w; ++x) {
      testPatternExpected(x, y, w, h, e);
      const uint8_t* p = rgba + (size_t(y) * w + x) * 4;
      for (int c = 0; c < 4; ++c)
        if (std::abs(int(p[c]) - int(e[c])) > tolerance) {
          ++bad;
          break;
        }
    }
  return bad;
}

std::vector<uint8_t> renderTestPatternOffscreen(VulkanContext& ctx, uint32_t w, uint32_t h) {
  OffscreenTarget target(ctx, w, h, VK_FORMAT_R8G8B8A8_UNORM);
  TestPattern tp(ctx, VK_FORMAT_R8G8B8A8_UNORM);
  VkCommandBuffer cb = ctx.beginOneShot();
  target.begin(cb, {{0, 0, 0, 0}});
  tp.record(cb, w, h);
  target.endAndReadback(cb);
  ctx.submitAndWait(cb);
  std::vector<uint8_t> px(size_t(w) * h * 4);
  std::memcpy(px.data(), target.pixels(), px.size());
  return px;
}

}  // namespace hexr
