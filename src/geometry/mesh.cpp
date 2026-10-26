#include "geometry/mesh.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace hexr {

namespace {
struct LatticeHash {
  size_t operator()(const IVec2& v) const {
    return std::hash<uint64_t>()(uint64_t(v.x) * 0x9E3779B97F4A7C15ull ^ uint64_t(v.y));
  }
};

// Local (per-cell) triangle lists. Index 6 is the hex centre vertex.
constexpr uint32_t kRect2[] = {0, 1, 2, 0, 2, 3};
constexpr uint32_t kHex4[] = {0, 1, 2, 2, 3, 4, 4, 5, 0, 0, 2, 4};
constexpr uint32_t kHex6[] = {6, 0, 1, 6, 1, 2, 6, 2, 3, 6, 3, 4, 6, 4, 5, 6, 5, 0};
}  // namespace

bool meshMatchesShape(MeshKind k, Shape s) {
  return (k == MeshKind::Rect2) == (s == Shape::Rect);
}

MeshKind defaultMesh(Shape s) { return s == Shape::Rect ? MeshKind::Rect2 : MeshKind::Hex4; }

const char* toString(MeshKind k) {
  switch (k) {
    case MeshKind::Rect2: return "rect2";
    case MeshKind::Hex4: return "hex4";
    case MeshKind::Hex6: return "hex6";
  }
  return "?";
}
const char* toString(VertexLayout l) { return l == VertexLayout::PerCell ? "per-cell" : "shared"; }
const char* toString(Shape s) { return s == Shape::Rect ? "rect" : "hex"; }

Mesh buildFillMesh(const Grid& grid, MeshKind kind, VertexLayout layout) {
  if (!meshMatchesShape(kind, grid.shape())) throw std::invalid_argument("mesh kind does not match grid shape");
  const uint32_t* local = kind == MeshKind::Rect2 ? kRect2 : kind == MeshKind::Hex4 ? kHex4 : kHex6;
  const uint32_t localIdx = kind == MeshKind::Rect2 ? 6 : kind == MeshKind::Hex4 ? 12 : 18;
  const uint32_t corners = uint32_t(grid.cornersPerCell());
  const uint32_t localVerts = corners + (kind == MeshKind::Hex6 ? 1 : 0);
  const uint32_t cells = uint32_t(grid.cellCount());

  Mesh m;
  m.trisPerItem = localIdx / 3;
  m.itemCount = cells;
  m.indices.reserve(size_t(cells) * localIdx);

  auto lattice = [&](uint32_t cell, std::array<IVec2, 7>& out) {
    const auto c = grid.cornersLattice(int(cell));
    for (uint32_t k = 0; k < corners; ++k) out[k] = c[k];
    if (kind == MeshKind::Hex6) out[6] = grid.centerLattice(int(cell));
  };

  std::array<IVec2, 7> lv{};
  if (layout == VertexLayout::PerCell) {
    m.positions.reserve(size_t(cells) * localVerts * 2);
    m.cellIds.reserve(size_t(cells) * localVerts);
    for (uint32_t cell = 0; cell < cells; ++cell) {
      lattice(cell, lv);
      const uint32_t base = m.vertexCount();
      // Local slot order: corners 0..n-1, then centre (slot 6 maps to vertex `corners`).
      for (uint32_t k = 0; k < localVerts; ++k) {
        const IVec2 p = (k < corners) ? lv[k] : lv[6];
        const Vec2 w = grid.toWorld(p);
        m.positions.push_back(float(w.x));
        m.positions.push_back(float(w.y));
        m.cellIds.push_back(cell);
      }
      for (uint32_t i = 0; i < localIdx; ++i)
        m.indices.push_back(base + (local[i] == 6 ? corners : local[i]));
    }
  } else {
    std::unordered_map<IVec2, uint32_t, LatticeHash> lookup;
    lookup.reserve(size_t(cells) * (kind == MeshKind::Hex6 ? 3 : 2) + 64);
    auto vid = [&](IVec2 p) {
      auto [it, inserted] = lookup.try_emplace(p, uint32_t(lookup.size()));
      if (inserted) {
        const Vec2 w = grid.toWorld(p);
        m.positions.push_back(float(w.x));
        m.positions.push_back(float(w.y));
      }
      return it->second;
    };
    for (uint32_t cell = 0; cell < cells; ++cell) {
      lattice(cell, lv);
      uint32_t ids[7];
      for (uint32_t k = 0; k < corners; ++k) ids[k] = vid(lv[k]);
      if (kind == MeshKind::Hex6) ids[6] = vid(lv[6]);
      for (uint32_t i = 0; i < localIdx; ++i) m.indices.push_back(ids[local[i]]);
    }
  }
  return m;
}

Mesh buildOutlineMesh(const Grid& grid, double strokeWidth) {
  if (!(strokeWidth > 0)) throw std::invalid_argument("stroke width must be positive");
  const auto edges = grid.uniqueEdges();
  Mesh m;
  m.trisPerItem = 2;
  m.itemCount = uint32_t(edges.size());
  m.positions.reserve(edges.size() * 8);
  m.cellIds.reserve(edges.size() * 4);
  m.indices.reserve(edges.size() * 6);
  const double h = strokeWidth / 2.0;
  for (uint32_t e = 0; e < edges.size(); ++e) {
    const Vec2 a = grid.toWorld(edges[e].a), b = grid.toWorld(edges[e].b);
    const double dx = b.x - a.x, dy = b.y - a.y, len = std::hypot(dx, dy);
    const double nx = -dy / len * h, ny = dx / len * h;
    const Vec2 q[4] = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
    const uint32_t base = m.vertexCount();
    for (const auto& p : q) {
      m.positions.push_back(float(p.x));
      m.positions.push_back(float(p.y));
      m.cellIds.push_back(e);
    }
    for (uint32_t i : kRect2) m.indices.push_back(base + i);
  }
  return m;
}

}  // namespace hexr
