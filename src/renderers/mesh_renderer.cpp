#include "renderers/mesh_renderer.hpp"

#include <cstring>

#include "mesh_attr_vert.h"
#include "mesh_color_frag.h"
#include "mesh_count_frag.h"
#include "mesh_id_frag.h"
#include "mesh_pos_vert.h"

namespace hexr {

namespace {
struct PushConstants {
  float scale[2];
  float offset[2];
  uint32_t idSource;
  uint32_t trisPerItem;
  uint32_t colorMode;
  uint32_t pad;
  float solidColor[4];
};
static_assert(sizeof(PushConstants) == 48);
}  // namespace

JsonObject DrawStats::toJson() const {
  JsonObject o;
  o.set("triangles", (unsigned long long)triangles)
      .set("vertices", (unsigned long long)vertices)
      .set("indices", (unsigned long long)indices)
      .set("draw_calls", (unsigned long long)drawCalls)
      .set("buffer_bytes", (unsigned long long)bufferBytes);
  return o;
}

MeshRenderer::MeshRenderer(VulkanContext& ctx, VkFormat colorFormat, OutputKind output)
    : ctx_(ctx), output_(output) {
  ShaderCode fs = output == OutputKind::Color    ? HEX_SPV(spv_mesh_color_frag)
                  : output == OutputKind::CellId ? HEX_SPV(spv_mesh_id_frag)
                                                 : HEX_SPV(spv_mesh_count_frag);
  PipelineDesc d{HEX_SPV(spv_mesh_attr_vert), fs};
  d.colorFormat = colorFormat;
  d.input = VertexInput::PosId;
  d.blend = output == OutputKind::Coverage ? BlendMode::Additive : BlendMode::None;
  d.pushConstantBytes = sizeof(PushConstants);
  attrPipe_ = createPipeline(ctx, d);
  d.vs = HEX_SPV(spv_mesh_pos_vert);
  d.input = VertexInput::Pos;
  posPipe_ = createPipeline(ctx, d);
}

MeshRenderer::~MeshRenderer() {
  vkDeviceWaitIdle(ctx_.device());
  release(fill_);
  release(outline_);
  destroy(ctx_, attrPipe_);
  destroy(ctx_, posPipe_);
}

void MeshRenderer::release(GpuMesh& g) {
  ctx_.destroy(g.positions);
  ctx_.destroy(g.ids);
  ctx_.destroy(g.indices);
  g = {};
}

void MeshRenderer::upload(const Mesh& m, GpuMesh& g) {
  release(g);
  g.positions = ctx_.createHostBuffer(m.positions.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  std::memcpy(g.positions.mapped, m.positions.data(), m.positions.size() * sizeof(float));
  g.hasIds = !m.cellIds.empty();
  if (g.hasIds) {
    g.ids = ctx_.createHostBuffer(m.cellIds.size() * sizeof(uint32_t), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    std::memcpy(g.ids.mapped, m.cellIds.data(), m.cellIds.size() * sizeof(uint32_t));
  }
  g.indices = ctx_.createHostBuffer(m.indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  std::memcpy(g.indices.mapped, m.indices.data(), m.indices.size() * sizeof(uint32_t));
  g.indexCount = uint32_t(m.indices.size());
  g.vertexCount = m.vertexCount();
  g.trisPerItem = m.trisPerItem;
  g.itemCount = m.itemCount;
}

void MeshRenderer::setScene(const Grid& grid, const Config& cfg) {
  vkDeviceWaitIdle(ctx_.device());
  cfg_ = cfg;
  stats_ = {};
  release(fill_);
  release(outline_);
  const bool wantFill = cfg.mode != DrawMode::Outline || output_ == OutputKind::CellId;
  const bool wantOutline = cfg.mode != DrawMode::Fill && output_ != OutputKind::CellId;
  const uint64_t drawsPerMesh = 1;
  if (wantFill) {
    const Mesh m = buildFillMesh(grid, cfg.mesh, cfg.layout);
    upload(m, fill_);
    stats_.triangles += m.triangleCount();
    stats_.vertices += m.vertexCount();
    stats_.indices += m.indices.size();
    stats_.bufferBytes += m.bytes();
    stats_.drawCalls += cfg.submit == Submit::PerCell ? m.itemCount : drawsPerMesh;
  }
  if (wantOutline) {
    const Mesh m = buildOutlineMesh(grid, cfg.stroke);
    upload(m, outline_);
    stats_.triangles += m.triangleCount();
    stats_.vertices += m.vertexCount();
    stats_.indices += m.indices.size();
    stats_.bufferBytes += m.bytes();
    stats_.drawCalls += cfg.submit == Submit::PerCell ? m.itemCount : drawsPerMesh;
  }
}

void MeshRenderer::drawMesh(VkCommandBuffer cb, const GpuMesh& g, const Pipeline& p, bool perItem, uint32_t idSource,
                            uint32_t colorMode, const float color[4], const float xf[4]) const {
  PushConstants pc{};
  std::memcpy(pc.scale, xf, 2 * sizeof(float));
  std::memcpy(pc.offset, xf + 2, 2 * sizeof(float));
  pc.idSource = idSource;
  pc.trisPerItem = g.trisPerItem;
  pc.colorMode = colorMode;
  std::memcpy(pc.solidColor, color, 4 * sizeof(float));
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline);
  vkCmdPushConstants(cb, p.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof pc, &pc);
  VkBuffer bufs[2] = {g.positions.buffer, g.ids.buffer};
  VkDeviceSize offs[2] = {0, 0};
  vkCmdBindVertexBuffers(cb, 0, g.hasIds && &p == &attrPipe_ ? 2 : 1, bufs, offs);
  vkCmdBindIndexBuffer(cb, g.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
  if (!perItem) {
    vkCmdDrawIndexed(cb, g.indexCount, 1, 0, 0, 0);
  } else {
    const uint32_t per = g.trisPerItem * 3;
    for (uint32_t i = 0; i < g.itemCount; ++i) vkCmdDrawIndexed(cb, per, 1, i * per, 0, i);
  }
}

void MeshRenderer::record(VkCommandBuffer cb, uint32_t w, uint32_t h, double panX, double panY) const {
  // world (pixels, y-down) -> NDC
  const float xf[4] = {float(2.0 / w), float(2.0 / h), float(-1.0 - 2.0 * panX / w), float(-1.0 - 2.0 * panY / h)};
  const bool perItem = cfg_.submit == Submit::PerCell;
  const float none[4] = {0, 0, 0, 0};
  if (fill_.indexCount) {
    const bool shared = cfg_.layout == VertexLayout::Shared;
    // Shared layout has no id attribute: batched draws derive the cell from gl_PrimitiveID,
    // per-cell draws pass it through firstInstance -> gl_InstanceIndex.
    const Pipeline& p = shared ? posPipe_ : attrPipe_;
    const uint32_t idSource = shared && !perItem ? 1u : 0u;
    drawMesh(cb, fill_, p, perItem, idSource, 0, none, xf);
  }
  if (outline_.indexCount) {
    const float black[4] = {0.05f, 0.05f, 0.05f, 1.0f};
    drawMesh(cb, outline_, attrPipe_, perItem, 0, 1, black, xf);
  }
}

}  // namespace hexr
