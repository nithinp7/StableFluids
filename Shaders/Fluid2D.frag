#version 450

layout(location=0) in vec2 screenUV;

layout(location=0) out vec4 color;

layout(set=0, binding=0) uniform sampler2D velocityField;
layout(set=0, binding=1) uniform sampler2D divergenceField;
layout(set=0, binding=2) uniform sampler2D pressureField;

layout(set=0, binding=3) uniform UniformBufferObject {
  vec4 debug;
} globals;

void main() {
  vec2 vel = texture(velocityField, screenUV).rg;
  float pres = texture(pressureField, screenUV).r;
  // color = vec4(1.0, 0.0, 0.0, 1.0);//
  // color = vec4(abs(pres), 0.0, 0.0, 1.0);
  color = vec4(abs(vel), 0.0, 1.0);
  // color = vec4(0.0, 0.0, abs(pres), 1.0);
  //vec4(0.5 * normalize(vel) + vec2(0.5), 0.0, 1.0);

#ifndef SKIP_TONEMAP
  // TODO: Tone-map??
  const float exposure = 0.8;
  color.rgb = vec3(1.0) - exp(-color.rgb * exposure);
#endif
}