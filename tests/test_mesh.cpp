#include <cmath>

#include "doctest.h"
#include "geometry/mesh.hpp"
#include "ink_model/ink_model.hpp"

using namespace hexr;

namespace {
double signedArea(const Mesh& m, uint32_t tri) {
  const uint32_t i0 = m.indices[tri * 3], i1 = m.indices[tri * 3 + 1], i2 = m.indices[tri * 3 + 2];
  const double ax = m.positions[i0 * 2], ay = m.positions[i0 * 2 + 1];
  const double bx = m.positions[i1 * 2], by = m.positions[i1 * 2 + 1];
  const double cx = m.positions[i2 * 2], cy = m.positions[i2 * 2 + 1];
  return 0.5 * ((bx - ax) * (cy - ay) - (cx - ax) * (by - ay));
}
}  // namespace

TEST_CASE("mesh kind must match shape") {
  Grid h(Shape::Hex, 2, 2, 1.0), r(Shape::Rect, 2, 2, 1.0);
  CHECK_THROWS(buildFillMesh(h, MeshKind::Rect2, VertexLayout::PerCell));
  CHECK_THROWS(buildFillMesh(r, MeshKind::Hex4, VertexLayout::PerCell));
  CHECK((defaultMesh(Shape::Hex) == MeshKind::Hex4));
  CHECK((defaultMesh(Shape::Rect) == MeshKind::Rect2));
}

TEST_CASE("fill mesh counts, index validity, winding, area and cell ordering") {
  struct Case { Shape s; MeshKind k; uint32_t tris, verts; };
  for (Case c : {Case{Shape::Rect, MeshKind::Rect2, 2, 4}, Case{Shape::Hex, MeshKind::Hex4, 4, 6},
                 Case{Shape::Hex, MeshKind::Hex6, 6, 7}})
    for (VertexLayout layout : {VertexLayout::PerCell, VertexLayout::Shared}) {
      CAPTURE(toString(c.k));
      CAPTURE(toString(layout));
      Grid g(c.s, 9, 7, 3.0);
      const Mesh m = buildFillMesh(g, c.k, layout);
      const uint32_t N = uint32_t(g.cellCount());
      CHECK(m.trisPerItem == c.tris);
      CHECK(m.itemCount == N);
      CHECK(m.triangleCount() == N * c.tris);
      if (layout == VertexLayout::PerCell) {
        CHECK(m.vertexCount() == N * c.verts);
        CHECK(m.cellIds.size() == m.vertexCount());
      } else {
        CHECK(m.cellIds.empty());
        // Unique corner count from Euler (V = E - F + 1), plus one centre per cell for hex6.
        const LayoutMetrics lm = computeMetrics(g, {});
        CHECK(m.vertexCount() == uint32_t(lm.joints) + (c.k == MeshKind::Hex6 ? N : 0));
      }
      for (uint32_t idx : m.indices) CHECK(idx < m.vertexCount());
      double total = 0;
      for (uint32_t cell = 0; cell < N; ++cell) {
        double cellArea = 0;
        for (uint32_t t = cell * m.trisPerItem; t < (cell + 1) * m.trisPerItem; ++t) {
          const double a = signedArea(m, t);
          CHECK(a > 1e-9);  // non-degenerate, consistent clockwise-on-screen winding (y-down)
          cellArea += a;
          // Triangle centroid lies in the owning cell: required for gl_PrimitiveID / trisPerItem.
          const uint32_t* tri = &m.indices[t * 3];
          Vec2 ctr{0, 0};
          for (int v = 0; v < 3; ++v) {
            ctr.x += m.positions[tri[v] * 2] / 3.0;
            ctr.y += m.positions[tri[v] * 2 + 1] / 3.0;
          }
          CHECK(g.cellAt(ctr) == int(cell));
          if (layout == VertexLayout::PerCell)
            for (int v = 0; v < 3; ++v) CHECK(m.cellIds[tri[v]] == cell);
        }
        CHECK(cellArea == doctest::Approx(g.cellArea()).epsilon(1e-5));
        total += cellArea;
      }
      CHECK(total == doctest::Approx(N * g.cellArea()).epsilon(1e-5));
    }
}

TEST_CASE("shared layout reuses vertices; per-cell positions match shared positions") {
  Grid g(Shape::Hex, 20, 20, 1.7);
  const Mesh pc = buildFillMesh(g, MeshKind::Hex4, VertexLayout::PerCell);
  const Mesh sh = buildFillMesh(g, MeshKind::Hex4, VertexLayout::Shared);
  CHECK(sh.vertexCount() < pc.vertexCount() / 2);
  REQUIRE(pc.indices.size() == sh.indices.size());
  for (size_t i = 0; i < pc.indices.size(); ++i) {
    CHECK(pc.positions[pc.indices[i] * 2] == sh.positions[sh.indices[i] * 2]);
    CHECK(pc.positions[pc.indices[i] * 2 + 1] == sh.positions[sh.indices[i] * 2 + 1]);
  }
  CHECK(sh.bytes() < pc.bytes());
}

TEST_CASE("outline mesh: one quad per unique edge") {
  for (Shape s : {Shape::Hex, Shape::Rect}) {
    Grid g(s, 5, 4, 2.0);
    const double w = 0.2;
    const Mesh m = buildOutlineMesh(g, w);
    const size_t edges = g.uniqueEdges().size();
    CHECK(m.itemCount == edges);
    CHECK(m.triangleCount() == edges * 2);
    for (uint32_t idx : m.indices) CHECK(idx < m.vertexCount());
    double area = 0;
    for (uint32_t t = 0; t < m.triangleCount(); ++t) area += std::abs(signedArea(m, t));
    CHECK(area == doctest::Approx(edges * 2.0 * w).epsilon(1e-5));
    CHECK(area == doctest::Approx(computeMetrics(g, {w}).strokeAreaApprox).epsilon(1e-5));
  }
  CHECK_THROWS(buildOutlineMesh(Grid(Shape::Hex, 1, 1, 1.0), 0.0));
}
