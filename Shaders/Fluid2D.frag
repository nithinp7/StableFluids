#version 450

#include "SimulationCommon.glsl"

layout(location=0) in vec2 screenUV;

layout(location=0) out vec4 outColor;
layout(location=1) out vec4 outHdrColor;

#define imageWidth push.params0
#define imageHeight push.params1

void main() {
  vec2 vel = texture(velocityFieldTexture, screenUV).rg;
  float pres = texture(pressureFieldTexture, screenUV).r;
  float f = texture(fractalTexture, screenUV).r;
  float f2 = f * f;//100.0 * f * f;

  // By default show color field
  bool bTonemap = true;
  vec3 color = texture(colorFieldTexture, screenUV).rgb;

  if (screenUV.x < 0.5)
  {
    // Show velocity
    if (bool(simUniforms.inputMask & INPUT_BIT_V)) {
      color = vec3(length(vel));
      bTonemap = false;
    }

    // Show fractal
    if (bool(simUniforms.inputMask & INPUT_BIT_F)) {
      color = vec3(f2);
      bTonemap = false;
    }

    // Show pressure
    if (bool(simUniforms.inputMask & INPUT_BIT_P)) {
      color = vec3(100. * abs(pres));
      bTonemap = false;
    }
  }

  AutoExposure exposure = getAutoExposureEntry((imageWidth * imageHeight - 1) / 32 + 2);

  if (bTonemap)
  {
    // TODO: color-grade?
    float top = exposure.maxIntensity;
    float bottom = exposure.minIntensity;

    float rangeCenter = 0.5 * top + 0.5 * bottom;
    float range = clamp(top - bottom, 0.0, 500  );
    // float clipBottom = 0.85 * exposure.minIntensity;
    float clipTop = rangeCenter + 0.5 * range;
    float clipBottom = rangeCenter - 0.5 * range;

    clipTop = top;
    clipBottom = bottom;

    outHdrColor = vec4(color, 1.0);
    // exposure.minIntensity = clamp(exposure.minIntensity, 0, 1);
    color -= vec3(clipBottom);
    color /= 0.1 * (clipTop - clipBottom);
    // color.rgb /= 0.4 * (clipTop - clipBottom);

    // outHdrColor = vec4(color, 1.0);

    color = vec3(1.0) - exp(-color.rgb);
    // color.rgb = vec3(1.0) - exp(-color.rgb * 0.5);
    // color.rgb = color.rgb / (color.rgb + vec3(1.0));
    outColor = vec4(color, 1.0);
  } else {
    outColor = vec4(color, 1.0);
    outHdrColor = vec4(color, 1.0); // ???    
  }
}