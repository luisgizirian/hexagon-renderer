#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec2 inPos;
layout(location = 0) flat out uint vId;
void main() {
  vId = uint(gl_InstanceIndex);  // per-cell draws pass the cell id as firstInstance
  gl_Position = vec4(inPos * pc.scale + pc.offset, 0.0, 1.0);
}
