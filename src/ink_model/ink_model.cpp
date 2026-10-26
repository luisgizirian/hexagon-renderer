#include "ink_model/ink_model.hpp"

#include <algorithm>
#include <cmath>

#include "geometry/mesh.hpp"
#include "platform/json.hpp"

namespace hexr {

namespace {
constexpr double kSqrt3 = 1.7320508075688772;
constexpr double kEps = 1e-9;
}  // namespace

double equalAreaSquareSide(double hexSide) { return hexSide * std::sqrt(1.5 * kSqrt3); }
double equalAreaHexSide(double squareSide) { return squareSide / std::sqrt(1.5 * kSqrt3); }
double equalAreaPerimeterRatio() { return 6.0 / (4.0 * std::sqrt(1.5 * kSqrt3)); }

void closedFormEdgeCounts(Shape shape, int cols, int rows, long long& internal, long long& external) {
  const long long C = cols, R = rows;
  if (shape == Shape::Rect) {
    internal = R * (C - 1) + C * (R - 1);
    external = 2 * (R + C);
  } else {
    // Odd-r layout: R*(C-1) east-west pairs, and 2C-1 diagonal pairs between consecutive rows.
    internal = R * (C - 1) + (R - 1) * (2 * C - 1);
    external = 6 * R * C - 2 * internal;
  }
}

LayoutMetrics computeMetrics(const Grid& grid, const InkAssumptions& ink) {
  LayoutMetrics m;
  m.shape = grid.shape();
  m.cols = grid.cols();
  m.rows = grid.rows();
  m.cells = grid.cellCount();
  m.cellSide = grid.size();
  m.cellArea = grid.cellArea();
  m.cellPerimeter = grid.cellPerimeter();
  m.totalFillArea = double(m.cells) * m.cellArea;
  m.totalPerimeterNaive = double(m.cells) * m.cellPerimeter;

  for (const Edge& e : grid.uniqueEdges()) (e.external() ? m.externalEdges : m.internalEdges)++;
  m.internalLength = double(m.internalEdges) * grid.size();
  m.externalLength = double(m.externalEdges) * grid.size();
  m.uniqueLength = m.internalLength + m.externalLength;
  // Euler's formula for a connected planar subdivision: V - E + F = 1 (F excludes the outer face).
  m.joints = (m.internalEdges + m.externalEdges) - m.cells + 1;

  const Bounds b = grid.bounds();
  m.boundsWidth = b.width();
  m.boundsHeight = b.height();
  m.boundsArea = m.boundsWidth * m.boundsHeight;
  m.whitespaceInBounds = std::max(0.0, m.boundsArea - m.totalFillArea);

  const double w = ink.strokeWidth;
  m.strokeAreaApprox = m.uniqueLength * w;
  m.inkFillOnly = ink.fillInkPerArea * m.totalFillArea;
  m.inkOutlineOnly = ink.strokeInkPerArea * m.strokeAreaApprox;
  const double strokeOverFill = m.internalLength * w + m.externalLength * w / 2.0;
  m.inkFillAndOutline = ink.fillInkPerArea * std::max(0.0, m.totalFillArea - strokeOverFill) +
                        ink.strokeInkPerArea * m.strokeAreaApprox;
  return m;
}

bool fitToPage(Shape shape, double a, const PageSpec& page, int& cols, int& rows) {
  const double W = page.usableWidth(), H = page.usableHeight();
  cols = rows = 0;
  if (!(a > 0) || W <= 0 || H <= 0) return false;
  if (shape == Shape::Rect) {
    cols = int(std::floor(W / a + kEps));
    rows = int(std::floor(H / a + kEps));
  } else {
    const double hexW = kSqrt3 * a;
    if (2 * a > H + kEps || hexW > W + kEps) return false;
    rows = 1 + int(std::floor((H - 2 * a) / (1.5 * a) + kEps));
    const double extra = rows > 1 ? hexW / 2 : 0.0;
    cols = int(std::floor((W - extra) / hexW + kEps));
    if (cols < 1 && rows > 1) {  // the half-hex offset of odd rows may not fit: fall back to one row
      rows = 1;
      cols = int(std::floor(W / hexW + kEps));
    }
  }
  if (cols < 1 || rows < 1) {
    cols = rows = 0;
    return false;
  }
  return true;
}

std::string metricsToJson(const LayoutMetrics& m, const InkAssumptions& ink) {
  JsonObject a;
  a.set("stroke_width", ink.strokeWidth)
      .set("fill_ink_per_area", ink.fillInkPerArea)
      .set("stroke_ink_per_area", ink.strokeInkPerArea)
      .set("stroke_model", "unique_length * stroke_width, butt caps, joints ignored");
  JsonObject o;
  o.set("shape", toString(m.shape))
      .set("cols", m.cols)
      .set("rows", m.rows)
      .set("cells", m.cells)
      .set("cell_side", m.cellSide)
      .set("cell_area", m.cellArea)
      .set("cell_perimeter", m.cellPerimeter)
      .set("total_fill_area", m.totalFillArea)
      .set("total_perimeter_naive", m.totalPerimeterNaive)
      .set("internal_edges", m.internalEdges)
      .set("external_edges", m.externalEdges)
      .set("internal_length", m.internalLength)
      .set("external_length", m.externalLength)
      .set("unique_length", m.uniqueLength)
      .set("joints", m.joints)
      .set("bounds_width", m.boundsWidth)
      .set("bounds_height", m.boundsHeight)
      .set("bounds_area", m.boundsArea)
      .set("whitespace_in_bounds", m.whitespaceInBounds)
      .set("stroke_area_approx", m.strokeAreaApprox)
      .set("ink_fill_only", m.inkFillOnly)
      .set("ink_outline_only", m.inkOutlineOnly)
      .set("ink_fill_and_outline", m.inkFillAndOutline)
      .set("assumptions", a);
  return o.str();
}

std::string metricsCsvHeader() {
  return "shape,cols,rows,cells,cell_side,cell_area,cell_perimeter,total_fill_area,total_perimeter_naive,"
         "internal_edges,external_edges,internal_length,external_length,unique_length,joints,bounds_width,"
         "bounds_height,bounds_area,whitespace_in_bounds,stroke_width,fill_ink_per_area,stroke_ink_per_area,"
         "stroke_area_approx,ink_fill_only,ink_outline_only,ink_fill_and_outline";
}

std::string metricsCsvRow(const LayoutMetrics& m, const InkAssumptions& ink) {
  std::string r = toString(m.shape);
  auto num = [&](double v) { r += ',' + jsonNumber(v); };
  auto cnt = [&](long long v) { r += ',' + std::to_string(v); };
  cnt(m.cols); cnt(m.rows); cnt(m.cells);
  num(m.cellSide); num(m.cellArea); num(m.cellPerimeter); num(m.totalFillArea); num(m.totalPerimeterNaive);
  cnt(m.internalEdges); cnt(m.externalEdges);
  num(m.internalLength); num(m.externalLength); num(m.uniqueLength);
  cnt(m.joints);
  num(m.boundsWidth); num(m.boundsHeight); num(m.boundsArea); num(m.whitespaceInBounds);
  num(ink.strokeWidth); num(ink.fillInkPerArea); num(ink.strokeInkPerArea);
  num(m.strokeAreaApprox); num(m.inkFillOnly); num(m.inkOutlineOnly); num(m.inkFillAndOutline);
  return r;
}

}  // namespace hexr
