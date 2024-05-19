#version 450

#include "SimulationCommon.glsl"

layout(location=0) in vec2 screenUV;

layout(location=0) out vec4 color;

#define imageWidth push.params0
#define imageHeight push.params1

void main() {
  vec2 vel = texture(velocityFieldTexture, screenUV).rg;
  float pres = texture(pressureFieldTexture, screenUV).r;
  float f = texture(fractalTexture, screenUV).r;
  float f2 = 100.0 * f * f;

  // By default show color field
  bool bTonemap = true;
  color = texture(colorFieldTexture, screenUV);

  // Show velocity
  if (bool(simUniforms.inputMask & INPUT_BIT_V)) {
    color = vec4(vec3(length(vel)), 1.0);
    bTonemap = false;
  }

  // Show fractal
  if (bool(simUniforms.inputMask & INPUT_BIT_F)) {
    color = vec4(vec3(f2), 1.0);
    bTonemap = false;
  }

  // Show pressure
  if (bool(simUniforms.inputMask & INPUT_BIT_P)) {
    color = vec4(vec3(100. * abs(pres)), 1.0);
    bTonemap = false;
  }

  AutoExposure exposure = getAutoExposureEntry((imageWidth * imageHeight - 1) / 32 + 2);
  if (bool(simUniforms.inputMask & INPUT_BIT_R)) {
    color.rgb = vec3(exposure.maxIntensity);
  }

  if (isnan(exposure.maxIntensity)) {
    color.rgb = vec3(1.0, 0.0, 0.0);
    return;
  }

  if (bTonemap)
  {
    // TODO: color-grade?
    float clipTop = exposure.maxIntensity;
    float clipBottom = 0.;//exposure.minIntensity;
    // float clipBottom = 0.85 * exposure.minIntensity;

    // exposure.minIntensity = clamp(exposure.minIntensity, 0, 1);
    color.rgb -= vec3(clipBottom);
    color.rgb /= 0.12 * (clipTop - clipBottom);
    // color.rgb /= (clipTop - clipBottom);

    color.rgb = vec3(1.0) - exp(-color.rgb);
    // color.rgb = vec3(1.0) - exp(-color.rgb * 0.5);
    // color.rgb = color.rgb / (color.rgb + vec3(1.0));
  }
}