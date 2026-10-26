#include "geometry/grid.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hexr {

namespace {
constexpr double kSqrt3 = 1.7320508075688772;

// Pointy-top corner offsets in lattice units (sqrt(3)/2 a, a/2), clockwise on screen.
constexpr int kHexCorner[6][2] = {{0, -2}, {1, -1}, {1, 1}, {0, 2}, {-1, 1}, {-1, -1}};
// Axial direction of the neighbour across edge k (corner k -> corner k+1).
constexpr int kHexDir[6][2] = {{+1, -1}, {+1, 0}, {0, +1}, {-1, +1}, {-1, 0}, {0, -1}};
// Rect: edge 0 top, 1 right, 2 bottom, 3 left.
constexpr int kRectCorner[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
constexpr int kRectDir[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};

double segmentDistance(Vec2 p, Vec2 a, Vec2 b) {
  const double dx = b.x - a.x, dy = b.y - a.y;
  const double len2 = dx * dx + dy * dy;
  double t = len2 > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2 : 0.0;
  t = std::clamp(t, 0.0, 1.0);
  const double ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
  return std::sqrt(ex * ex + ey * ey);
}
}  // namespace

Axial offsetToAxial(int col, int row) { return {col - (row - (row & 1)) / 2, row}; }

void axialToOffset(Axial a, int& col, int& row) {
  row = a.r;
  col = a.q + (a.r - (a.r & 1)) / 2;
}

Axial hexEdgeDirection(int k) { return {kHexDir[k][0], kHexDir[k][1]}; }

Grid::Grid(Shape shape, int cols, int rows, double size)
    : shape_(shape), cols_(cols), rows_(rows), size_(size) {
  if (cols <= 0 || rows <= 0) throw std::invalid_argument("grid dimensions must be positive");
  if (!(size > 0) || !std::isfinite(size)) throw std::invalid_argument("cell size must be positive");
  if (int64_t(cols) * rows > (int64_t(1) << 30)) throw std::invalid_argument("grid too large");
  if (shape == Shape::Hex) {
    unitX_ = kSqrt3 / 2.0 * size;
    unitY_ = size / 2.0;
    shiftX_ = 1;  // leftmost corner of an even row sits at lattice x = -1
    shiftY_ = 2;  // top corner of row 0 sits at lattice y = -2
  } else {
    unitX_ = unitY_ = size;
  }
}

std::array<IVec2, 6> Grid::cornersLattice(int cell) const {
  int col, row;
  colRow(cell, col, row);
  std::array<IVec2, 6> out{};
  if (shape_ == Shape::Hex) {
    const int64_t cx = 2 * int64_t(col) + (row & 1) + shiftX_;
    const int64_t cy = 3 * int64_t(row) + shiftY_;
    for (int k = 0; k < 6; ++k) out[k] = {cx + kHexCorner[k][0], cy + kHexCorner[k][1]};
  } else {
    for (int k = 0; k < 4; ++k) out[k] = {col + kRectCorner[k][0], row + kRectCorner[k][1]};
  }
  return out;
}

IVec2 Grid::centerLattice(int cell) const {
  if (shape_ != Shape::Hex) throw std::logic_error("centerLattice is only exact for hex grids");
  int col, row;
  colRow(cell, col, row);
  return {2 * int64_t(col) + (row & 1) + shiftX_, 3 * int64_t(row) + shiftY_};
}

Vec2 Grid::center(int cell) const {
  if (shape_ == Shape::Hex) return toWorld(centerLattice(cell));
  int col, row;
  colRow(cell, col, row);
  return {(col + 0.5) * size_, (row + 0.5) * size_};
}

std::array<Vec2, 6> Grid::cornersWorld(int cell) const {
  const auto c = cornersLattice(cell);
  std::array<Vec2, 6> out{};
  for (int k = 0; k < cornersPerCell(); ++k) out[k] = toWorld(c[k]);
  return out;
}

int Grid::neighbor(int cell, int edge) const {
  int col, row;
  colRow(cell, col, row);
  int ncol, nrow;
  if (shape_ == Shape::Hex) {
    Axial a = offsetToAxial(col, row);
    a.q += kHexDir[edge][0];
    a.r += kHexDir[edge][1];
    axialToOffset(a, ncol, nrow);
  } else {
    ncol = col + kRectDir[edge][0];
    nrow = row + kRectDir[edge][1];
  }
  if (ncol < 0 || nrow < 0 || ncol >= cols_ || nrow >= rows_) return -1;
  return index(ncol, nrow);
}

std::vector<int> Grid::neighbors(int cell) const {
  std::vector<int> out;
  for (int k = 0; k < cornersPerCell(); ++k)
    if (int n = neighbor(cell, k); n >= 0) out.push_back(n);
  return out;
}

std::vector<Edge> Grid::uniqueEdges() const {
  std::vector<Edge> edges;
  const int n = cornersPerCell();
  edges.reserve(size_t(cellCount()) * (n / 2) + 2 * size_t(cols_ + rows_));
  for (int cell = 0; cell < cellCount(); ++cell) {
    const auto c = cornersLattice(cell);
    for (int k = 0; k < n; ++k) {
      const bool owned = shape_ == Shape::Hex ? (k <= 2) : (k == 1 || k == 2);
      const int nb = neighbor(cell, k);
      if (owned || nb < 0) edges.push_back({c[k], c[(k + 1) % n], cell, nb});
    }
  }
  return edges;
}

Bounds Grid::bounds() const {
  if (shape_ == Shape::Hex) {
    const int64_t maxIx = 2 * int64_t(cols_ - 1) + (rows_ > 1 ? 1 : 0) + 1 + shiftX_;
    const int64_t maxIy = 3 * int64_t(rows_ - 1) + 2 + shiftY_;
    return {0, 0, double(maxIx) * unitX_, double(maxIy) * unitY_};
  }
  return {0, 0, cols_ * size_, rows_ * size_};
}

double Grid::cellArea() const {
  return shape_ == Shape::Hex ? 1.5 * kSqrt3 * size_ * size_ : size_ * size_;
}

double Grid::cellPerimeter() const { return (shape_ == Shape::Hex ? 6.0 : 4.0) * size_; }

int Grid::cellAt(Vec2 p) const {
  int col, row;
  if (shape_ == Shape::Hex) {
    const double x = p.x - double(shiftX_) * unitX_;
    const double y = p.y - double(shiftY_) * unitY_;
    const double fr = y / (1.5 * size_);
    const double fq = x / (kSqrt3 * size_) - fr / 2.0;
    const double fs = -fq - fr;
    double rq = std::round(fq), rr = std::round(fr), rs = std::round(fs);
    const double dq = std::abs(rq - fq), dr = std::abs(rr - fr), ds = std::abs(rs - fs);
    if (dq > dr && dq > ds)
      rq = -rr - rs;
    else if (dr > ds)
      rr = -rq - rs;
    axialToOffset({int(rq), int(rr)}, col, row);
  } else {
    col = int(std::floor(p.x / size_));
    row = int(std::floor(p.y / size_));
  }
  if (col < 0 || row < 0 || col >= cols_ || row >= rows_) return -1;
  return index(col, row);
}

double Grid::distanceToCellBoundary(int cell, Vec2 p) const {
  const auto c = cornersWorld(cell);
  const int n = cornersPerCell();
  double d = INFINITY;
  for (int k = 0; k < n; ++k) d = std::min(d, segmentDistance(p, c[k], c[(k + 1) % n]));
  return d;
}

std::vector<int> Grid::cellsNear(Vec2 p) const {
  std::vector<int> out;
  // Locate the nearest lattice cell even if it is outside the grid, then scan its ring.
  int col, row;
  if (shape_ == Shape::Hex) {
    const double x = p.x - double(shiftX_) * unitX_;
    const double y = p.y - double(shiftY_) * unitY_;
    row = int(std::lround(y / (1.5 * size_)));
    col = int(std::lround((x / (kSqrt3 * size_)) - 0.5 * (row & 1)));
  } else {
    col = int(std::floor(p.x / size_));
    row = int(std::floor(p.y / size_));
  }
  for (int dr = -2; dr <= 2; ++dr)
    for (int dc = -2; dc <= 2; ++dc) {
      const int c = col + dc, r = row + dr;
      if (c >= 0 && r >= 0 && c < cols_ && r < rows_) out.push_back(index(c, r));
    }
  return out;
}

}  // namespace hexr
