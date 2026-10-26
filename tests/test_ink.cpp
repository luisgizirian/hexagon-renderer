#include <algorithm>
#include <cmath>
#include <string>

#include "doctest.h"
#include "ink_model/ink_model.hpp"
#include "ink_model/svg.hpp"

using namespace hexr;

static const double kSqrt3 = std::sqrt(3.0);

TEST_CASE("hexagon and square area / perimeter formulas") {
  const double a = 3.0;
  Grid h(Shape::Hex, 1, 1, a);
  CHECK(h.cellArea() == doctest::Approx(3.0 * kSqrt3 / 2.0 * a * a));
  CHECK(h.cellPerimeter() == doctest::Approx(6 * a));
  Grid s(Shape::Rect, 1, 1, a);
  CHECK(s.cellArea() == doctest::Approx(a * a));
  CHECK(s.cellPerimeter() == doctest::Approx(4 * a));
}

TEST_CASE("equal-area comparison: ~6.95% shorter hex perimeter") {
  const double a = 1.0, s = equalAreaSquareSide(a);
  CHECK(s * s == doctest::Approx(1.5 * kSqrt3 * a * a));
  CHECK(equalAreaHexSide(s) == doctest::Approx(a));
  const double ratio = (6 * a) / (4 * s);
  CHECK(ratio == doctest::Approx(equalAreaPerimeterRatio()));
  CHECK(ratio == doctest::Approx(0.930605).epsilon(1e-6));
  CHECK((1 - ratio) * 100 == doctest::Approx(6.9395).epsilon(1e-4));
}

TEST_CASE("closed-form edge counts match enumeration") {
  for (Shape shape : {Shape::Hex, Shape::Rect})
    for (int c : {1, 2, 3, 7, 16})
      for (int r : {1, 2, 3, 8, 15}) {
        CAPTURE(c);
        CAPTURE(r);
        const LayoutMetrics m = computeMetrics(Grid(shape, c, r, 1.0), {});
        long long in, ex;
        closedFormEdgeCounts(shape, c, r, in, ex);
        CHECK(m.internalEdges == in);
        CHECK(m.externalEdges == ex);
        // Each cell side is either shared (counted twice) or external (once).
        CHECK(2 * in + ex == (shape == Shape::Hex ? 6LL : 4LL) * c * r);
        CHECK(m.totalPerimeterNaive == doctest::Approx(double(2 * in + ex)));
      }
}

TEST_CASE("limiting cases") {
  const LayoutMetrics h = computeMetrics(Grid(Shape::Hex, 1, 1, 2.0), {0.1});
  CHECK(h.internalEdges == 0);
  CHECK(h.externalEdges == 6);
  CHECK(h.joints == 6);
  CHECK(h.uniqueLength == doctest::Approx(12.0));
  CHECK(h.strokeAreaApprox == doctest::Approx(1.2));
  const LayoutMetrics r = computeMetrics(Grid(Shape::Rect, 1, 1, 2.0), {0.1});
  CHECK(r.externalEdges == 4);
  CHECK(r.joints == 4);
  CHECK(r.whitespaceInBounds == doctest::Approx(0.0));
  CHECK(h.whitespaceInBounds > 0);  // pointy-top hex leaves corner triangles in its bbox

  // Vanishing stroke -> outline ink -> 0 and fill+outline -> fill.
  const LayoutMetrics t = computeMetrics(Grid(Shape::Hex, 10, 10, 1.0), {1e-12});
  CHECK(t.inkOutlineOnly == doctest::Approx(0.0));
  CHECK(t.inkFillAndOutline == doctest::Approx(t.inkFillOnly));
}

TEST_CASE("large grids: unique edge length per cell approaches 3a (hex) and 2s (square)") {
  const double a = 1.0, s = equalAreaSquareSide(a);
  const LayoutMetrics h = computeMetrics(Grid(Shape::Hex, 400, 400, a), {});
  const LayoutMetrics r = computeMetrics(Grid(Shape::Rect, 400, 400, s), {});
  CHECK(h.totalFillArea == doctest::Approx(r.totalFillArea));  // equal-area, same count: fill identical
  CHECK(h.uniqueLength / h.cells == doctest::Approx(3 * a).epsilon(0.01));
  CHECK(r.uniqueLength / r.cells == doctest::Approx(2 * s).epsilon(0.01));
  // The infinite-grid outline ratio equals the single-cell perimeter ratio; finite grids differ.
  CHECK(h.uniqueLength / r.uniqueLength == doctest::Approx(equalAreaPerimeterRatio()).epsilon(0.005));
  const LayoutMetrics h1 = computeMetrics(Grid(Shape::Hex, 3, 3, a), {});
  const LayoutMetrics r1 = computeMetrics(Grid(Shape::Rect, 3, 3, s), {});
  CHECK(std::abs(h1.uniqueLength / r1.uniqueLength - equalAreaPerimeterRatio()) > 0.005);
}

TEST_CASE("ink: fill+outline does not double-count stroke over fill") {
  InkAssumptions ink{0.5, 1.0, 1.0};
  const LayoutMetrics m = computeMetrics(Grid(Shape::Rect, 4, 3, 10.0), ink);
  // With equal densities, ink = covered area = fill + outer half of external strokes.
  CHECK(m.inkFillAndOutline == doctest::Approx(m.totalFillArea + m.externalLength * 0.25));
  InkAssumptions dense{0.5, 0.2, 1.0};
  const LayoutMetrics d = computeMetrics(Grid(Shape::Rect, 4, 3, 10.0), dense);
  CHECK(d.inkFillAndOutline > d.inkFillOnly);
}

TEST_CASE("fit to page") {
  PageSpec page{100, 50, 0};
  int c, r;
  REQUIRE(fitToPage(Shape::Rect, 10, page, c, r));
  CHECK(c == 10);
  CHECK(r == 5);
  REQUIRE(fitToPage(Shape::Hex, 5, page, c, r));
  const Bounds b = Grid(Shape::Hex, c, r, 5).bounds();
  CHECK(b.width() <= 100 + 1e-9);
  CHECK(b.height() <= 50 + 1e-9);
  CHECK(Grid(Shape::Hex, c + 1, r, 5).bounds().width() > 100);
  CHECK(Grid(Shape::Hex, c, r + 1, 5).bounds().height() > 50);
  CHECK_FALSE(fitToPage(Shape::Hex, 30, page, c, r));  // 2a = 60 > 50
  CHECK_FALSE(fitToPage(Shape::Rect, 200, page, c, r));
}

TEST_CASE("svg export contains every cell and every unique edge") {
  Grid g(Shape::Hex, 4, 3, 5.0);
  SvgOptions o;
  const std::string svg = gridToSvg(g, o);
  size_t polys = 0, moves = 0;
  for (size_t p = svg.find("<polygon"); p != std::string::npos; p = svg.find("<polygon", p + 1)) ++polys;
  const size_t d = svg.find(" d=\"");
  REQUIRE(d != std::string::npos);
  for (size_t p = d; p < svg.size() && svg[p] != '/'; ++p)
    if (svg[p] == 'M') ++moves;
  CHECK(polys == 12);
  CHECK(moves == g.uniqueEdges().size());
  CHECK(svg.find("<svg") != std::string::npos);
  CHECK(svg.find("</svg>") != std::string::npos);
}

TEST_CASE("csv rows match the header") {
  const LayoutMetrics m = computeMetrics(Grid(Shape::Hex, 3, 2, 1.0), {});
  auto commas = [](const std::string& x) { return std::count(x.begin(), x.end(), ','); };
  CHECK(commas(metricsCsvHeader()) == commas(metricsCsvRow(m, {})));
  CHECK(metricsCsvRow(m, {}).rfind("hex,3,2,6,", 0) == 0);
}
