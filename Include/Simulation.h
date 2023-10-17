#pragma once

#include <Althea/Application.h>
#include <Althea/ComputePipeline.h>
#include <Althea/DescriptorSet.h>
#include <Althea/ImageResource.h>
#include <Althea/SingleTimeCommandBuffer.h>
#include <Althea/DynamicBuffer.h>
#include <Althea/TransientUniforms.h>
#include <Althea/PerFrameResources.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <memory>

using namespace AltheaEngine;

namespace StableFluids {
struct Particle {
  glm::vec3 position;
  glm::vec3 velocity;
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
  bool clear;
};

class Simulation {
public:
  Simulation(Application& app, SingleTimeCommandBuffer& commandBuffer);
  void update(
      const Application& app,
      VkCommandBuffer commandBuffer,
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

  bool clear = true;
  float zoom = 1.0f;
  glm::vec2 offset = glm::vec2(-0.706835, 0.235839);

private:
  float _lastZoom = 0.0f;
  glm::vec2 _lastOffset = glm::vec2(0.0f);

  // Simulation uniforms
  std::unique_ptr<TransientUniforms<SimulationUniforms>> _pSimulationUniforms; 
  std::unique_ptr<PerFrameResources> _pSimulationResources;

  // Fractal pass
  ImageResource _iterationCounts{};
  ImageResource _fractalTexture{};
  std::unique_ptr<DescriptorSetAllocator> _pFractalMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pFractalMaterial;
  std::unique_ptr<ComputePipeline> _pFractalPass;

  // Velocity advection pass
  ImageResource _velocityField{};
  ImageResource _advectedVelocityField{};
  std::unique_ptr<DescriptorSetAllocator> _pAdvectMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pAdvectMaterial;
  std::unique_ptr<ComputePipeline> _pAdvectPass;

  // Divergence calculation pass
  ImageResource _divergenceField{};
  std::unique_ptr<DescriptorSetAllocator> _pDivergenceMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pDivergenceMaterial;
  std::unique_ptr<ComputePipeline> _pDivergencePass;

  // Pressure calculation pass
  // Ping-pong buffers for pressure computation
  ImageResource _pressureFieldA{};
  ImageResource _pressureFieldB{};
  std::unique_ptr<DescriptorSetAllocator> _pPressureMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pPressureMaterialA;
  std::unique_ptr<DescriptorSet> _pPressureMaterialB;
  std::unique_ptr<ComputePipeline> _pPressurePass;

  // Velocity update pass
  ImageResource _colorFieldA{};
  ImageResource _colorFieldB{};
  std::unique_ptr<DescriptorSetAllocator> _pUpdateMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pUpdateMaterial;
  std::unique_ptr<ComputePipeline> _pUpdateVelocityPass;

  // Advect color dye
  std::unique_ptr<DescriptorSetAllocator> _pAdvectColorMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pAdvectColorMaterial;
  std::unique_ptr<ComputePipeline> _pAdvectColorPass;
  
  // Udate color field
  std::unique_ptr<DescriptorSetAllocator> _pUpdateColorMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pUpdateColorMaterial;
  std::unique_ptr<ComputePipeline> _pUpdateColorPass;

  // Particle storage buffer
  DynamicBuffer _particles{};
  std::unique_ptr<DescriptorSetAllocator> _pUpdateParticlesMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pUpdateParticlesMaterial;
  std::unique_ptr<ComputePipeline> _pUpdateParticlesPass;
};
} // namespace StableFluids