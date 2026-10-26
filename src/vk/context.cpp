#include "vk/context.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace hexr {

std::string vkResultString(VkResult r) {
  switch (r) {
#define C(x) case x: return #x
    C(VK_SUCCESS); C(VK_NOT_READY); C(VK_TIMEOUT); C(VK_INCOMPLETE);
    C(VK_ERROR_OUT_OF_HOST_MEMORY); C(VK_ERROR_OUT_OF_DEVICE_MEMORY); C(VK_ERROR_INITIALIZATION_FAILED);
    C(VK_ERROR_DEVICE_LOST); C(VK_ERROR_MEMORY_MAP_FAILED); C(VK_ERROR_LAYER_NOT_PRESENT);
    C(VK_ERROR_EXTENSION_NOT_PRESENT); C(VK_ERROR_FEATURE_NOT_PRESENT); C(VK_ERROR_INCOMPATIBLE_DRIVER);
    C(VK_ERROR_FORMAT_NOT_SUPPORTED); C(VK_ERROR_SURFACE_LOST_KHR); C(VK_SUBOPTIMAL_KHR);
    C(VK_ERROR_OUT_OF_DATE_KHR);
#undef C
    default: return "VkResult(" + std::to_string(int(r)) + ")";
  }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                            VkDebugUtilsMessageTypeFlagsEXT,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* data,
                                                            void* user) {
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    static_cast<VulkanContext*>(user)->validationMessages_++;
    std::fprintf(stderr, "[vulkan %s] %s\n",
                 severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? "error" : "warning",
                 data->pMessage);
  }
  return VK_FALSE;
}

VulkanContext::VulkanContext(const ContextOptions& opt) {
  const char* env = std::getenv("HEX_VALIDATION");
  bool wantValidation = opt.validation || (env && std::strcmp(env, "0") != 0);

  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> layers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
  bool haveValidation = false;
  for (auto& l : layers)
    if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) haveValidation = true;
  if (wantValidation && !haveValidation)
    std::fprintf(stderr, "warning: validation requested but VK_LAYER_KHRONOS_validation not found "
                         "(set VK_LAYER_PATH, see docs/ENVIRONMENT.md)\n");
  validation_ = wantValidation && haveValidation;

  std::vector<const char*> exts = opt.instanceExtensions;
  std::vector<const char*> layerNames;
  if (validation_) {
    exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    layerNames.push_back("VK_LAYER_KHRONOS_validation");
  }

  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "hexagon-renderer";
  app.apiVersion = VK_API_VERSION_1_3;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;
  ici.enabledExtensionCount = uint32_t(exts.size());
  ici.ppEnabledExtensionNames = exts.data();
  ici.enabledLayerCount = uint32_t(layerNames.size());
  ici.ppEnabledLayerNames = layerNames.data();
  HEX_VK_CHECK(vkCreateInstance(&ici, nullptr, &instance_));

  if (validation_) {
    VkDebugUtilsMessengerCreateInfoEXT dci{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dci.pfnUserCallback = debugCallback;
    dci.pUserData = this;
    auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    if (create) HEX_VK_CHECK(create(instance_, &dci, nullptr, &messenger_));
  }

  uint32_t devCount = 0;
  vkEnumeratePhysicalDevices(instance_, &devCount, nullptr);
  if (devCount == 0) throw std::runtime_error("no Vulkan physical devices");
  std::vector<VkPhysicalDevice> devs(devCount);
  vkEnumeratePhysicalDevices(instance_, &devCount, devs.data());
  phys_ = devs[0];
  vkGetPhysicalDeviceProperties(phys_, &props_);
  vkGetPhysicalDeviceMemoryProperties(phys_, &memProps_);
  if (VK_API_VERSION_MINOR(props_.apiVersion) < 3 && VK_API_VERSION_MAJOR(props_.apiVersion) == 1)
    throw std::runtime_error("Vulkan 1.3 device required");

  uint32_t qfCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qfCount, nullptr);
  std::vector<VkQueueFamilyProperties> qfs(qfCount);
  vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qfCount, qfs.data());
  bool found = false;
  for (uint32_t i = 0; i < qfCount; ++i)
    if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queueFamily_ = i;
      found = true;
      break;
    }
  if (!found) throw std::runtime_error("no graphics queue family");

  VkPhysicalDeviceFeatures supported{};
  vkGetPhysicalDeviceFeatures(phys_, &supported);
  if (!supported.geometryShader)
    throw std::runtime_error("geometryShader feature (needed for gl_PrimitiveID in fragment shaders) unsupported");

  float prio = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = queueFamily_;
  qci.queueCount = 1;
  qci.pQueuePriorities = &prio;

  VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  f13.dynamicRendering = VK_TRUE;
  VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  f12.pNext = &f13;
  f12.hostQueryReset = VK_TRUE;
  VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
  features.pNext = &f12;
  features.features.geometryShader = VK_TRUE;

  std::vector<const char*> devExts;
  if (opt.swapchain) devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  for (auto* e : devExts) deviceExtensions_.emplace_back(e);

  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.pNext = &features;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.enabledExtensionCount = uint32_t(devExts.size());
  dci.ppEnabledExtensionNames = devExts.data();
  HEX_VK_CHECK(vkCreateDevice(phys_, &dci, nullptr, &device_));
  vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);

  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pci.queueFamilyIndex = queueFamily_;
  HEX_VK_CHECK(vkCreateCommandPool(device_, &pci, nullptr, &pool_));
  VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  HEX_VK_CHECK(vkCreateFence(device_, &fci, nullptr, &fence_));
}

VulkanContext::~VulkanContext() {
  if (device_) {
    vkDeviceWaitIdle(device_);
    vkDestroyFence(device_, fence_, nullptr);
    vkDestroyCommandPool(device_, pool_, nullptr);
    vkDestroyDevice(device_, nullptr);
  }
  if (messenger_) {
    auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    if (destroy) destroy(instance_, messenger_, nullptr);
  }
  if (instance_) vkDestroyInstance(instance_, nullptr);
}

uint32_t VulkanContext::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags required) const {
  for (uint32_t i = 0; i < memProps_.memoryTypeCount; ++i)
    if ((typeBits & (1u << i)) && (memProps_.memoryTypes[i].propertyFlags & required) == required) return i;
  return UINT32_MAX;
}

Buffer VulkanContext::createHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
  Buffer b;
  b.size = size;
  VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bci.size = size ? size : 4;
  bci.usage = usage;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  HEX_VK_CHECK(vkCreateBuffer(device_, &bci, nullptr, &b.buffer));
  VkMemoryRequirements req;
  vkGetBufferMemoryRequirements(device_, b.buffer, &req);
  const VkMemoryPropertyFlags hv = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  uint32_t type = findMemoryType(req.memoryTypeBits, hv | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (type == UINT32_MAX) type = findMemoryType(req.memoryTypeBits, hv);
  if (type == UINT32_MAX) throw std::runtime_error("no host-visible coherent memory type");
  VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = type;
  HEX_VK_CHECK(vkAllocateMemory(device_, &mai, nullptr, &b.memory));
  HEX_VK_CHECK(vkBindBufferMemory(device_, b.buffer, b.memory, 0));
  HEX_VK_CHECK(vkMapMemory(device_, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped));
  return b;
}

void VulkanContext::destroy(Buffer& b) {
  if (b.buffer) vkDestroyBuffer(device_, b.buffer, nullptr);
  if (b.memory) vkFreeMemory(device_, b.memory, nullptr);
  b = {};
}

Image VulkanContext::createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage) {
  Image img;
  img.format = format;
  img.width = w;
  img.height = h;
  VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.format = format;
  ici.extent = {w, h, 1};
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;
  ici.tiling = VK_IMAGE_TILING_OPTIMAL;
  ici.usage = usage;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  HEX_VK_CHECK(vkCreateImage(device_, &ici, nullptr, &img.image));
  VkMemoryRequirements req;
  vkGetImageMemoryRequirements(device_, img.image, &req);
  VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (mai.memoryTypeIndex == UINT32_MAX) throw std::runtime_error("no device-local memory for image");
  HEX_VK_CHECK(vkAllocateMemory(device_, &mai, nullptr, &img.memory));
  HEX_VK_CHECK(vkBindImageMemory(device_, img.image, img.memory, 0));
  VkImageViewCreateInfo vci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vci.image = img.image;
  vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vci.format = format;
  vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  HEX_VK_CHECK(vkCreateImageView(device_, &vci, nullptr, &img.view));
  return img;
}

void VulkanContext::destroy(Image& img) {
  if (img.view) vkDestroyImageView(device_, img.view, nullptr);
  if (img.image) vkDestroyImage(device_, img.image, nullptr);
  if (img.memory) vkFreeMemory(device_, img.memory, nullptr);
  img = {};
}

bool VulkanContext::formatSupports(VkFormat f, VkFormatFeatureFlags features) const {
  VkFormatProperties p;
  vkGetPhysicalDeviceFormatProperties(phys_, f, &p);
  return (p.optimalTilingFeatures & features) == features;
}

VkCommandBuffer VulkanContext::beginOneShot() {
  VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool = pool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cb;
  HEX_VK_CHECK(vkAllocateCommandBuffers(device_, &ai, &cb));
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  HEX_VK_CHECK(vkBeginCommandBuffer(cb, &bi));
  return cb;
}

void VulkanContext::submitAndWait(VkCommandBuffer cb) {
  HEX_VK_CHECK(vkEndCommandBuffer(cb));
  VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  HEX_VK_CHECK(vkResetFences(device_, 1, &fence_));
  HEX_VK_CHECK(vkQueueSubmit(queue_, 1, &si, fence_));
  HEX_VK_CHECK(vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX));
  vkFreeCommandBuffers(device_, pool_, 1, &cb);
}

JsonObject VulkanContext::describe() const {
  JsonObject o;
  auto ver = [](uint32_t v) {
    return std::to_string(VK_API_VERSION_MAJOR(v)) + "." + std::to_string(VK_API_VERSION_MINOR(v)) + "." +
           std::to_string(VK_API_VERSION_PATCH(v));
  };
  uint32_t instVer = 0;
  vkEnumerateInstanceVersion(&instVer);
  o.set("loader_version", ver(instVer));
  o.set("device_name", std::string(props_.deviceName));
  o.set("device_api_version", ver(props_.apiVersion));
  o.set("driver_version_raw", (unsigned long long)props_.driverVersion);
  o.set("vendor_id", (unsigned long long)props_.vendorID);
  o.set("device_type", props_.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated"
                       : props_.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete"
                                                                                  : "other");
  o.set("timestamp_period_ns", double(props_.limits.timestampPeriod));
  o.set("timestamp_compute_and_graphics", bool(props_.limits.timestampComputeAndGraphics));
  o.set("max_image_dimension_2d", (unsigned long long)props_.limits.maxImageDimension2D);
  o.set("sub_pixel_precision_bits", (unsigned long long)props_.limits.subPixelPrecisionBits);
  o.set("validation_layers", validation_);
  VkDeviceSize heapTotal = 0;
  for (uint32_t i = 0; i < memProps_.memoryHeapCount; ++i)
    if (memProps_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) heapTotal += memProps_.memoryHeaps[i].size;
  o.set("device_local_heap_bytes", (unsigned long long)heapTotal);
  return o;
}

void transitionImage(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to,
                     VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage,
                     VkAccessFlags dstAccess) {
  VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  b.oldLayout = from;
  b.newLayout = to;
  b.srcAccessMask = srcAccess;
  b.dstAccessMask = dstAccess;
  b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image = image;
  b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

}  // namespace hexr
