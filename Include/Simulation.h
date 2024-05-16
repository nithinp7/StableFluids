#pragma once

#include <Althea/Application.h>
#include <Althea/ComputePipeline.h>
#include <Althea/DescriptorSet.h>
#include <Althea/DynamicBuffer.h>
#include <Althea/GlobalHeap.h>
#include <Althea/ImageResource.h>
#include <Althea/PerFrameResources.h>
#include <Althea/RenderPass.h>
#include <Althea/SingleTimeCommandBuffer.h>
#include <Althea/TransientUniforms.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <memory>

using namespace AltheaEngine;

namespace StableFluids {
struct Particle {
  glm::vec3 position;
  glm::vec3 velocity;
};

struct SimulationPushConstants {
  uint32_t simUniforms;
  uint32_t params0;
  uint32_t params1;
  uint32_t params2;
};

struct SimulationUniforms {
  float offsetX;
  float offsetY;
  float lastOffsetX;
  float lastOffsetY;

  float zoom;
  float lastZoom;
  int width;
  int height;

  float time;
  float dt;
  float sorOmega;
  float density;

  float vorticity;
  uint32_t flags;
  uint32_t inputMask;
  uint32_t padding2;

  uint32_t fractalTexture;
  uint32_t velocityFieldTexture;
  uint32_t colorFieldTexture;
  uint32_t divergenceFieldTexture;

  uint32_t pressureFieldTexture;
  uint32_t advectedColorFieldImage;
  uint32_t advectedVelocityFieldImage;
  uint32_t divergenceFieldImage;

  uint32_t pressureFieldImage;
  uint32_t fractalImage;
  uint32_t velocityFieldImage;
  uint32_t iterationCountsImage;

  uint32_t colorFieldImage;
};

class Simulation {
public:
  Simulation() = default;
  Simulation(
      Application& app,
      SingleTimeCommandBuffer& commandBuffer,
      GlobalHeap& heap);
  void update(
      const Application& app,
      VkCommandBuffer commandBuffer,
      VkDescriptorSet heapSet,
      const FrameContext& frame);

  void tryRecompileShaders(Application& app);

  const ImageResource& getFractalIterations() const {
    return this->_iterationCounts;
  }

  const ImageResource& getFractalTexture() const {
    return this->_fractalTexture;
  }

  const ImageResource& getVelocityTexture() const {
    return this->_velocityField;
  }

  const ImageResource& getDivergenceTexture() const {
    return this->_divergenceField;
  }

  const ImageResource& getPressureTexture() const {
    return this->_pressureFieldA;
  }

  const ImageResource& getColorTexture() const { return this->_colorFieldA; }

  UniformHandle getSimUniforms(const FrameContext& frame) const {
    return _simulationUniforms.getCurrentHandle(frame);
  }
  
  bool clear = true;
  float zoom = 1.0f;
  glm::vec2 offset = glm::vec2(-0.706835, 0.235839);
  glm::vec2 targetPanDir = glm::vec2(0.0f);
  float targetZoomDir = 0.0f;

private:
  float _lastZoom = 0.0f;
  glm::vec2 _lastOffset = glm::vec2(0.0f);

  glm::vec2 _velocity2D = glm::vec2(0.0f);
  float _velocityZoom = 0.0f;
  float _targetSpeed2D = 0.5f;
  float _targetZoomSpeed = 0.5f;

  float _accelerationMag2D = 0.0f;
  float _accelerationMagZoom = 0.0f;

  float _targetZoomDir = 0.0f;

  float _maxSpeed2D = 1.0f;
  float _maxZoomSpeed = 1.0f;

  float _velocitySettleTime = 1.0f;

  // Simulation uniforms
  TransientUniforms<SimulationUniforms> _simulationUniforms;

  // Fractal pass
  ImageResource _iterationCounts{};
  ImageResource _fractalTexture{};
  ComputePipeline _fractalPass;

  // Velocity advection pass
  ImageResource _velocityField{};
  ImageResource _advectedVelocityField{};
  ComputePipeline _advectPass;

  // Divergence calculation pass
  ImageResource _divergenceField{};
  ComputePipeline _divergencePass;

  // Pressure calculation pass
  // Ping-pong buffers for pressure computation
  ImageResource _pressureFieldA{};
  ImageResource _pressureFieldB{};
  ComputePipeline _pressurePass;

  // Velocity update pass
  ImageResource _colorFieldA{};
  ImageResource _colorFieldB{};
  ComputePipeline _updateVelocityPass;

  // Advect color dye
  ComputePipeline _advectColorPass;

  // Udate color field
  ComputePipeline _updateColorPass;

  // Particle storage buffer
  DynamicBuffer _particles{};
  ComputePipeline _updateParticlesPass;
};
} // namespace StableFluids