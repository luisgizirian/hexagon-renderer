#include <cmath>
#include <set>
#include <tuple>

#include "doctest.h"
#include "geometry/grid.hpp"

using namespace hexr;

static const double kSqrt3 = std::sqrt(3.0);

TEST_CASE("offset <-> axial round trip, odd-r") {
  for (int row = -5; row <= 5; ++row)
    for (int col = -5; col <= 5; ++col) {
      int c, r;
      axialToOffset(offsetToAxial(col, row), c, r);
      CHECK(c == col);
      CHECK(r == row);
    }
  // Odd rows are shifted right: (0,1) has axial q = 0, (0,2) has q = -1.
  CHECK(offsetToAxial(0, 1).q == 0);
  CHECK(offsetToAxial(0, 2).q == -1);
}

TEST_CASE("invalid grids throw") {
  CHECK_THROWS(Grid(Shape::Hex, 0, 3, 1.0));
  CHECK_THROWS(Grid(Shape::Rect, 3, -1, 1.0));
  CHECK_THROWS(Grid(Shape::Hex, 3, 3, 0.0));
  CHECK_THROWS(Grid(Shape::Hex, 3, 3, NAN));
}

TEST_CASE("single hexagon dimensions (pointy-top)") {
  const double a = 2.5;
  Grid g(Shape::Hex, 1, 1, a);
  const Bounds b = g.bounds();
  CHECK(b.width() == doctest::Approx(kSqrt3 * a));
  CHECK(b.height() == doctest::Approx(2 * a));
  const auto c = g.cornersWorld(0);
  for (int k = 0; k < 6; ++k) {
    const auto& p = c[k];
    const auto& q = c[(k + 1) % 6];
    CHECK(std::hypot(q.x - p.x, q.y - p.y) == doctest::Approx(a));  // regular: all sides = a
    const Vec2 ctr = g.center(0);
    CHECK(std::hypot(p.x - ctr.x, p.y - ctr.y) == doctest::Approx(a));  // circumradius = a
  }
  CHECK(c[0].y == doctest::Approx(0.0));  // pointy top touches y = 0
}

TEST_CASE("neighbours are symmetric and consistent with edges") {
  for (Shape shape : {Shape::Hex, Shape::Rect}) {
    Grid g(shape, 7, 6, 1.3);
    const int n = g.cornersPerCell();
    for (int cell = 0; cell < g.cellCount(); ++cell) {
      const auto cc = g.cornersLattice(cell);
      for (int k = 0; k < n; ++k) {
        const int nb = g.neighbor(cell, k);
        if (nb < 0) continue;
        const int back = (k + n / 2) % n;
        CHECK(g.neighbor(nb, back) == cell);
        // Shared edge: identical lattice endpoints in reverse order (exact, no float error).
        const auto nc = g.cornersLattice(nb);
        CHECK(cc[k] == nc[(back + 1) % n]);
        CHECK(cc[(k + 1) % n] == nc[back]);
        // Neighbour centres are one cell pitch apart.
        const Vec2 a = g.center(cell), b = g.center(nb);
        const double pitch = shape == Shape::Hex ? kSqrt3 * 1.3 : 1.3;
        CHECK(std::hypot(a.x - b.x, a.y - b.y) == doctest::Approx(pitch));
      }
    }
  }
}

TEST_CASE("interior cells have full neighbourhoods") {
  Grid h(Shape::Hex, 5, 5, 1.0);
  CHECK(h.neighbors(h.index(2, 2)).size() == 6);
  CHECK(h.neighbors(h.index(2, 1)).size() == 6);
  CHECK(h.neighbors(h.index(0, 0)).size() == 2);   // even-row corner: E, SE
  CHECK(h.neighbors(h.index(4, 1)).size() == 3);   // odd-row right edge: W, NW, SW
  Grid r(Shape::Rect, 5, 5, 1.0);
  CHECK(r.neighbors(r.index(2, 2)).size() == 4);
  CHECK(r.neighbors(r.index(0, 0)).size() == 2);
}

TEST_CASE("bounds are tight and contain every corner") {
  for (Shape shape : {Shape::Hex, Shape::Rect})
    for (auto [c, r] : {std::pair{1, 1}, {1, 4}, {5, 1}, {8, 9}}) {
      Grid g(shape, c, r, 3.7);
      const Bounds b = g.bounds();
      double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
      for (int cell = 0; cell < g.cellCount(); ++cell)
        for (int k = 0; k < g.cornersPerCell(); ++k) {
          const Vec2 p = g.cornersWorld(cell)[k];
          minX = std::min(minX, p.x);
          minY = std::min(minY, p.y);
          maxX = std::max(maxX, p.x);
          maxY = std::max(maxY, p.y);
        }
      CHECK(minX == doctest::Approx(b.minX));
      CHECK(minY == doctest::Approx(b.minY));
      CHECK(maxX == doctest::Approx(b.maxX));
      CHECK(maxY == doctest::Approx(b.maxY));
    }
}

TEST_CASE("cellAt reference agrees with geometry") {
  for (Shape shape : {Shape::Hex, Shape::Rect}) {
    Grid g(shape, 9, 7, 2.0);
    for (int cell = 0; cell < g.cellCount(); ++cell) {
      const Vec2 c = g.center(cell);
      CHECK(g.cellAt(c) == cell);
      // Points 90% of the way from the centre to each corner are still inside.
      for (int k = 0; k < g.cornersPerCell(); ++k) {
        const Vec2 p = g.cornersWorld(cell)[k];
        CHECK(g.cellAt({c.x + 0.9 * (p.x - c.x), c.y + 0.9 * (p.y - c.y)}) == cell);
      }
    }
    CHECK(g.cellAt({-0.5, -0.5}) == -1);
    const Bounds b = g.bounds();
    CHECK(g.cellAt({b.maxX + 0.5, b.maxY + 0.5}) == -1);
  }
}

TEST_CASE("distance to cell boundary") {
  Grid r(Shape::Rect, 2, 2, 4.0);
  CHECK(r.distanceToCellBoundary(0, {2, 2}) == doctest::Approx(2.0));
  CHECK(r.distanceToCellBoundary(0, {1, 2}) == doctest::Approx(1.0));
  Grid h(Shape::Hex, 1, 1, 2.0);
  CHECK(h.distanceToCellBoundary(0, h.center(0)) == doctest::Approx(kSqrt3));  // inradius
}

TEST_CASE("large grids: exact shared vertices and no accumulated drift") {
  // Far from the origin, vertices are still computed from integer lattice coordinates, so the
  // same vertex reached from two cells has bit-identical float coordinates.
  Grid g(Shape::Hex, 3000, 3000, 7.123);
  const int cell = g.index(2999, 2999);
  const int nb = g.neighbor(cell, 4);  // west
  REQUIRE(nb >= 0);
  const auto a = g.cornersWorld(cell), b = g.cornersWorld(nb);
  CHECK(float(a[4].x) == float(b[2].x));
  CHECK(float(a[4].y) == float(b[2].y));
  CHECK(g.cellAt(g.center(cell)) == cell);
  // Centre spacing remains exact relative to the closed-form position.
  const Vec2 c = g.center(cell);
  CHECK(c.x == doctest::Approx(kSqrt3 * 7.123 * (2999 + 0.5 + 0.5)).epsilon(1e-12));
  CHECK(c.y == doctest::Approx(7.123 * (1.0 + 1.5 * 2999)).epsilon(1e-12));
}

TEST_CASE("unique edges: no duplicates, correct ownership") {
  for (Shape shape : {Shape::Hex, Shape::Rect}) {
    Grid g(shape, 6, 5, 1.0);
    std::set<std::tuple<int64_t, int64_t, int64_t, int64_t>> seen;
    for (const Edge& e : g.uniqueEdges()) {
      using K = std::tuple<int64_t, int64_t, int64_t, int64_t>;
      const bool swap = e.a.x > e.b.x || (e.a.x == e.b.x && e.a.y > e.b.y);
      const K key = swap ? K{e.b.x, e.b.y, e.a.x, e.a.y} : K{e.a.x, e.a.y, e.b.x, e.b.y};
      CHECK(seen.insert(key).second);
      CHECK(e.cellA >= 0);
      if (!e.external()) CHECK(e.cellB != e.cellA);
    }
  }
}
