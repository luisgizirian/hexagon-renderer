#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec2 inPos;
layout(location = 1) in uint inId;
layout(location = 0) flat out uint vId;
void main() {
  vId = inId;
  gl_Position = vec4(inPos * pc.scale + pc.offset, 0.0, 1.0);
}
