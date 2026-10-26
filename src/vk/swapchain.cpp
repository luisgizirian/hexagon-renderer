#include "vk/swapchain.hpp"

#include <algorithm>

#include "vk/offscreen.hpp"

namespace hexr {

Swapchain::Swapchain(VulkanContext& ctx, VkSurfaceKHR surface, uint32_t width, uint32_t height)
    : ctx_(ctx), surface_(surface) {
  VkBool32 present = VK_FALSE;
  HEX_VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(ctx.physicalDevice(), ctx.queueFamily(), surface, &present));
  if (!present) throw std::runtime_error("graphics queue family cannot present to this surface");

  VkDevice dev = ctx.device();
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = ctx.commandPool();
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = kFrames;
  HEX_VK_CHECK(vkAllocateCommandBuffers(dev, &ai, cmd_));
  for (uint32_t i = 0; i < kFrames; ++i) {
    HEX_VK_CHECK(vkCreateSemaphore(dev, &sci, nullptr, &acquired_[i]));
    HEX_VK_CHECK(vkCreateFence(dev, &fci, nullptr, &inFlight_[i]));
  }
  create(width, height);
}

Swapchain::~Swapchain() {
  VkDevice dev = ctx_.device();
  vkDeviceWaitIdle(dev);
  cleanup();
  if (swapchain_) vkDestroySwapchainKHR(dev, swapchain_, nullptr);
  for (uint32_t i = 0; i < kFrames; ++i) {
    vkDestroySemaphore(dev, acquired_[i], nullptr);
    vkDestroyFence(dev, inFlight_[i], nullptr);
  }
  vkFreeCommandBuffers(dev, ctx_.commandPool(), kFrames, cmd_);
}

void Swapchain::cleanup() {
  VkDevice dev = ctx_.device();
  for (VkImageView v : views_) vkDestroyImageView(dev, v, nullptr);
  for (VkSemaphore s : renderDone_) vkDestroySemaphore(dev, s, nullptr);
  views_.clear();
  renderDone_.clear();
  images_.clear();
}

void Swapchain::create(uint32_t width, uint32_t height) {
  VkDevice dev = ctx_.device();
  VkPhysicalDevice pd = ctx_.physicalDevice();
  vkDeviceWaitIdle(dev);
  cleanup();
  requestedW_ = width;
  requestedH_ = height;

  VkSurfaceCapabilitiesKHR caps;
  HEX_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface_, &caps));
  uint32_t n = 0;
  HEX_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface_, &n, nullptr));
  std::vector<VkSurfaceFormatKHR> formats(n);
  HEX_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface_, &n, formats.data()));
  // UNORM keeps the palette identical to the offscreen RGBA8 output.
  VkSurfaceFormatKHR chosen = formats.at(0);
  for (const auto& f : formats)
    if ((f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) &&
        f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosen = f;
      break;
    }
  format_ = chosen.format;

  if (caps.currentExtent.width != UINT32_MAX) {
    extent_ = caps.currentExtent;
  } else {
    extent_.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent_.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
  }
  uint32_t count = caps.minImageCount + 1;
  if (caps.maxImageCount) count = std::min(count, caps.maxImageCount);

  VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ci.surface = surface_;
  ci.minImageCount = count;
  ci.imageFormat = chosen.format;
  ci.imageColorSpace = chosen.colorSpace;
  ci.imageExtent = extent_;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;  // always supported; vsync-limited (viewer only)
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = swapchain_;
  VkSwapchainKHR next;
  HEX_VK_CHECK(vkCreateSwapchainKHR(dev, &ci, nullptr, &next));
  if (swapchain_) vkDestroySwapchainKHR(dev, swapchain_, nullptr);
  swapchain_ = next;

  HEX_VK_CHECK(vkGetSwapchainImagesKHR(dev, swapchain_, &n, nullptr));
  images_.resize(n);
  HEX_VK_CHECK(vkGetSwapchainImagesKHR(dev, swapchain_, &n, images_.data()));
  VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  for (VkImage img : images_) {
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = img;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format_;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView v;
    HEX_VK_CHECK(vkCreateImageView(dev, &vi, nullptr, &v));
    views_.push_back(v);
    VkSemaphore s;
    HEX_VK_CHECK(vkCreateSemaphore(dev, &sci, nullptr, &s));
    renderDone_.push_back(s);
  }
}

VkCommandBuffer Swapchain::beginFrame(VkClearColorValue clear, uint32_t width, uint32_t height) {
  VkDevice dev = ctx_.device();
  HEX_VK_CHECK(vkWaitForFences(dev, 1, &inFlight_[frame_], VK_TRUE, UINT64_MAX));
  if (width == 0 || height == 0) return VK_NULL_HANDLE;  // minimised
  const VkResult r = vkAcquireNextImageKHR(dev, swapchain_, UINT64_MAX, acquired_[frame_], VK_NULL_HANDLE, &image_);
  if (r == VK_ERROR_OUT_OF_DATE_KHR) {
    create(width, height);
    return VK_NULL_HANDLE;
  }
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) HEX_VK_CHECK(r);
  HEX_VK_CHECK(vkResetFences(dev, 1, &inFlight_[frame_]));

  VkCommandBuffer cb = cmd_[frame_];
  HEX_VK_CHECK(vkResetCommandBuffer(cb, 0));
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  HEX_VK_CHECK(vkBeginCommandBuffer(cb, &bi));
  transitionImage(cb, images_[image_], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
  beginDynamicRendering(cb, views_[image_], extent_.width, extent_.height, clear);
  return cb;
}

void Swapchain::endFrame(uint32_t width, uint32_t height) {
  VkCommandBuffer cb = cmd_[frame_];
  vkCmdEndRendering(cb);
  transitionImage(cb, images_[image_], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
  HEX_VK_CHECK(vkEndCommandBuffer(cb));

  const VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &acquired_[frame_];
  si.pWaitDstStageMask = &wait;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &renderDone_[image_];
  HEX_VK_CHECK(vkQueueSubmit(ctx_.queue(), 1, &si, inFlight_[frame_]));

  VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &renderDone_[image_];
  pi.swapchainCount = 1;
  pi.pSwapchains = &swapchain_;
  pi.pImageIndices = &image_;
  const VkResult r = vkQueuePresentKHR(ctx_.queue(), &pi);
  frame_ = (frame_ + 1) % kFrames;
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || width != requestedW_ || height != requestedH_)
    create(width, height);
  else if (r != VK_SUCCESS)
    HEX_VK_CHECK(r);
}

}  // namespace hexr
