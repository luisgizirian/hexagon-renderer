#include "ink_model/svg.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace hexr {

namespace {
std::string num(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.6g", v);
  return b;
}
}  // namespace

std::string gridToSvg(const Grid& grid, const SvgOptions& opt) {
  const Bounds b = grid.bounds();
  const double w = b.width() + 2 * opt.margin, h = b.height() + 2 * opt.margin;
  std::ostringstream s;
  s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << num(w) << opt.unit << "\" height=\"" << num(h)
    << opt.unit << "\" viewBox=\"" << num(-opt.margin) << ' ' << num(-opt.margin) << ' ' << num(w) << ' '
    << num(h) << "\">\n";
  const int n = grid.cornersPerCell();
  if (opt.fill) {
    s << "<g fill=\"" << opt.fillColor << "\" stroke=\"none\">\n";
    for (int c = 0; c < grid.cellCount(); ++c) {
      const auto p = grid.cornersWorld(c);
      s << "<polygon points=\"";
      for (int k = 0; k < n; ++k) s << (k ? " " : "") << num(p[k].x) << ',' << num(p[k].y);
      s << "\"/>\n";
    }
    s << "</g>\n";
  }
  if (opt.outline) {
    s << "<path fill=\"none\" stroke=\"" << opt.strokeColor << "\" stroke-width=\"" << num(opt.strokeWidth)
      << "\" stroke-linecap=\"butt\" d=\"";
    for (const Edge& e : grid.uniqueEdges()) {
      const Vec2 a = grid.toWorld(e.a), c = grid.toWorld(e.b);
      s << 'M' << num(a.x) << ',' << num(a.y) << 'L' << num(c.x) << ',' << num(c.y);
    }
    s << "\"/>\n";
  }
  s << "</svg>\n";
  return s.str();
}

bool writeTextFile(const std::string& path, const std::string& text) {
  std::ofstream f(path, std::ios::binary);
  f << text;
  return bool(f);
}

}  // namespace hexr
