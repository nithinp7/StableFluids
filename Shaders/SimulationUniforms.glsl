
layout(set=0, binding=0) uniform SimulationUniforms {
  vec2 offset;
  vec2 lastOffset;
  float zoom;
  float lastZoom;
  int width;
  int height;
  float time;
  float dt;
  float sorOmega;
  float density;
  float vorticity;
  bool clear;
} simulation;