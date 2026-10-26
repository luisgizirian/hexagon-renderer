#include "vk/offscreen.hpp"

#include <cstdio>

namespace hexr {

uint32_t formatBytesPerPixel(VkFormat f) {
  switch (f) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SFLOAT: return 4;
    case VK_FORMAT_R16_SFLOAT: return 2;
    default: throw std::runtime_error("unsupported offscreen format");
  }
}

OffscreenTarget::OffscreenTarget(VulkanContext& ctx, uint32_t width, uint32_t height, VkFormat format)
    : ctx_(ctx) {
  if (width == 0 || height == 0 || width > ctx.properties().limits.maxImageDimension2D ||
      height > ctx.properties().limits.maxImageDimension2D)
    throw std::runtime_error("invalid offscreen target size");
  image_ = ctx.createImage(width, height, format,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  readback_ = ctx.createHostBuffer(VkDeviceSize(width) * height * formatBytesPerPixel(format),
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

OffscreenTarget::~OffscreenTarget() {
  vkDeviceWaitIdle(ctx_.device());
  ctx_.destroy(readback_);
  ctx_.destroy(image_);
}

void beginDynamicRendering(VkCommandBuffer cb, VkImageView view, uint32_t w, uint32_t h, VkClearColorValue clear) {
  VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
  att.imageView = view;
  att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  att.clearValue.color = clear;
  VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
  ri.renderArea = {{0, 0}, {w, h}};
  ri.layerCount = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments = &att;
  vkCmdBeginRendering(cb, &ri);
  VkViewport vp{0, 0, float(w), float(h), 0, 1};
  VkRect2D sc{{0, 0}, {w, h}};
  vkCmdSetViewport(cb, 0, 1, &vp);
  vkCmdSetScissor(cb, 0, 1, &sc);
}

void OffscreenTarget::begin(VkCommandBuffer cb, VkClearColorValue clear) {
  transitionImage(cb, image_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  beginDynamicRendering(cb, image_.view, image_.width, image_.height, clear);
}

void OffscreenTarget::endAndReadback(VkCommandBuffer cb) {
  vkCmdEndRendering(cb);
  transitionImage(cb, image_.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT);
  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {image_.width, image_.height, 1};
  vkCmdCopyImageToBuffer(cb, image_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_.buffer, 1, &copy);
  VkBufferMemoryBarrier bb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
  bb.srcQueueFamilyIndex = bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  bb.buffer = readback_.buffer;
  bb.size = VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bb, 0,
                       nullptr);
}

bool writePPM(const std::string& path, const uint8_t* rgba, uint32_t w, uint32_t h) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  std::fprintf(f, "P6\n%u %u\n255\n", w, h);
  for (size_t i = 0; i < size_t(w) * h; ++i) std::fwrite(rgba + i * 4, 1, 3, f);
  return std::fclose(f) == 0;
}

}  // namespace hexr
