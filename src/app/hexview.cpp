// hexview: GLFW window rendering the same MeshRenderer scenes as hexbench (FIFO / vsync-limited, so
// frame times here are NOT benchmark results; use hexbench for measurements).
//
// Keys: S shape | M mesh (hex4/hex6) | L layout | B submit (batched/per-cell) | F fill/outline mode
//       +/- cell size | [ ] stroke width | arrows pan | 0 reset pan | Esc/Q quit
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>

#include "geometry/grid.hpp"
#include "platform/cli.hpp"
#include "renderers/mesh_renderer.hpp"
#include "vk/swapchain.hpp"

using namespace hexr;

namespace {

struct ViewerState {
  Config cfg;
  double panX = 0, panY = 0;
  bool dirty = true;
};

void onKey(GLFWwindow* win, int key, int, int action, int mods) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  auto* st = static_cast<ViewerState*>(glfwGetWindowUserPointer(win));
  Config& c = st->cfg;
  const double step = (mods & GLFW_MOD_SHIFT) ? 200 : 40;
  switch (key) {
    case GLFW_KEY_ESCAPE:
    case GLFW_KEY_Q: glfwSetWindowShouldClose(win, GLFW_TRUE); return;
    case GLFW_KEY_S:
      c.shape = c.shape == Shape::Hex ? Shape::Rect : Shape::Hex;
      c.mesh = defaultMesh(c.shape);
      break;
    case GLFW_KEY_M:
      if (c.shape == Shape::Hex) c.mesh = c.mesh == MeshKind::Hex4 ? MeshKind::Hex6 : MeshKind::Hex4;
      break;
    case GLFW_KEY_L:
      c.layout = c.layout == VertexLayout::PerCell ? VertexLayout::Shared : VertexLayout::PerCell;
      break;
    case GLFW_KEY_B: c.submit = c.submit == Submit::Batched ? Submit::PerCell : Submit::Batched; break;
    case GLFW_KEY_F:
      c.mode = c.mode == DrawMode::Fill ? DrawMode::Outline
               : c.mode == DrawMode::Outline ? DrawMode::FillOutline : DrawMode::Fill;
      break;
    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_ADD: c.cellSize *= 1.25; break;
    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT: c.cellSize = std::max(0.5, c.cellSize / 1.25); break;
    case GLFW_KEY_RIGHT_BRACKET: c.stroke *= 1.25; break;
    case GLFW_KEY_LEFT_BRACKET: c.stroke = std::max(0.05, c.stroke / 1.25); break;
    case GLFW_KEY_LEFT: st->panX -= step; return;
    case GLFW_KEY_RIGHT: st->panX += step; return;
    case GLFW_KEY_UP: st->panY -= step; return;
    case GLFW_KEY_DOWN: st->panY += step; return;
    case GLFW_KEY_0: st->panX = st->panY = 0; return;
    default: return;
  }
  st->dirty = true;
}

int run(const Config& initial) {
  if (!glfwInit()) {
    std::fprintf(stderr, "error: glfwInit failed (is DISPLAY set? try DISPLAY=:1)\n");
    return 1;
  }
  if (!glfwVulkanSupported()) {
    std::fprintf(stderr, "error: GLFW reports no Vulkan support\n");
    glfwTerminate();
    return 1;
  }
  uint32_t extCount = 0;
  const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
  ContextOptions opt;
  opt.validation = initial.validationLayers;
  opt.instanceExtensions.assign(exts, exts + extCount);
  opt.swapchain = true;

  int rc = 0;
  {
    VulkanContext ctx(opt);
    if (!glfwGetPhysicalDevicePresentationSupport(ctx.instance(), ctx.physicalDevice(), ctx.queueFamily()))
      throw std::runtime_error("queue family has no presentation support");

    const int initW = initial.width ? initial.width : 1280, initH = initial.height ? initial.height : 800;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* win = glfwCreateWindow(initW, initH, "hexview", nullptr, nullptr);
    if (!win) throw std::runtime_error("glfwCreateWindow failed");
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    HEX_VK_CHECK(glfwCreateWindowSurface(ctx.instance(), win, nullptr, &surface));

    ViewerState st;
    st.cfg = initial;
    glfwSetWindowUserPointer(win, &st);
    glfwSetKeyCallback(win, onKey);
    {
      int fbw, fbh;
      glfwGetFramebufferSize(win, &fbw, &fbh);
      Swapchain sc(ctx, surface, uint32_t(fbw), uint32_t(fbh));
      MeshRenderer renderer(ctx, sc.format(), OutputKind::Color);

      uint64_t frames = 0, totalFrames = 0;
      auto t0 = std::chrono::steady_clock::now();
      while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        if (st.dirty) {
          vkDeviceWaitIdle(ctx.device());  // buffers may still be in use by in-flight frames
          const Grid grid(st.cfg.shape, st.cfg.cols, st.cfg.rows, st.cfg.effectiveCellSize());
          renderer.setScene(grid, st.cfg);
          st.dirty = false;
        }
        glfwGetFramebufferSize(win, &fbw, &fbh);
        VkCommandBuffer cb = sc.beginFrame({{1.0f, 1.0f, 1.0f, 1.0f}}, uint32_t(fbw), uint32_t(fbh));
        if (cb == VK_NULL_HANDLE) {
          if (fbw == 0 || fbh == 0) glfwWaitEvents();
          continue;
        }
        renderer.record(cb, sc.extent().width, sc.extent().height, st.panX, st.panY);
        sc.endFrame(uint32_t(fbw), uint32_t(fbh));
        ++frames;
        ++totalFrames;

        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - t0).count();
        const bool last = initial.frames > 0 && totalFrames >= uint64_t(initial.frames);
        if (dt > 0.5 || last) {
          const DrawStats& s = renderer.stats();
          char title[512];
          std::snprintf(title, sizeof title,
                        "hexview | %s %s %s %s %s | %dx%d cells, size %.2f | %llu tris, %llu draws, %.1f KiB | "
                        "%.1f fps (vsync)",
                        toString(st.cfg.shape), toString(st.cfg.mesh), toString(st.cfg.layout),
                        toString(st.cfg.submit), toString(st.cfg.mode), st.cfg.cols, st.cfg.rows,
                        st.cfg.effectiveCellSize(), (unsigned long long)s.triangles,
                        (unsigned long long)s.drawCalls, s.bufferBytes / 1024.0, frames / dt);
          glfwSetWindowTitle(win, title);
          if (last) std::printf("%s\n", title);
          if (dt > 0.5) {
            frames = 0;
            t0 = now;
          }
        }
        if (last) glfwSetWindowShouldClose(win, GLFW_TRUE);
      }
      vkDeviceWaitIdle(ctx.device());
    }
    vkDestroySurfaceKHR(ctx.instance(), surface, nullptr);
    glfwDestroyWindow(win);
    if (ctx.validationMessages() > 0) {
      std::fprintf(stderr, "validation messages: %u\n", ctx.validationMessages());
      rc = 1;
    }
  }
  glfwTerminate();
  return rc;
}

}  // namespace

int main(int argc, char** argv) {
  const ParseResult p = parseArgs(argc, argv);
  if (!p.ok) {
    std::fprintf(stderr, "error: %s\n", p.error.c_str());
    return 2;
  }
  if (p.cfg.command == Command::Help) {
    std::puts("usage: hexview [--shape hex|rect] [--mesh ..] [--layout ..] [--submit ..] [--mode ..]\n"
              "               [--cols N --rows N --cell-size S --stroke W] [--width W --height H]\n"
              "               [--frames N] [--validation]\n"
              "keys: S shape, M mesh, L layout, B submit, F mode, +/- size, [ ] stroke, arrows pan, Q quit");
    return 0;
  }
  try {
    return run(p.cfg);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    glfwTerminate();
    return 1;
  }
}
