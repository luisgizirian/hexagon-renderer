# Ink / space model

This is a **geometric estimate**, not a measurement of printer ink. It answers how much area and
how much stroke length a hexagonal layout needs compared with a square layout, for fill-only,
outline-only and fill-plus-outline drawings. It does not depend on the GPU code (`src/ink_model`,
`hexbench ink`, `hexbench svg`).

## Single cell

| | Hexagon (side a) | Square (side s) |
|-|------------------|-----------------|
| Area | (3√3/2)·a² | s² |
| Perimeter | 6a | 4s |

- **Equal area:** s = a·√(3√3/2) ≈ 1.6119·a.
- At equal area, the hex perimeter is 0.930605× the square's, i.e. **6.94 % shorter**
  (`theoretical_equal_area_perimeter_ratio`).

## Grids

For a grid of C×R cells (hex grids use odd-r rows):

- **Unique edges.** Shared edges are counted once.
  - hex internal = R(C−1) + (R−1)(2C−1); hex external = 6RC − 2·internal
  - rect internal = R(C−1) + C(R−1); rect external = 2(R+C)
  - These closed forms are tested against explicit enumeration.
- **Joints** (unique stroke vertices) come from Euler's formula: V = E − F + 1.
- **Naive perimeter** sums every cell's perimeter, so shared edges count twice. It is reported only
  to show how large the double-count error is.
- **Bounds / whitespace.** A hex grid has ragged borders, so its bounding box contains uncovered
  area. A square grid has none.

## Stroke and ink approximations

With stroke width *w* (centred on each edge, butt caps, joints ignored), and densities
`fill_ink_per_area` = f and `stroke_ink_per_area` = k:

- stroke area ≈ unique_length · w
- **fill only** = f · fill_area
- **outline only** = k · stroke area
- **fill + outline** = f · (fill_area − internal_length·w − external_length·w/2) + k · stroke area

In the fill + outline case, the opaque stroke replaces the fill underneath it, so ink is not counted
twice. Internal strokes lie entirely over fill. External strokes lie half over fill and half outside.

**Limitations:**
- Joints are ignored: butt caps leave notches at corners and overlap at 3- or 4-way joints. The
  error is O(joints · w²), so it is small when w ≪ cell size.
- Ink density, dot gain, paper and halftoning are all ignored.

## Equal-area comparison (`hexbench ink`)

`hexbench ink` compares a hex grid with the **equal-area square grid of the same C×R**, so both have
the same cell count and identical fill area. Numbers for a = 5, w = 0.3, f = k = 1 (output of the
current code):

| C×R | hex unique length | square unique length | hex/square outline | hex/square fill+outline | hex/square bbox area | hex bbox whitespace |
|-----|------------------:|---------------------:|-------------------:|------------------------:|---------------------:|--------------------:|
| 1×1 | 30 | 32.24 | 0.9306 | 0.9952 | 1.333 | 25.0 % |
| 3×3 | 190 | 193.42 | 0.9823 | 1.0033 | 1.296 | 22.9 % |
| 10×10 | 1 695 | 1 773.04 | 0.9560 | 1.0016 | 1.085 | 7.8 % |
| 100×100 | 151 995 | 162 797.34 | 0.9336 | 1.0002 | 1.008 | 0.8 % |
| 1000×1000 | 15 019 995 | 16 134 667.53 | 0.9309 | 1.0000 | 1.001 | 0.08 % |

**Observations:**
- The outline-ink saving approaches the single-cell ratio (−6.94 %) only for large grids. Small
  grids lose most of it, because the hex boundary is ragged and has more external edge length.
- Fill + outline ink is essentially equal (within ±0.5 %). Fill dominates, and the outer half of the
  longer hex boundary stroke slightly offsets the shorter interior strokes.

## Fixed page

`fitToPage` packs as many whole cells as fit inside the usable page area (page minus margins):

- hex rows = 1 + ⌊(H − 2a)/(1.5a)⌋
- hex cols = ⌊(W − (rows > 1 ? √3a/2 : 0)) / (√3a)⌋
- square: ⌊W/s⌋ × ⌊H/s⌋

The `page_fit` block of `hexbench ink` reports the cells, fill utilization, whitespace and inks.

Example on A4 with a 10 mm margin, a = 5 mm (s ≈ 8.06 mm), w = 0.3 mm:

| | cells | utilization | unique stroke length (mm) |
|-|------:|------------:|--------------------------:|
| hex 21×36 | 756 | 93.3 % | 11 905 |
| square 23×34 | 782 | 96.5 % | 13 064 |

## SVG export

`hexbench svg` writes one `<polygon>` per cell (fill) and one `<path>` holding every unique edge
exactly once (outline). This lets the geometry be inspected or printed with any SVG tool.
