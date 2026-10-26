#pragma once
#include <string>

#include "vk/context.hpp"

namespace hexr {

uint32_t formatBytesPerPixel(VkFormat f);

// Offscreen colour target rendered with dynamic rendering and read back to a host buffer.
class OffscreenTarget {
 public:
  OffscreenTarget(VulkanContext& ctx, uint32_t width, uint32_t height, VkFormat format);
  ~OffscreenTarget();
  OffscreenTarget(const OffscreenTarget&) = delete;
  OffscreenTarget& operator=(const OffscreenTarget&) = delete;

  // Transitions the image, begins rendering with a clear, and sets a full-target viewport/scissor.
  void begin(VkCommandBuffer cb, VkClearColorValue clear);
  // Ends rendering and records a copy into the host-visible readback buffer.
  void endAndReadback(VkCommandBuffer cb);

  const void* pixels() const { return readback_.mapped; }
  uint32_t width() const { return image_.width; }
  uint32_t height() const { return image_.height; }
  VkFormat format() const { return image_.format; }
  uint32_t bytesPerPixel() const { return formatBytesPerPixel(image_.format); }

 private:
  VulkanContext& ctx_;
  Image image_;
  Buffer readback_;
};

void beginDynamicRendering(VkCommandBuffer cb, VkImageView view, uint32_t w, uint32_t h, VkClearColorValue clear);

bool writePPM(const std::string& path, const uint8_t* rgba, uint32_t w, uint32_t h);

}  // namespace hexr
