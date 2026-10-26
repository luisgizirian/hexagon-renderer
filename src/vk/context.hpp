#pragma once
#include <vulkan/vulkan.h>

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "platform/json.hpp"

namespace hexr {

std::string vkResultString(VkResult r);

#define HEX_VK_CHECK(expr)                                                                             \
  do {                                                                                                 \
    VkResult hex_r_ = (expr);                                                                          \
    if (hex_r_ != VK_SUCCESS)                                                                          \
      throw std::runtime_error(std::string(#expr) + " failed: " + ::hexr::vkResultString(hex_r_));   \
  } while (0)

struct ContextOptions {
  bool validation = false;                     // also enabled by env HEX_VALIDATION=1
  std::vector<const char*> instanceExtensions; // e.g. from glfwGetRequiredInstanceExtensions
  bool swapchain = false;
};

struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;
  void* mapped = nullptr;
};

struct Image {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_UNDEFINED;
  uint32_t width = 0, height = 0;
};

class VulkanContext {
 public:
  explicit VulkanContext(const ContextOptions& opt);
  ~VulkanContext();
  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;

  VkInstance instance() const { return instance_; }
  VkPhysicalDevice physicalDevice() const { return phys_; }
  VkDevice device() const { return device_; }
  VkQueue queue() const { return queue_; }
  uint32_t queueFamily() const { return queueFamily_; }
  VkCommandPool commandPool() const { return pool_; }
  const VkPhysicalDeviceProperties& properties() const { return props_; }
  bool validationEnabled() const { return validation_; }
  uint32_t validationMessages() const { return validationMessages_.load(); }

  uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags required) const;
  // Host-visible, coherent and persistently mapped (device-local too when available, which is
  // the case for the Orin's unified memory).
  Buffer createHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
  void destroy(Buffer& b);
  Image createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage);
  void destroy(Image& img);
  bool formatSupports(VkFormat f, VkFormatFeatureFlags features) const;

  VkCommandBuffer beginOneShot();
  void submitAndWait(VkCommandBuffer cb);

  JsonObject describe() const;

 private:
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                      VkDebugUtilsMessageTypeFlagsEXT type,
                                                      const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                      void* user);
  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
  VkPhysicalDevice phys_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  uint32_t queueFamily_ = 0;
  VkCommandPool pool_ = VK_NULL_HANDLE;
  VkFence fence_ = VK_NULL_HANDLE;
  VkPhysicalDeviceProperties props_{};
  VkPhysicalDeviceMemoryProperties memProps_{};
  bool validation_ = false;
  std::atomic<uint32_t> validationMessages_{0};
  std::vector<std::string> deviceExtensions_;
};

// Image layout transition helper (classic barriers, conservative stage masks).
void transitionImage(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                     VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage,
                     VkAccessFlags dstAccess);

}  // namespace hexr
