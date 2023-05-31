#pragma once

#include <Althea/Application.h>
#include <Althea/ComputePipeline.h>
#include <Althea/DescriptorSet.h>
#include <Althea/ImageResource.h>
#include <Althea/SingleTimeCommandBuffer.h>

#include <memory>

using namespace AltheaEngine;

namespace StableFluids {
class Simulation {
public:
  Simulation(Application& app, SingleTimeCommandBuffer& commandBuffer);

  const ImageResource& getVelocityTexture() const {
    return this->_velocityField;
  }

  const ImageResource& getDivergenceTexture() const {
    return this->_divergenceField;
  }

  const ImageResource& getPressureTexture() const {
    return this->_pressureFieldA;
  }

private:
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
  std::unique_ptr<DescriptorSetAllocator> _pUpdateMaterialAllocator;
  std::unique_ptr<DescriptorSet> _pUpdateMaterial;
  std::unique_ptr<ComputePipeline> _pUpdateVelocityPass;
};
} // namespace StableFluids