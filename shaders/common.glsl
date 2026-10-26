// Shared push-constant block for mesh pipelines (48 bytes).
layout(push_constant) uniform PC {
  vec2 scale;        // world -> NDC scale
  vec2 offset;       // world -> NDC offset
  uint idSource;     // 0: flat id from vertex stage, 1: gl_PrimitiveID / trisPerItem
  uint trisPerItem;
  uint colorMode;    // 0: per-item palette, 1: solidColor
  uint pad;
  vec4 solidColor;
} pc;
