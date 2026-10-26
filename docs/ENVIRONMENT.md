# Environment (Phase 1 report)

This was collected read-only on the target device. The power mode, clocks and services were not
changed. Run `hexbench info` to get the live values as JSON (this includes the current clocks,
temperatures, load and free memory).

## Hardware / OS

| Item | Value |
|------|-------|
| Board | NVIDIA Jetson Orin Nano Engineering Reference Developer Kit **Super** |
| CPU | 6× Cortex-A78AE (max 1.73 GHz) |
| RAM | 7.4 GiB unified (only ~2–3 GiB available during inspection because ollama/docker were running) |
| L4T / JetPack | R36.5.2 (JetPack 6.x) |
| OS / kernel | Ubuntu 22.04.5 LTS, 5.15.199-tegra, aarch64 |
| Power mode | `nvpmodel`: **25W**. Not changed. `jetson_clocks` was not run. |
| GPU clock | devfreq `17000000.gpu`, governor `nvhost_podgov`, 306–918 MHz (dynamic) |
| Thermal | `cpu/gpu/soc0-2/tj-thermal` zones readable (~48–52 °C idle) |
| Display | Xorg session on `:1`, DP-1 3840×2160 @ **30 Hz** (SSH shells need `DISPLAY=:1`) |

## Toolchain / SDKs

| Item | Value |
|------|-------|
| Compiler / build | GCC 11.4, CMake 3.22.1, Unix Makefiles (no ninja/clang) |
| Vulkan | loader 1.3.204, device API **1.3.251**, NVIDIA driver 540.5.0, "NVIDIA Tegra Orin (nvgpu)", integrated |
| Vulkan timing | `timestampValidBits` 64, `timestampPeriod` 1 ns, `timestampComputeAndGraphics`, `VK_EXT_calibrated_timestamps` |
| Other Vulkan | `VK_EXT_memory_budget`, `VK_KHR_external_memory_fd`, timeline semaphores, swapchain, multiDrawIndirect, geometryShader |
| Queues | graphics+compute (×16), compute-only (×8), transfer (×2) |
| CUDA | 12.6.68, sm_87, 8 SMs (checked with a compiled probe) |
| Profilers | Nsight Systems 2024.5, Nsight Compute 2024.3, Nsight Graphics 2024.2 |

## Dependencies added for this project

These were missing: `glslang-tools` (shader compiler), `vulkan-validationlayers` and `libglfw3-dev`.
`sudo` requires a password on this machine, so the approved Ubuntu packages were **extracted into a
user-local prefix** instead of being installed system-wide. No system files were modified:

```bash
scripts/setup-local-deps.sh   # apt-get download + dpkg -x into ~/.local/hexdeps/root
source scripts/env.sh         # VK_LAYER_PATH, LD_LIBRARY_PATH, PATH
```

- CMake finds GLFW and glslangValidator through `HEX_DEPS_PREFIX`. `hexview` gets an RPATH to the local lib dir.
- The validation layer manifest's `library_path` is rewritten to an absolute path.
- The CTest `gpu` test sets the layer environment variables itself.
- Equivalent system install: `sudo apt install glslang-tools vulkan-validationlayers libglfw3-dev`.
  After that, the local prefix and `env.sh` are not needed.

## Implications for benchmarking (Phase 4)

- The background services (ollama, docker) and the dynamic GPU/CPU clock governors add noise. Each
  run therefore records the power mode, clocks, temperatures, load average and available memory,
  and the runs are repeated.
- The windowed path is vsync-locked at 30 Hz, so headless offscreen rendering is the measurement path.
- Free memory is limited, so grid sizes are capped (CLI limit 2^26 cells) and buffer sizes are reported.
- The loader is 1.3.204 and the device is 1.3.251, so the code targets the Vulkan 1.3 core only.
