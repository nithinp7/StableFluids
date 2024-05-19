#ifndef _SIMULATIONCOMMON_
#define _SIMULATIONCOMMON_

#include <Bindless/GlobalHeap.glsl>
#include <Misc/Input.glsl>

layout(push_constant) uniform PushConstant {
  uint simUniforms;
  uint params0;
  uint params1;
  uint params2;
} push;

UNIFORM_BUFFER(_simulationUniforms, SimulationUniforms{
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
  uint flags;
  uint inputMask;
  uint padding2;

  uint fractalTexture;
  uint velocityFieldTexture;
  uint colorFieldTexture;
  uint divergenceFieldTexture;

  uint pressureFieldTexture;
  uint advectedColorFieldImage;
  uint advectedVelocityFieldImage;
  uint divergenceFieldImage;

  uint pressureFieldImage;
  uint fractalImage;
  uint velocityFieldImage;
  uint iterationCountsImage;

  uint colorFieldImage;
  uint autoExposureBuffer;
});
#define simUniforms _simulationUniforms[push.simUniforms]

SAMPLER2D(_textureHeap);
IMAGE2D_RW(_rgba32fimageHeap, rgba32f);
IMAGE2D_RW(_r32fimageHeap, r32f);
IMAGE2D_RW(_rg16fimageHeap, rg16f);
IMAGE2D_RW(_r16fimageHeap, r16f);
DECL_IMAGE_HEAP(uniform iimage2D _iimageHeap, r32i);

struct AutoExposure {
  float minIntensity;
  float maxIntensity;  
};

BUFFER_RW(_autoExposureBuffer, AutoExposureBuffer{
  AutoExposure entries[];
});
#define getAutoExposureEntry(idx)   _autoExposureBuffer[simUniforms.autoExposureBuffer].entries[idx]

#define fractalTexture              _textureHeap[simUniforms.fractalTexture]
#define velocityFieldTexture        _textureHeap[simUniforms.velocityFieldTexture]
#define colorFieldTexture           _textureHeap[simUniforms.colorFieldTexture]
#define divergenceFieldTexture      _textureHeap[simUniforms.divergenceFieldTexture]

#define pressureFieldTexture        _textureHeap[simUniforms.pressureFieldTexture]
#define advectedColorFieldImage     _rgba32fimageHeap[simUniforms.advectedColorFieldImage]
#define advectedVelocityFieldImage  _rg16fimageHeap[simUniforms.advectedVelocityFieldImage]
#define divergenceFieldImage        _r16fimageHeap[simUniforms.divergenceFieldImage]

#define fractalImage                _r32fimageHeap[simUniforms.fractalImage]
#define velocityFieldImage          _rg16fimageHeap[simUniforms.velocityFieldImage]

#define iterationCountsImage        _iimageHeap[simUniforms.iterationCountsImage]

#define isClearFlagSet() bool(simUniforms.flags & 1) 

#endif // _SIMULATIONCOMMON_