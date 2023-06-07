#version 450

layout(location=0) out vec2 screenUV;

layout(set=0, binding=4) uniform UniformBufferObject {
  vec4 debug;
} globals;

void main() {
  vec2 screenPos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  screenUV = screenPos;
  vec4 pos = vec4(screenPos * 2.0f - 1.0f, 0.0f, 1.0f);

  gl_Position = pos;
}