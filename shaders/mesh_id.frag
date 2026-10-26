#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) flat in uint vId;
layout(location = 0) out uint outId;
// Writes item id + 1 (0 = background) into an R32_UINT target for validation.
void main() {
  outId = (pc.idSource == 1u ? uint(gl_PrimitiveID) / pc.trisPerItem : vId) + 1u;
}
