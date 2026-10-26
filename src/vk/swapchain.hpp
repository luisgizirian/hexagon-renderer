// Minimal FIFO swapchain for the windowed viewer (dynamic rendering, 2 frames in flight).
#pragma once

#include <vector>

#include "vk/context.hpp"

namespace hexr {

class Swapchain {
 public:
  Swapchain(VulkanContext& ctx, VkSurfaceKHR surface, uint32_t width, uint32_t height);
  ~Swapchain();
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  VkFormat format() const { return format_; }
  VkExtent2D extent() const { return extent_; }

  // Returns a command buffer recording into the acquired image (already transitioned and inside
  // dynamic rendering with the given clear colour), or VK_NULL_HANDLE if the swapchain had to be
  // recreated (skip the frame).
  VkCommandBuffer beginFrame(VkClearColorValue clear, uint32_t width, uint32_t height);
  void endFrame(uint32_t width, uint32_t height);

 private:
  void create(uint32_t width, uint32_t height);
  void cleanup();

  static constexpr uint32_t kFrames = 2;
  VulkanContext& ctx_;
  VkSurfaceKHR surface_;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
  std::vector<VkImage> images_;
  std::vector<VkImageView> views_;
  std::vector<VkSemaphore> renderDone_;  // one per swapchain image
  VkSemaphore acquired_[kFrames]{};
  VkFence inFlight_[kFrames]{};
  VkCommandBuffer cmd_[kFrames]{};
  uint32_t frame_ = 0, image_ = 0;
  uint32_t requestedW_ = 0, requestedH_ = 0;
};

}  // namespace hexr
