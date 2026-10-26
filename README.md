# hexagon-renderer

A reproducible native C++17/Vulkan benchmark for the **NVIDIA Jetson Orin Nano**. It compares
strategies for rendering dense **hexagonal vs rectangular grids** and includes a separate,
GPU-independent **ink/space model** for the same layouts.

## Status

| Phase | State |
|-------|-------|
| 1 Discovery / environment report | ✅ done ([docs/ENVIRONMENT.md](docs/ENVIRONMENT.md)) |
| 2 Vertical slice (Vulkan context, offscreen, test pattern, window) | ✅ done |
| 3 Correctness baseline (rect + explicit hex renderers, GPU validation, ink model, tests) | ✅ done |
| 4 Benchmarking (GPU timestamps, stats, sweeps, CSV/JSON) | ⬜ **next** |
| 5 Instancing, procedural SDF, Vulkan compute, CUDA | ⬜ |
| 6 3D prisms, Dear ImGui viewer | ⬜ |
| 7 Final results report | ⬜ |

**There are no performance results yet.** `--warmup`/`--iterations` are accepted but not used until Phase 4.
[docs/PLAN.md](docs/PLAN.md) has the detailed done/remaining breakdown, the known gaps and the
steps to resume.

## Build

Requirements: CMake ≥ 3.22, GCC with C++17, Vulkan SDK headers/loader (`libvulkan-dev`),
`glslangValidator`, GLFW 3.3 (viewer only), Vulkan validation layers (optional, for tests/debug).

```bash
# Either install system-wide (needs sudo):
#   sudo apt install glslang-tools vulkan-validationlayers libglfw3-dev
# or extract them into ~/.local/hexdeps without root (what this machine uses):
scripts/setup-local-deps.sh
source scripts/env.sh        # VK_LAYER_PATH / LD_LIBRARY_PATH / PATH for the local prefix

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j6
```

CMake options: `HEX_BUILD_VIEWER` (ON), `HEX_BUILD_TESTS` (ON), `HEX_DEPS_PREFIX`
(default `~/.local/hexdeps/root/usr`; ignored if the packages are installed system-wide).

## Test

```bash
cd build && ctest --output-on-failure
```

- `unit` (`hextests`): geometry, meshes, ink model, CLI and JSON (30 cases, ≈24k assertions).
- `gpu` (`hexgputests`): offscreen renders compared pixel-by-pixel against independent CPU
  references, with validation layers enabled (`HEX_VALIDATION=1`). Any validation message fails the suite.

## Run

```bash
B=./build/hexbench
$B help
$B info                                   # device / software / power / thermal JSON
$B selftest --validation                  # procedural test pattern, every pixel verified
$B validate --shape hex --mesh hex6 --layout shared --cols 101 --rows 77 --cell-size 3.3
$B render --mode fill+outline --cols 24 --rows 16 --cell-size 12 --out grid.ppm
$B ink --cols 40 --rows 30 --cell-size 5 --stroke 0.3 [--format csv]
$B svg --cols 20 --rows 12 --cell-size 5 --out grid.svg

DISPLAY=:1 ./build/hexview --mode fill+outline --cell-size 14   # windowed (on the local X display)
```

`hexview` keys: `S` shape, `M` mesh, `L` layout, `B` batched/per-cell, `F` fill/outline mode,
`+`/`-` cell size, `[`/`]` stroke, arrows pan, `Q` quit. It is FIFO/vsync-limited (the attached
display runs at 30 Hz), so its FPS is **not** a benchmark result.

## Strategies implemented so far

| Shape | Mesh | Layout | Submission |
|-------|------|--------|------------|
| rect  | `rect2` (2 tris) | `per-cell` (own vertices + cell-id attribute) / `shared` (deduplicated, id from `gl_PrimitiveID`) | `batched` (1 draw) / `per-cell` (1 draw per cell) |
| hex   | `hex4` (4 tris, no centre) / `hex6` (6-tri centre fan) | same | same |

Modes: `fill`, `outline` (one quad per unique edge, butt caps) and `fill+outline`.

How the cell id reaches the fragment shader:
- `per-cell` layout: a vertex attribute.
- `shared` layout, batched draw: `gl_PrimitiveID / trisPerItem`.
- `shared` layout, per-cell draws: `firstInstance` → `gl_InstanceIndex`.

Fill correctness is verified for every combination: each pixel is compared against a CPU
point-in-cell reference using coverage-count and cell-id passes, and there is a negative control.
Outlines are not yet pixel-validated. See [docs/GEOMETRY.md](docs/GEOMETRY.md).

## Commands

| Command | Output |
|---------|--------|
| `render` | Renders offscreen, prints the config and draw stats as JSON, and writes a PPM with `--out`. Outline targets are padded by half the stroke width. |
| `validate` | GPU vs CPU fill check. Nonzero exit on any gap, overlap, spurious pixel or id mismatch. |
| `selftest` | Renders the procedural test pattern and verifies every pixel. |
| `info` | System, power, clocks, thermal and Vulkan device JSON. |
| `ink` | Hex vs equal-area square metrics, ratios and A4 page fit. JSON, or CSV via `--format csv`. |
| `svg` | Vector export: a polygon per cell plus each unique edge once. |

`render`, `validate`, `selftest` and `hexview` accept `--validation`. They exit nonzero if the validation layers
report any message.

## Layout

```
src/geometry    grid lattice, neighbours, unique edges, CPU point-in-cell reference, mesh builders
src/ink_model   area/perimeter/edge/stroke/page metrics, SVG export
src/platform    CLI, JSON, sysfs/system info
src/vk          context, offscreen target, pipelines, swapchain
src/renderers   test pattern, mesh renderer, GPU-vs-CPU validation
src/app         hexbench (headless CLI), hexview (GLFW)
shaders/        GLSL → SPIR-V headers at build time
tests/          doctest suites
docs/           PLAN.md (status/roadmap), ENVIRONMENT.md, GEOMETRY.md, INK_MODEL.md
scripts/        setup-local-deps.sh (no-root deps), env.sh (runtime env)
```
