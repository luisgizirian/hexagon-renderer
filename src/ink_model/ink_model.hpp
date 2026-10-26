#pragma once
// GPU-independent geometric "ink and space" model. All quantities are geometric
// estimates in the grid's world units (use millimetres for print layouts). They are NOT
// measurements of real printer ink; see docs/INK_MODEL.md for assumptions and limits.
#include <string>

#include "geometry/grid.hpp"

namespace hexr {

struct InkAssumptions {
  double strokeWidth = 0.25;     // world units (e.g. mm)
  double fillInkPerArea = 1.0;   // relative ink per unit area for solid fill
  double strokeInkPerArea = 1.0; // relative ink per unit area for strokes
};

struct LayoutMetrics {
  Shape shape = Shape::Rect;
  int cols = 0, rows = 0;
  long long cells = 0;
  double cellSide = 0;
  double cellArea = 0;
  double cellPerimeter = 0;
  double totalFillArea = 0;
  double totalPerimeterNaive = 0;  // sum of every cell's perimeter (shared edges counted twice)
  long long internalEdges = 0, externalEdges = 0;
  double internalLength = 0, externalLength = 0, uniqueLength = 0;
  long long joints = 0;            // unique lattice vertices where strokes meet
  double boundsWidth = 0, boundsHeight = 0, boundsArea = 0;
  double whitespaceInBounds = 0;   // bounding-box area not covered by cells (ragged hex borders)
  // Thin-stroke approximation: area ~= uniqueLength * strokeWidth. Ignores the overlap/notch
  // at joints (order strokeWidth^2 per joint) and assumes butt caps.
  double strokeAreaApprox = 0;
  double inkFillOnly = 0;
  double inkOutlineOnly = 0;
  double inkFillAndOutline = 0;    // stroke painted opaquely over fill (no double ink)
};

LayoutMetrics computeMetrics(const Grid& grid, const InkAssumptions& ink);

// Closed-form unique edge counts (cross-check for the enumeration in Grid::uniqueEdges).
void closedFormEdgeCounts(Shape shape, int cols, int rows, long long& internal, long long& external);

// Equal-area conversions between a regular hexagon (side a) and a square (side s).
double equalAreaSquareSide(double hexSide);  // s = a * sqrt(3*sqrt(3)/2)
double equalAreaHexSide(double squareSide);
// Hex perimeter / square perimeter at equal area (~0.9306, i.e. ~6.95% shorter).
double equalAreaPerimeterRatio();

struct PageSpec {
  double width = 210, height = 297, margin = 10;  // A4 in mm by default
  double usableWidth() const { return width - 2 * margin; }
  double usableHeight() const { return height - 2 * margin; }
};

// Largest cols x rows grid of the given shape/size that fits inside the usable page area.
// Returns false if not even one cell fits.
bool fitToPage(Shape shape, double cellSide, const PageSpec& page, int& cols, int& rows);

std::string metricsToJson(const LayoutMetrics& m, const InkAssumptions& ink);

// One CSV header line and one data row per layout (no trailing newline).
std::string metricsCsvHeader();
std::string metricsCsvRow(const LayoutMetrics& m, const InkAssumptions& ink);

}  // namespace hexr
