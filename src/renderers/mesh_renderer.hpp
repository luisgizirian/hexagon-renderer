#pragma once
// Explicit-geometry grid renderer: Strategy A (rect baseline) and Strategy B (explicit hexagon
// triangles: hex4 / hex6, per-cell or shared vertices, batched or per-cell draws).
#include <cstdint>

#include "geometry/mesh.hpp"
#include "platform/cli.hpp"
#include "platform/json.hpp"
#include "vk/pipeline.hpp"

namespace hexr {

enum class OutputKind {
  Color,     // RGBA8 palette colours / solid outline colour
  CellId,    // R32_UINT, item id + 1 (0 = background); fill only
  Coverage,  // R32_SFLOAT additive count of covering fragments
};

struct DrawStats {
  uint64_t triangles = 0;
  uint64_t vertices = 0;
  uint64_t indices = 0;
  uint64_t drawCalls = 0;
  uint64_t bufferBytes = 0;
  JsonObject toJson() const;
};

class MeshRenderer {
 public:
  MeshRenderer(VulkanContext& ctx, VkFormat colorFormat, OutputKind output);
  ~MeshRenderer();
  MeshRenderer(const MeshRenderer&) = delete;
  MeshRenderer& operator=(const MeshRenderer&) = delete;

  // Builds and uploads the meshes for `grid` according to cfg.mesh / layout / mode / stroke.
  void setScene(const Grid& grid, const Config& cfg);
  // Records draws inside an active dynamic-rendering scope covering a w x h target.
  // World units map 1:1 to pixels, with (panX, panY) world offset.
  void record(VkCommandBuffer cb, uint32_t w, uint32_t h, double panX = 0, double panY = 0) const;
  const DrawStats& stats() const { return stats_; }

 private:
  struct GpuMesh {
    Buffer positions, ids, indices;
    uint32_t indexCount = 0, vertexCount = 0, trisPerItem = 0, itemCount = 0;
    bool hasIds = false;
  };
  void upload(const Mesh& m, GpuMesh& g);
  void release(GpuMesh& g);
  void drawMesh(VkCommandBuffer cb, const GpuMesh& g, const Pipeline& p, bool perItem, uint32_t idSource,
                uint32_t colorMode, const float color[4], const float xf[4]) const;

  VulkanContext& ctx_;
  OutputKind output_;
  Pipeline attrPipe_, posPipe_;
  GpuMesh fill_, outline_;
  Config cfg_;
  DrawStats stats_;
};

}  // namespace hexr
