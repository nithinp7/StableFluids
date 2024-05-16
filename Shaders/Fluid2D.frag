#version 450

#include "SimulationCommon.glsl"

layout(location=0) in vec2 screenUV;

layout(location=0) out vec4 color;

void main() {
  vec2 vel = texture(velocityFieldTexture, screenUV).rg;
  float pres = texture(pressureFieldTexture, screenUV).r;
  float f = texture(fractalTexture, screenUV).r;
  float f2 = 100.0 * f * f;

  color = texture(colorFieldTexture, screenUV);
  //if (bool(simUniforms.inputMask & INPUT_BIT_CTRL)) 
  {
    if (bool(simUniforms.inputMask & INPUT_BIT_V)) {
      color = vec4(vec3(length(vel)), 1.0);
      // color = vec4(abs(vel), 0.0, 1.0);
    }

    if (bool(simUniforms.inputMask & INPUT_BIT_F)) {
      color = vec4(vec3(f2), 1.0);//vec4(f2 * sin(13.0 * f), f2 * sin(13.0 * f + 0.2), f2 * sin(13.0 * f + 0.45), 1.0);
    }

    if (bool(simUniforms.inputMask & INPUT_BIT_P)) {
      color = vec4(vec3(100. * abs(pres)), 1.0);
    }
  }

#ifndef SKIP_TONEMAP
  // TODO: Tone-map??
  const float exposure = 0.2;
  color.rgb = vec3(1.0) - exp(-color.rgb * exposure);
  // color.rgb = color.rgb / (color.rgb + vec3(1.0));
#endif
}