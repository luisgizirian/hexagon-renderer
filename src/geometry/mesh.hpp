#pragma once
// Triangle mesh builders for grid cells.
#include <cstdint>
#include <string>
#include <vector>

#include "geometry/grid.hpp"

namespace hexr {

enum class MeshKind {
  Rect2,  // square as 2 triangles
  Hex4,   // hexagon as 4 triangles, boundary vertices only: (0,1,2) (2,3,4) (4,5,0) (0,2,4)
  Hex6,   // hexagon as a 6-triangle fan around an added centre vertex
};

enum class VertexLayout {
  PerCell,  // every cell has its own vertices + a per-vertex cell id attribute
  Shared,   // vertices deduplicated across cells (position only); cell id from gl_PrimitiveID
};

struct Mesh {
  std::vector<float> positions;    // xy pairs, world units
  std::vector<uint32_t> cellIds;   // one per vertex (PerCell layout only)
  std::vector<uint32_t> indices;   // triangle list
  uint32_t trisPerItem = 0;        // triangles per cell (fill) or per edge (outline)
  uint32_t itemCount = 0;          // cells or edges
  uint32_t vertexCount() const { return uint32_t(positions.size() / 2); }
  uint32_t triangleCount() const { return uint32_t(indices.size() / 3); }
  size_t bytes() const {
    return positions.size() * sizeof(float) + cellIds.size() * sizeof(uint32_t) +
           indices.size() * sizeof(uint32_t);
  }
};

bool meshMatchesShape(MeshKind k, Shape s);
MeshKind defaultMesh(Shape s);
const char* toString(MeshKind k);
const char* toString(VertexLayout l);
const char* toString(Shape s);

// Solid cell interiors. Indices are contiguous per cell (cell i owns triangles
// [i*trisPerItem, (i+1)*trisPerItem)), so gl_PrimitiveID / trisPerItem == cell id.
Mesh buildFillMesh(const Grid& grid, MeshKind kind, VertexLayout layout);

// Outline strokes: one quad (2 triangles) per unique edge, centred on the edge,
// width `strokeWidth` world units, butt caps. Shared edges are drawn exactly once.
// Uses the PerCell layout with the edge index as the id attribute.
Mesh buildOutlineMesh(const Grid& grid, double strokeWidth);

}  // namespace hexr
