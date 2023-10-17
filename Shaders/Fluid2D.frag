#version 450

layout(location=0) in vec2 screenUV;

layout(location=0) out vec4 color;

layout(set=0, binding=0) uniform sampler2D fractalTexture;
layout(set=0, binding=1) uniform sampler2D velocityField;
layout(set=0, binding=2) uniform sampler2D divergenceField;
layout(set=0, binding=3) uniform sampler2D pressureField;
layout(set=0, binding=4) uniform sampler2D colorField;

layout(set=0, binding=5) uniform UniformBufferObject {
  float time;
} globals;

void main() {
  vec2 vel = texture(velocityField, screenUV).rg;
  float pres = texture(pressureField, screenUV).r;
  // color = vec4(1.0, 0.0, 0.0, 1.0);//
  // color = vec4(abs(pres), 0.0, 0.0, 1.0);
  // color = vec4(abs(vel), 0.0, 1.0);
  color = texture(colorField, screenUV);
  // color = vec4(vec3(length(vel)), 1.0);
  // color = vec4(0.0, 0.0, abs(pres), 1.0);
  //vec4(0.5 * normalize(vel) + vec2(0.5), 0.0, 1.0);

  float f = texture(fractalTexture, screenUV).r;
  // float f2 = 100.0 * f;
  float f2 = 100.0 * f * f;
  // color = vec4(vec3(f2), 1.0);//vec4(f2 * sin(13.0 * f), f2 * sin(13.0 * f + 0.2), f2 * sin(13.0 * f + 0.45), 1.0);

#ifndef SKIP_TONEMAP
  // TODO: Tone-map??
  const float exposure = 0.2;
  color.rgb = vec3(1.0) - exp(-color.rgb * exposure);
  // color.rgb = color.rgb / (color.rgb + vec3(1.0));
#endif
}