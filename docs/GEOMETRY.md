# Geometry conventions

## Coordinates

- World units are pixels when rendering. **y points down** and the grid bounding box starts at (0, 0).
- **Hexagons are pointy-top** and use axial coordinates (q, r). Rectangular regions use **odd-r**
  offset coordinates (col, row): odd rows are shifted right by half a cell.
- `size` is the hexagon side *a*, which equals its circumradius. Hex width = √3·a, height = 2a,
  row pitch = 1.5a, column pitch = √3·a.
- For squares, `size` is the side *s*.

## Integer lattice (crack-free shared vertices)

Every vertex is an **integer lattice point** scaled by per-axis units:

| Shape | unit x | unit y | corner offsets from centre (k = 0..5 / 0..3) |
|-------|--------|--------|------------------------------------------------|
| hex   | √3/2·a | a/2    | (0,−2) (1,−1) (1,1) (0,2) (−1,1) (−1,−1) |
| rect  | s      | s      | (0,0) (1,0) (1,1) (0,1) relative to the top-left |

The hex centre of cell (col, row) is lattice (2·col + (row & 1) + 1, 3·row + 2).

A vertex shared by neighbouring cells is computed from the *same integers* with the *same
multiply*, so it is bit-identical in float. Combined with Vulkan's top-left rasterization rule, this
gives watertight fills with no gaps or double coverage. This is verified on the GPU (see below).
Large grids do not accumulate drift: tests check 3000×3000 cells.

## Neighbours and edges

- Hex edge *k* joins corner *k* and corner *k+1*. Edge directions in order are
  NE(+1,−1), E(+1,0), SE(0,+1), SW(−1,+1), W(−1,0), NW(0,−1) in axial coordinates.
  The opposite edge is *k+3*.
- Rect edges are top, right, bottom, left. The opposite edge is *k+2*.
- **Ownership:** each unique edge is emitted exactly once. A hex owns edges 0–2 (NE, E, SE) and a
  rect owns edges 1–2 (right, bottom). Boundary edges are emitted by the only cell adjacent to them.
  Tests check this against closed forms and against de-duplicated enumeration.

## Triangulations

| Mesh | Triangles | Vertices per cell (per-cell layout) | Notes |
|------|-----------|-------------------------------------|-------|
| `rect2` | 2 | 4 | |
| `hex4`  | 4: (0,1,2) (2,3,4) (4,5,0) (0,2,4) | 6 | boundary vertices only |
| `hex6`  | 6: centre fan | 7 | adds a centre vertex |

- All triangles have the same winding (positive signed area in y-down space). Culling is disabled.
- The triangles of a cell are contiguous in the index buffer. This lets a batched draw recover the
  cell as `gl_PrimitiveID / trisPerItem`.

**Vertex layouts:**
- `per-cell`: every cell has its own vertices plus a `uint` cell-id attribute.
- `shared`: positions are de-duplicated across the grid (V = E − F + 1 corners, plus the hex6 centres).
  There is no id attribute. The cell id comes from `gl_PrimitiveID` (batched draws) or from
  `gl_InstanceIndex` via `firstInstance` (per-cell draws).

**Outline:** one quad (2 triangles) per unique edge, centred on the edge, with butt caps. Joints
show small notches or overlaps, which is expected. The ink model uses the same approximation.

## Correctness checks

`hexbench validate` and the `gpu` test suite render into R32 targets and compare every pixel with
the independent CPU reference `Grid::cellAt`. That reference uses cube rounding for hexes and
`floor` for squares.

| Pass | Format | What it measures |
|------|--------|------------------|
| coverage | R32_SFLOAT, additive blend | number of fragments per pixel |
| id | R32_UINT | cell id + 1 |

Each pixel centre is classified by its distance to the reference cell boundary. Pixels within
0.02 px of an edge are *ambiguous*, because the exact tie-break there depends on the rasterizer. All
other pixels must satisfy:

- inside → coverage exactly 1, with the correct id. Failures are counted as `gaps` / `idMismatches`.
- outside → coverage 0. Failures are counted as `spurious`.
- anywhere → coverage ≤ 1. Failures are counted as `overlaps`.

A negative-control test renders one grid and checks it against a different reference grid. The
validator must report failures.
