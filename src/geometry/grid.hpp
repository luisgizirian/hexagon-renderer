#pragma once
// Grid geometry for rectangular (square) and hexagonal cells.
//
// Conventions (see docs/GEOMETRY.md):
// * World coordinates are y-down (screen convention), units are arbitrary ("world units"),
//   and the grid bounding box starts at (0,0).
// * Hexagons are regular and POINTY-TOP, indexed with AXIAL coordinates (q, r).
//   A rectangular region of hexes is addressed with ODD-R offset coordinates (col, row):
//   odd rows are shifted right by half a hex width.
// * The hex "size" is the side length a (== circumradius).
// * Every vertex lies on an integer lattice. For hexes the lattice unit is
//   (sqrt(3)/2 * a, a/2); for squares it is (s, s). Shared vertices therefore have
//   bit-identical float coordinates in every cell that uses them, which avoids
//   cracks between adjacent cells during rasterization.
#include <array>
#include <cstdint>
#include <vector>

namespace hexr {

enum class Shape { Rect, Hex };

struct Vec2 {
  double x = 0, y = 0;
};
struct IVec2 {
  int64_t x = 0, y = 0;
  bool operator==(const IVec2& o) const { return x == o.x && y == o.y; }
};
struct Bounds {
  double minX = 0, minY = 0, maxX = 0, maxY = 0;
  double width() const { return maxX - minX; }
  double height() const { return maxY - minY; }
};
struct Axial {
  int q = 0, r = 0;
};

// One boundary segment shared by at most two cells.
struct Edge {
  IVec2 a, b;        // lattice endpoints
  int cellA = -1;    // owning cell
  int cellB = -1;    // neighbour across the edge, -1 if this is an outer boundary edge
  bool external() const { return cellB < 0; }
};

Axial offsetToAxial(int col, int row);
void axialToOffset(Axial a, int& col, int& row);
// Axial neighbour direction for hex edge k (edge k joins corner k and k+1).
Axial hexEdgeDirection(int k);

class Grid {
 public:
  Grid(Shape shape, int cols, int rows, double size);

  Shape shape() const { return shape_; }
  int cols() const { return cols_; }
  int rows() const { return rows_; }
  double size() const { return size_; }
  int cellCount() const { return cols_ * rows_; }
  int cornersPerCell() const { return shape_ == Shape::Hex ? 6 : 4; }

  double unitX() const { return unitX_; }
  double unitY() const { return unitY_; }

  // Corners in lattice coordinates, in clockwise order on screen (y-down).
  // Hex: k=0 top, 1 upper-right, 2 lower-right, 3 bottom, 4 lower-left, 5 upper-left.
  // Rect: 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left.
  std::array<IVec2, 6> cornersLattice(int cell) const;
  IVec2 centerLattice(int cell) const;  // exact for hex; rect centre needs half units (2x lattice)
  Vec2 toWorld(IVec2 p) const { return {double(p.x) * unitX_, double(p.y) * unitY_}; }
  Vec2 center(int cell) const;
  std::array<Vec2, 6> cornersWorld(int cell) const;

  // Neighbour across edge k of a cell, or -1 when outside the grid.
  int neighbor(int cell, int edge) const;
  std::vector<int> neighbors(int cell) const;

  // Each shared edge exactly once. Ownership rule: a cell emits edge k when
  // k is one of its "owned" edges (hex: 0,1,2 = NE,E,SE; rect: 1,2 = right,bottom)
  // or when there is no neighbour across it.
  std::vector<Edge> uniqueEdges() const;

  Bounds bounds() const;
  double cellArea() const;
  double cellPerimeter() const;

  // Independent CPU reference: which cell contains world point p (-1 if none).
  // Hex uses cube rounding of fractional axial coordinates; rect uses floor.
  int cellAt(Vec2 p) const;
  // Distance from p to the boundary of a cell polygon (always >= 0).
  double distanceToCellBoundary(int cell, Vec2 p) const;
  // Candidate cells near p (containing cell + neighbours), for boundary-distance queries.
  std::vector<int> cellsNear(Vec2 p) const;

  int index(int col, int row) const { return row * cols_ + col; }
  void colRow(int cell, int& col, int& row) const {
    col = cell % cols_;
    row = cell / cols_;
  }

 private:
  Shape shape_;
  int cols_, rows_;
  double size_;
  double unitX_, unitY_;
  int64_t shiftX_ = 0, shiftY_ = 0;  // lattice shift so that bounds start at 0
};

}  // namespace hexr
