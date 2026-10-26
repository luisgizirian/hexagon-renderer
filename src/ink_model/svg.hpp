#pragma once
#include <string>

#include "geometry/grid.hpp"

namespace hexr {

struct SvgOptions {
  bool fill = true;
  bool outline = true;
  double strokeWidth = 0.25;  // world units; SVG user units are mapped 1:1 to `unit`
  double margin = 0;          // added around the grid bounds
  std::string unit = "mm";
  std::string fillColor = "#8fb3d9";
  std::string strokeColor = "#000000";
};

// Vector export for independent inspection / print tests. Fill is drawn as one polygon per cell
// (no stroke); outlines are drawn as a single path containing every unique edge exactly once.
std::string gridToSvg(const Grid& grid, const SvgOptions& opt);
bool writeTextFile(const std::string& path, const std::string& text);

}  // namespace hexr
