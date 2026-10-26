#pragma once
// Command-line configuration shared by hexbench and hexview.
#include <string>

#include "geometry/grid.hpp"
#include "geometry/mesh.hpp"
#include "platform/json.hpp"

namespace hexr {

enum class Command { Render, Info, Ink, Svg, Validate, Selftest, Help };
enum class DrawMode { Fill, Outline, FillOutline };
enum class Submit { Batched, PerCell };

const char* toString(Command c);
const char* toString(DrawMode m);
const char* toString(Submit s);

struct Config {
  Command command = Command::Render;
  Shape shape = Shape::Hex;
  MeshKind mesh = MeshKind::Hex4;
  bool meshExplicit = false;
  VertexLayout layout = VertexLayout::PerCell;
  Submit submit = Submit::Batched;
  DrawMode mode = DrawMode::Fill;
  int cols = 64, rows = 64;
  double cellSize = 8.0;      // hex side or square side, world units (= pixels when rendering)
  bool equalArea = false;     // rect only: treat cellSize as a hex side and use the equal-area square
  int width = 0, height = 0;  // render target; 0 = fit grid bounds
  double stroke = 1.0;        // outline width, world units
  int warmup = 10, iterations = 100;
  int frames = 0;             // hexview: exit after N frames (0 = run until closed)
  std::string out;            // output path ("-" or empty = stdout for text outputs)
  std::string format = "json";
  bool validationLayers = false;
  // Ink / page model
  double pageWidth = 210, pageHeight = 297, pageMargin = 10;
  double fillInk = 1.0, strokeInk = 1.0;

  // Side actually used for the grid (applies --equal-area).
  double effectiveCellSize() const;
};

struct ParseResult {
  bool ok = false;
  std::string error;
  Config cfg;
};

ParseResult parseArgs(int argc, const char* const* argv);
JsonObject configToJson(const Config& c);
std::string usage();

}  // namespace hexr
