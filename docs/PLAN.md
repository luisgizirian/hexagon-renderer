# Project plan and status

This is the working plan for the hexagon vs rectangle grid rendering and ink-efficiency benchmark on
the Jetson Orin Nano. Use it to pick up the work from where it stopped. The original brief has
seven phases.

**Current state (2026-10-26): Phases 1–3 are done. Phase 4 (benchmarking) is next and has not started.**
No performance results exist yet. Do not quote frame rates from `hexview`: it is vsync-locked at 30 Hz.

Legend: ✅ done and verified · 🟡 partial · ⬜ not started

---

## Ground rules (agreed with the user)

- Do not change `nvpmodel` or `jetson_clocks`, and do not stop services. Only **record** power mode,
  clocks, temperatures, load and memory with every run.
- Do not install anything system-wide without approval. `sudo` needs a password on this machine,
  so dependencies live in a user-local prefix (see [ENVIRONMENT.md](ENVIRONMENT.md)).
- Never fabricate results. Label anything unverified. Each step gets tests before the next one starts.
- Headless offscreen rendering is the measurement path. The GLFW window is only for inspection.
- Tooling decisions: C++17 + CMake (Makefiles), GLSL → SPIR-V via `glslangValidator` at build time,
  doctest (vendored) + CTest. Dear ImGui (vendored) is planned for Phase 6.

## How to resume

```bash
cd ~/repos/hexagon-renderer
source scripts/env.sh                      # local validation layers / glslang / GLFW
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j6
(cd build && ctest --output-on-failure)    # must be green before continuing
./build/hexbench info                      # confirm device state
```

Then continue at **Phase 4** below.

Housekeeping still pending: the repo has `git init` and all files staged, but **nothing is
committed**, because no git `user.name`/`user.email` is configured. Set them and make the first
commit before starting Phase 4.

---

## Phase 1 — Discovery ✅

- ✅ Read-only device inspection. Results are in [ENVIRONMENT.md](ENVIRONMENT.md): Orin Nano Super,
  L4T R36.5.2, Vulkan 1.3.251 (NVIDIA 540.5.0), CUDA 12.6 sm_87, nvpmodel 25W, X display `:1` at 30 Hz.
- ✅ Capability gaps found and approved: `glslang-tools`, `vulkan-validationlayers`, `libglfw3-dev`.
  These are installed user-locally with `scripts/setup-local-deps.sh`.

## Phase 2 — Minimal vertical slice ✅

| Item | Status | Where |
|------|--------|-------|
| CMake project, warnings-as-clean (0 warnings in Debug and Release), shader → header step | ✅ | `CMakeLists.txt`, `shaders/` |
| Vulkan context: instance/device/queue, optional validation (`--validation` / `HEX_VALIDATION=1`), host-visible buffers, images, one-shot submit | ✅ | `src/vk/context.*` |
| Offscreen target + readback + PPM dump | ✅ | `src/vk/offscreen.*` |
| Procedural test pattern verified pixel-by-pixel (`hexbench selftest`) | ✅ | `src/renderers/test_pattern.*` |
| GLFW window + FIFO swapchain (`hexview`, `--frames N` auto-exit) | ✅ | `src/vk/swapchain.*`, `src/app/hexview.cpp` |
| Environment/state JSON (`hexbench info`) | ✅ | `src/platform/sysinfo.*` |

## Phase 3 — Correctness baseline ✅

| Item | Status | Where |
|------|--------|-------|
| Pointy-top axial hex grid (odd-r), rect grid, integer lattice, neighbours, bounds, unique edges, CPU `cellAt` reference | ✅ | `src/geometry/grid.*`, [GEOMETRY.md](GEOMETRY.md) |
| Meshes: `rect2`, `hex4`, `hex6`; `per-cell` vs `shared` layouts; outline quads | ✅ | `src/geometry/mesh.*` |
| Strategy A (rect) + Strategy B (explicit hex): fill / outline / fill+outline, batched (1 draw) vs per-cell draws | ✅ | `src/renderers/mesh_renderer.*`, `shaders/mesh_*` |
| GPU-vs-CPU fill validation (coverage count + cell id, negative control) | ✅ | `src/renderers/validation.*`, `hexbench validate` |
| Ink/space model: areas, perimeters, closed-form + enumerated edges, joints, stroke area, ink (3 modes), bbox whitespace, equal-area and fixed-page comparisons, CSV/JSON | ✅ | `src/ink_model/ink_model.*`, [INK_MODEL.md](INK_MODEL.md) |
| SVG export | ✅ | `src/ink_model/svg.*`, `hexbench svg` |
| CLI parsing/validation, JSON serialization | ✅ | `src/platform/cli.*`, `src/platform/json.*` |
| Tests: `unit` (30 cases, ~24k assertions) + `gpu` (7 cases, validation layers on, 0 messages) | ✅ | `tests/` |

**Known gaps carried forward from Phase 3:**
- Outline rendering is checked visually but **not pixel-validated**. Butt-cap joints overlap by
  design. A stroke-coverage check against the analytic stroke area would close this.
- `--warmup` and `--iterations` are parsed and validated but **not used yet** (Phase 4).
- `--format csv` is only supported by `ink`. Other commands reject it explicitly.
- No antialiasing yet. All fills are aliased, using the top-left rule. AA arrives with the procedural
  strategy, and comparisons will need matched visual quality.

---

## Phase 4 — Benchmarking ⬜ (next)

Todo `benchmark-core`. Suggested order: build each piece with a test, then run the first sweep.

1. ⬜ **`src/benchmark/` timing core**
   - Query pool with `VK_QUERY_TYPE_TIMESTAMP` around each pass. Use `hostQueryReset`, which is
     already enabled. The timestamp period is 1 ns, with 64 valid bits.
   - `VK_EXT_calibrated_timestamps` to correlate CPU and GPU clocks for end-to-end latency.
   - CPU timers (`steady_clock`) for geometry generation, buffer upload, command recording and
     submit+wait. Report each **separately**.
   - Warm-up and measured iterations come from `--warmup` / `--iterations`. Stats: min, median,
     mean, p5, p95, p99, stddev, and N.
   - Sanity check that the GPU actually ran the work: GPU time scales with the workload, and a
     readback checksum or a validate run is attached to the result.
2. ⬜ **Workload counters**: triangles, vertices, indices, instances and draw calls (already in
   `DrawStats`). Add buffer bytes, `VK_EXT_memory_budget` heap usage, and pipeline statistics or
   occlusion queries as a fragment/coverage proxy (shaded pixels, overdraw).
3. ⬜ **Static vs dynamic geometry**: a `--update static|dynamic` option. Dynamic means geometry is
   regenerated and re-uploaded every frame (or colours only). Persistent-mapped buffers already exist.
4. ⬜ **`hexbench bench` command**: one config in, one JSON/CSV record out. Each record includes
   `hexbench info` state (power mode, clocks, temps, load, free memory) captured before and after.
5. ⬜ **Sweep script** (`scripts/sweep.sh` or `.py`): shape × mesh × layout × submit × mode × grid
   size × cell size × resolution → `results/<timestamp>/*.csv`. Repeat runs to measure variability.
   Cap grid sizes because only ~2–3 GiB of RAM is free, and fail gracefully on allocation errors.
6. ⬜ Tests: stats maths, the CSV/JSON schema, and a GPU smoke benchmark with tiny N that checks
   timings are > 0 and increase with grid size.
7. ⬜ Docs: `docs/METHODOLOGY.md` (what is measured, sync points, noise sources, how to reproduce).

## Phase 5 — Alternative strategies ⬜

| Todo | Strategy | Notes |
|------|----------|-------|
| `instancing` | C: shared hex/rect mesh + per-instance attributes | Compare attribute layouts (vec2 offset vs packed), instance count, draw-call reduction |
| `procedural` | D: one quad per cell (or one fullscreen pass) + analytic hex SDF coverage with AA | Must match visual quality; measure fragment cost vs geometry cost, overdraw, cell size and resolution effects |
| `vk-compute` | E: compute generates vertex buffers / cell data; compute writes pixels directly | Time compute, barriers and display separately |
| `cuda` | F: kernels for geometry or pixels | Report kernel-only vs end-to-end separately; Vulkan interop via `VK_KHR_external_memory_fd`, otherwise document the copy cost |

Each new strategy must pass `validate` (or an equivalent reference comparison) before it is benchmarked.

## Phase 6 — 3D and visualization ⬜

- ⬜ `three-d`: instanced hex prisms with depth and simple lighting. Document the cost against 2D.
- ⬜ `imgui-viewer`: vendor Dear ImGui into `hexview`, with panels for strategy, grid, cell size,
  shape, mode, counts, benchmark results and ink comparison. Minimal key controls already exist.

## Phase 7 — Final report ⬜

- ⬜ `final-report`: `docs/RESULTS.md` containing:
  - the environment, build/run/test instructions and test results
  - example datasets
  - performance and ink tables
  - limitations
  - evidence-based recommendations for dense 2D, sparse, outlined, small-cell and 3D cases.

---

## Todo dependency graph (remaining)

```
benchmark-core ─┬─> instancing ──> three-d
                ├─> procedural ─┐
                ├─> vk-compute ─┼─> final-report
                ├─> cuda ───────┘
                └─> imgui-viewer
```

## Risks / notes

- Background services (ollama, docker) and dynamic DVFS add noise, so record state and repeat runs.
  Never pin clocks without approval.
- The loader is 1.3.204 and the device is 1.3.251, so use only Vulkan 1.3 core features (e.g. dynamic
  rendering). `geometryShader` is enabled only so fragment shaders can read
  `gl_PrimitiveID`.
- Over SSH, use `DISPLAY=:1` for `hexview`. Benchmarks need no display.
