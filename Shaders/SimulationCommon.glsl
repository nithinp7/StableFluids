#ifndef _SIMULATIONCOMMON_
#define _SIMULATIONCOMMON_

#include <Bindless/GlobalHeap.glsl>

layout(location=push_constant) uniform PushConstant {
  uint simUniforms;
  uint params0;
  uint params1;
  uint params2;
} push;

BUFFER_RW(_simulationUniforms, SimulationUniforms{
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
  uint padding1;
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
});
#define simUniforms _simulationUniforms[push.simUniforms]

SAMPLER2D(_textureHeap);
IMAGE2D_RW(_imageHeap);
IMAGE2D_RW(_r16imageHeap, r16f);
DECL_IMAGE_HEAP(uniform iimage2D _iimageHeap);

#define fractalTexture              _textureHeap[simUniforms.fractalTexture]
#define velocityFieldTexture        _textureHeap[simUniforms.velocityFieldTexture]
#define colorFieldTexture           _textureHeap[simUniforms.colorFieldTexture]
#define divergenceFieldTexture      _textureHeap[simUniforms.divergenceFieldTexture]

#define pressureFieldTexture        _textureHeap[simUniforms.pressureFieldTexture]
#define advectedColorFieldImage     _imageHeap[simUniforms.advectedColorFieldImage]
#define advectedVelocityFieldImage  _imageHeap[simUniforms.advectedVelocityFieldImage]
#define divergenceFieldImage        _imageHeap[simUniforms.divergenceFieldImage]

// TODO: Fix
#define prevPressureFieldImage      _imageHeap[simUniforms.prevPressureFieldImage]
#define pressureFieldImage          _imageHeap[simUniforms.pressureFieldImage]
#define fractalImage                _imageHeap[simUniforms.fractalImage]
#define velocityFieldImage          _imageHeap[simUniforms.velocityFieldImage]

#define iterationCountsImage        _iimageHeap[simUniforms.iterationCountsImage]

#define isClearFlagSet() bool(simUniforms.flags & 1) 

#endif _SIMULATIONCOMMON_