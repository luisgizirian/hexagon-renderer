#include "vk/pipeline.hpp"

namespace hexr {

namespace {
VkShaderModule makeModule(VkDevice dev, ShaderCode c) {
  VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  ci.codeSize = c.bytes;
  ci.pCode = c.code;
  VkShaderModule m;
  HEX_VK_CHECK(vkCreateShaderModule(dev, &ci, nullptr, &m));
  return m;
}
}  // namespace

Pipeline createPipeline(VulkanContext& ctx, const PipelineDesc& d) {
  VkDevice dev = ctx.device();
  Pipeline p;
  VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, d.pushConstantBytes};
  VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  lci.pushConstantRangeCount = d.pushConstantBytes ? 1 : 0;
  lci.pPushConstantRanges = &pcr;
  HEX_VK_CHECK(vkCreatePipelineLayout(dev, &lci, nullptr, &p.layout));

  VkShaderModule vs = makeModule(dev, d.vs), fs = makeModule(dev, d.fs);
  VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                               {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vs;
  stages[0].pName = "main";
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fs;
  stages[1].pName = "main";

  VkVertexInputBindingDescription bindings[2] = {{0, 8, VK_VERTEX_INPUT_RATE_VERTEX},
                                                 {1, 4, VK_VERTEX_INPUT_RATE_VERTEX}};
  VkVertexInputAttributeDescription attrs[2] = {{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
                                                {1, 1, VK_FORMAT_R32_UINT, 0}};
  VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  const uint32_t n = d.input == VertexInput::None ? 0 : d.input == VertexInput::Pos ? 1 : 2;
  vi.vertexBindingDescriptionCount = n;
  vi.pVertexBindingDescriptions = bindings;
  vi.vertexAttributeDescriptionCount = n;
  vi.pVertexAttributeDescriptions = attrs;

  VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
  vp.viewportCount = 1;
  vp.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_NONE;
  rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rs.lineWidth = 1.0f;
  VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  VkPipelineColorBlendAttachmentState att{};
  att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                       VK_COLOR_COMPONENT_A_BIT;
  if (d.blend == BlendMode::Additive) {
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.srcAlphaBlendFactor = att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.colorBlendOp = att.alphaBlendOp = VK_BLEND_OP_ADD;
  }
  VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
  cb.attachmentCount = 1;
  cb.pAttachments = &att;
  VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
  ds.dynamicStateCount = 2;
  ds.pDynamicStates = dyn;
  VkPipelineRenderingCreateInfo rci{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  rci.colorAttachmentCount = 1;
  rci.pColorAttachmentFormats = &d.colorFormat;

  VkGraphicsPipelineCreateInfo gci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
  gci.pNext = &rci;
  gci.stageCount = 2;
  gci.pStages = stages;
  gci.pVertexInputState = &vi;
  gci.pInputAssemblyState = &ia;
  gci.pViewportState = &vp;
  gci.pRasterizationState = &rs;
  gci.pMultisampleState = &ms;
  gci.pColorBlendState = &cb;
  gci.pDynamicState = &ds;
  gci.layout = p.layout;
  const VkResult r = vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gci, nullptr, &p.pipeline);
  vkDestroyShaderModule(dev, vs, nullptr);
  vkDestroyShaderModule(dev, fs, nullptr);
  HEX_VK_CHECK(r);
  return p;
}

void destroy(VulkanContext& ctx, Pipeline& p) {
  if (p.pipeline) vkDestroyPipeline(ctx.device(), p.pipeline, nullptr);
  if (p.layout) vkDestroyPipelineLayout(ctx.device(), p.layout, nullptr);
  p = {};
}

}  // namespace hexr
