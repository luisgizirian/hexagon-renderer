#pragma once
#include <cstddef>
#include <cstdint>

#include "vk/context.hpp"

namespace hexr {

enum class VertexInput { None, Pos, PosId };
enum class BlendMode { None, Additive };

struct ShaderCode {
  const uint32_t* code;
  size_t bytes;
};
#define HEX_SPV(arr) ::hexr::ShaderCode{arr, sizeof(arr)}

struct PipelineDesc {
  ShaderCode vs, fs;
  VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
  VertexInput input = VertexInput::None;
  BlendMode blend = BlendMode::None;
  uint32_t pushConstantBytes = 0;
};

struct Pipeline {
  VkPipeline pipeline = VK_NULL_HANDLE;
  VkPipelineLayout layout = VK_NULL_HANDLE;
};

// Triangle-list pipeline for dynamic rendering, no culling, no depth, dynamic viewport/scissor.
Pipeline createPipeline(VulkanContext& ctx, const PipelineDesc& d);
void destroy(VulkanContext& ctx, Pipeline& p);

}  // namespace hexr
