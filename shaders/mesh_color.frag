#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) flat in uint vId;
layout(location = 0) out vec4 outColor;
vec3 palette(uint id) {
  uint h = id * 2654435761u;
  h ^= h >> 15;
  h *= 2246822519u;
  h ^= h >> 13;
  return vec3(float(h & 255u), float((h >> 8) & 255u), float((h >> 16) & 255u)) / 255.0 * 0.55 + 0.35;
}
void main() {
  uint id = pc.idSource == 1u ? uint(gl_PrimitiveID) / pc.trisPerItem : vId;
  outColor = pc.colorMode == 1u ? pc.solidColor : vec4(palette(id), 1.0);
}
