#version 450
layout(push_constant) uniform PC { vec2 size; } pc;
layout(location = 0) out vec4 outColor;
// Must match testPatternExpected() in src/renderers/test_pattern.cpp.
void main() {
  ivec2 p = ivec2(gl_FragCoord.xy);
  float checker = float(((p.x >> 4) + (p.y >> 4)) & 1);
  outColor = vec4(gl_FragCoord.x / pc.size.x, gl_FragCoord.y / pc.size.y, checker, 1.0);
}
