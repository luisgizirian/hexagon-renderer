#version 450
layout(location = 0) flat in uint vId;
layout(location = 0) out vec4 outCount;
// Additively blended: each covering fragment adds 1, so the target holds the coverage count.
void main() { outCount = vec4(1.0); }
