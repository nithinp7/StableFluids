#include "Simulation.h"

#include <cassert>
#include <cstdint>

#include <iostream>

using namespace AltheaEngine;

namespace StableFluids {

Simulation::Simulation(
    Application& app,
    SingleTimeCommandBuffer& commandBuffer) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  // Simulation uniforms
  {
    this->_pSimulationUniforms = std::make_unique<TransientUniforms<SimulationUniforms>>(app);
    
    DescriptorSetLayoutBuilder builder{};
    builder.addUniformBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT);

    this->_pSimulationResources = std::make_unique<PerFrameResources>(app, builder);
    this->_pSimulationResources->assign().bindTransientUniforms(*this->_pSimulationUniforms);
  }
  // Create texture resources

  // TODO: More efficient texture formats? Where can we afford less precision?
  // Is 16-bit enough floating-point precision for all these stages?

  // Iteration counts texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R32_SINT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_iterationCounts.image = Image(app, imageOptions);

    ImageViewOptions viewOptions{};
    viewOptions.format = imageOptions.format;
    this->_iterationCounts.view =
        ImageView(app, this->_iterationCounts.image, viewOptions);

    SamplerOptions samplerOptions{};
    samplerOptions.normalized = false;
    samplerOptions.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    this->_iterationCounts.sampler = Sampler(app, samplerOptions);
  }

  // Fractal texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R32_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    this->_fractalTexture.image = Image(app, imageOptions);

    ImageViewOptions viewOptions{};
    viewOptions.format = imageOptions.format;
    this->_fractalTexture.view =
        ImageView(app, this->_fractalTexture.image, viewOptions);

    this->_fractalTexture.sampler = Sampler(app, {});
  }

  // Velocity field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    this->_velocityField.image = Image(app, imageOptions);

    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R16G16_SFLOAT;
    this->_velocityField.view =
        ImageView(app, this->_velocityField.image, viewOptions);

    SamplerOptions samplerOptions{};
    samplerOptions.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerOptions.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    this->_velocityField.sampler = Sampler(app, samplerOptions);
  }

  // Advected velocity field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_advectedVelocityField.image = Image(app, imageOptions);

    // TODO: Are views and samplers needed for storage-only usage?
    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R16G16_SFLOAT;
    this->_advectedVelocityField.view =
        ImageView(app, this->_advectedVelocityField.image, viewOptions);
    this->_advectedVelocityField.sampler = Sampler(app, {});
  }

  // Divergence field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    this->_divergenceField.image = Image(app, imageOptions);

    // TODO: Are views and samplers needed for storage-only usage?
    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R16_SFLOAT;
    this->_divergenceField.view =
        ImageView(app, this->_divergenceField.image, viewOptions);
    this->_divergenceField.sampler = Sampler(app, {});
  }

  // Pressure field textures
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R16_SFLOAT;

    this->_pressureFieldA.image = Image(app, imageOptions);
    this->_pressureFieldA.view =
        ImageView(app, this->_pressureFieldA.image, viewOptions);
    this->_pressureFieldA.sampler = Sampler(app, {});

    // Only one of the ping-pong buffers needs to be capable of being sampled in
    // the fragment shader
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_pressureFieldB.image = Image(app, imageOptions);
    this->_pressureFieldB.view =
        ImageView(app, this->_pressureFieldB.image, viewOptions);
    this->_pressureFieldB.sampler = Sampler(app, {});
  }

  // Color field textures
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R32G32B32A32_SFLOAT;

    SamplerOptions samplerOptions{};
    samplerOptions.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerOptions.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    this->_colorFieldA.image = Image(app, imageOptions);
    this->_colorFieldA.view =
        ImageView(app, this->_colorFieldA.image, viewOptions);
    this->_colorFieldA.sampler = Sampler(app, samplerOptions);

    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;

    this->_colorFieldB.image = Image(app, imageOptions);
    this->_colorFieldB.view =
        ImageView(app, this->_colorFieldB.image, viewOptions);
    this->_colorFieldB.sampler = Sampler(app, {});
  }

  // Create compute passes

  // Fractal pass
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Fractal iteration counts
        .addStorageImageBinding()
        // Fractal texture
        .addStorageImageBinding();

    this->_pFractalMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
    this->_pFractalMaterial = std::make_unique<DescriptorSet>(
        this->_pFractalMaterialAllocator->allocate());
    this->_pFractalMaterial->assign()
        .bindStorageImage(
            this->_iterationCounts.view,
            this->_iterationCounts.sampler)
        .bindStorageImage(
            this->_fractalTexture.view,
            this->_fractalTexture.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(GProjectDirectory + "/Shaders/Mandelbrot.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pFractalMaterialAllocator->getLayout());

    this->_pFractalPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Velocity advection pass
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Original velocity field
        .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
        // Advected velocity field
        .addStorageImageBinding()
        // Color field
        .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT);

    this->_pAdvectMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
    this->_pAdvectMaterial = std::make_unique<DescriptorSet>(
        this->_pAdvectMaterialAllocator->allocate());
    this->_pAdvectMaterial->assign()
        .bindTextureDescriptor(
            this->_velocityField.view,
            this->_velocityField.sampler)
        .bindStorageImage(
            this->_advectedVelocityField.view,
            this->_advectedVelocityField.sampler)
        .bindTextureDescriptor(
            this->_colorFieldA.view,
            this->_colorFieldA.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(
        GProjectDirectory + "/Shaders/AdvectVelocity.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pAdvectMaterialAllocator->getLayout());

    this->_pAdvectPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Divergence calculation pass
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Advected velocity field
        .addStorageImageBinding()
        // Divergence field
        .addStorageImageBinding();

    this->_pDivergenceMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
    this->_pDivergenceMaterial = std::make_unique<DescriptorSet>(
        this->_pDivergenceMaterialAllocator->allocate());
    this->_pDivergenceMaterial->assign()
        .bindStorageImage(
            this->_advectedVelocityField.view,
            this->_advectedVelocityField.sampler)
        .bindStorageImage(
            this->_divergenceField.view,
            this->_divergenceField.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(
        GProjectDirectory + "/Shaders/CalculateDivergence.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pDivergenceMaterialAllocator->getLayout());

    this->_pDivergencePass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Pressure calculation pass
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Divergence field
        .addStorageImageBinding()
        // Previous pressure field approximation
        .addStorageImageBinding()
        // Updated pressure field
        .addStorageImageBinding();

    this->_pPressureMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 2);
    // Two materials for the two phases of the ping-pong-buffered pressure
    // computation
    this->_pPressureMaterialA = std::make_unique<DescriptorSet>(
        this->_pPressureMaterialAllocator->allocate());
    this->_pPressureMaterialA->assign()
        .bindStorageImage(
            this->_divergenceField.view,
            this->_divergenceField.sampler)
        .bindStorageImage(
            this->_pressureFieldA.view,
            this->_pressureFieldA.sampler)
        .bindStorageImage(
            this->_pressureFieldB.view,
            this->_pressureFieldB.sampler);

    this->_pPressureMaterialB = std::make_unique<DescriptorSet>(
        this->_pPressureMaterialAllocator->allocate());
    this->_pPressureMaterialB->assign()
        .bindStorageImage(
            this->_divergenceField.view,
            this->_divergenceField.sampler)
        .bindStorageImage(
            this->_pressureFieldB.view,
            this->_pressureFieldB.sampler)
        .bindStorageImage(
            this->_pressureFieldA.view,
            this->_pressureFieldA.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(
        GProjectDirectory + "/Shaders/CalculatePressure.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pPressureMaterialAllocator->getLayout());

    this->_pPressurePass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Velocity update pass
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Pressure field
        .addStorageImageBinding()
        // Advected velocity field
        .addStorageImageBinding()
        // "Fixed" velocity field
        .addStorageImageBinding();

    this->_pUpdateMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
    this->_pUpdateMaterial = std::make_unique<DescriptorSet>(
        this->_pUpdateMaterialAllocator->allocate());
    this->_pUpdateMaterial->assign()
        .bindStorageImage(
            this->_pressureFieldA.view,
            this->_pressureFieldA.sampler)
        .bindStorageImage(
            this->_advectedVelocityField.view,
            this->_advectedVelocityField.sampler)
        .bindStorageImage(
            this->_velocityField.view,
            this->_velocityField.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(
        GProjectDirectory + "/Shaders/UpdateVelocity.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pUpdateMaterialAllocator->getLayout());

    this->_pUpdateVelocityPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Advect color field
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Fractal texture
        .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
        // Velocity field
        .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
        // Previous color field
        .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
        // Advected color field
        .addStorageImageBinding();

    this->_pAdvectColorMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
    this->_pAdvectColorMaterial = std::make_unique<DescriptorSet>(
        this->_pAdvectColorMaterialAllocator->allocate());
    this->_pAdvectColorMaterial->assign()
        .bindTextureDescriptor(
            this->_fractalTexture.view,
            this->_fractalTexture.sampler)
        .bindTextureDescriptor(
            this->_velocityField.view,
            this->_velocityField.sampler)
        .bindTextureDescriptor(
            this->_colorFieldA.view,
            this->_colorFieldA.sampler)
        .bindStorageImage(this->_colorFieldB.view, this->_colorFieldB.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(GProjectDirectory + "/Shaders/AdvectColor.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pAdvectColorMaterialAllocator->getLayout());

    this->_pAdvectColorPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Copy color field (TODO: Do diffusion step as well?)
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Temporary color field
        .addStorageImageBinding()
        // Target color field
        .addStorageImageBinding();

    this->_pUpdateColorMaterialAllocator =
        std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
    this->_pUpdateColorMaterial = std::make_unique<DescriptorSet>(
        this->_pUpdateColorMaterialAllocator->allocate());
    this->_pUpdateColorMaterial->assign()
        .bindStorageImage(this->_colorFieldB.view, this->_colorFieldB.sampler)
        .bindStorageImage(this->_colorFieldA.view, this->_colorFieldA.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(GProjectDirectory + "/Shaders/CopyColors.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pSimulationResources->getLayout())
        .addDescriptorSet(this->_pUpdateColorMaterialAllocator->getLayout());

    this->_pUpdateColorPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Particle update pass
  // {
    
  //   // Build material
  //   DescriptorSetLayoutBuilder layoutBuilder{};
  //   layoutBuilder
  //       // Particle positions
  //       .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT)
  //       // Velocity field
  //       .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
  //       // Previous color field
  //       .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
  //       // Advected color field
  //       .addStorageImageBinding();

  //   this->_pAdvectColorMaterialAllocator =
  //       std::make_unique<DescriptorSetAllocator>(app, layoutBuilder, 1);
  //   this->_pAdvectColorMaterial = std::make_unique<DescriptorSet>(
  //       this->_pAdvectColorMaterialAllocator->allocate());
  //   this->_pAdvectColorMaterial->assign()
  //       .bindTextureDescriptor(
  //           this->_fractalTexture.view,
  //           this->_fractalTexture.sampler)
  //       .bindTextureDescriptor(
  //           this->_velocityField.view,
  //           this->_velocityField.sampler)
  //       .bindTextureDescriptor(
  //           this->_colorFieldA.view,
  //           this->_colorFieldA.sampler)
  //       .bindStorageImage(this->_colorFieldB.view, this->_colorFieldB.sampler);

  //   // Build pipeine
  //   ComputePipelineBuilder builder{};
  //   builder.setComputeShader(GProjectDirectory + "/Shaders/AdvectColor.comp");
  //   builder.layoutBuilder
  //       .addDescriptorSet(this->_pAdvectColorMaterialAllocator->getLayout())
  //       .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

  //   this->_pAdvectColorPass =
  //       std::make_unique<ComputePipeline>(app, std::move(builder));
  // }
}

void Simulation::update(
    const Application& app,
    VkCommandBuffer commandBuffer,
    const FrameContext& frame) {
  // TODO: Refactor this out into generalized 2D controller
  float deltaTime = glm::clamp(frame.deltaTime, 0.0f, 1.0f / 30.0f);

  // this->_targetSpeed2D = 
  //     glm::clamp(this->_targetSpeed2D + this->_accelerationMag2D * deltaTime, 0.0f, this->_maxSpeed2D);
  
  glm::vec2 targetVelocity;
  float dirMag = glm::length(this->targetPanDir);
  if (dirMag < 0.001f)
  {
    targetVelocity = glm::vec2(0.0f);
  } else {
    targetVelocity = this->_targetSpeed2D * this->targetPanDir;//glm::normalize(this->targetPanDir);
  }

  float targetZoomVelocity;
  if (abs(targetZoomDir) < 0.001f) {
    targetZoomVelocity = 0.0f;
  } else {
    targetZoomVelocity = this->_targetZoomSpeed * glm::sign(this->targetZoomDir);
  }

  // Velocity feedback controller
  float K = 4.0f / this->_velocitySettleTime;
  glm::vec2 acceleration = K * (targetVelocity - this->_velocity2D);
  float zoomAcceleration = 1.0f * (targetZoomVelocity - this->_velocityZoom);

  this->_velocity2D += acceleration * deltaTime;
  this->_velocityZoom += zoomAcceleration * deltaTime;

  this->zoom *= glm::pow(2.0, this->_velocityZoom * deltaTime);
  this->offset += this->_velocity2D / this->zoom * deltaTime;

  const VkExtent2D& extent = app.getSwapChainExtent();

  SimulationUniforms uniforms{};
  uniforms.width = static_cast<int>(extent.width);
  uniforms.height = static_cast<int>(extent.height);
  uniforms.time = static_cast<float>(frame.currentTime);
  uniforms.dt = frame.deltaTime;
  uniforms.sorOmega = 1.f;
  uniforms.density = 0.5f;
  uniforms.vorticity = 0.5f;
  uniforms.clear = this->clear;
  uniforms.zoom = this->zoom;
  uniforms.lastZoom = this->_lastZoom;
  uniforms.offsetX = this->offset.x;
  uniforms.offsetY = this->offset.y;
  uniforms.lastOffsetX = this->_lastOffset.x;
  uniforms.lastOffsetY = this->_lastOffset.y;

  this->_pSimulationUniforms->updateUniforms(uniforms, frame);

  this->clear = false;

  uint32_t groupCountX = (extent.width - 1) / 16 + 1;
  uint32_t groupCountY = (extent.height - 1) / 16 + 1;

  VkDescriptorSet materials[2];
  materials[0] = this->_pSimulationResources->getCurrentDescriptorSet(frame);

  // Update fractal pass
  if (this->zoom != this->_lastZoom || this->offset != this->_lastOffset) {
    this->_lastZoom = this->zoom;
    this->_lastOffset = this->offset;

    this->_iterationCounts.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_fractalTexture.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    this->_pFractalPass->bindPipeline(commandBuffer);
    materials[1] = this->_pFractalMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pFractalPass->getLayout(),
        0,
        2,
        materials,
        0,
        nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Advect velocity pass
  {
    this->_velocityField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_advectedVelocityField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    this->_pAdvectPass->bindPipeline(commandBuffer);
    materials[1] = this->_pAdvectMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pAdvectPass->getLayout(),
        0,
        2,
        materials,
        0,
        nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Calculate divergence pass
  {
    this->_advectedVelocityField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_divergenceField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    this->_pDivergencePass->bindPipeline(commandBuffer);
    materials[1] = this->_pDivergenceMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pDivergencePass->getLayout(),
        0,
        2,
        materials,
        0,
        nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Calculate pressure passes
  {
    this->_pPressurePass->bindPipeline(commandBuffer);

    this->_divergenceField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    const uint32_t CALC_PRESSURE_ITERS = 40;
    // Must be an even number of iterations so the ping-pong buffer results end
    // up up in a consistent place (pressureFieldA)
    assert(CALC_PRESSURE_ITERS % 2 == 0);
    for (uint32_t pressureIter = 0; pressureIter < CALC_PRESSURE_ITERS;
         ++pressureIter) {
      uint32_t phase = pressureIter % 2;

      this->_pressureFieldA.image.transitionLayout(
          commandBuffer,
          VK_IMAGE_LAYOUT_GENERAL,
          phase ? VK_ACCESS_SHADER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
      this->_pressureFieldB.image.transitionLayout(
          commandBuffer,
          VK_IMAGE_LAYOUT_GENERAL,
          phase ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_SHADER_WRITE_BIT,
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

      materials[1] =
          phase ? this->_pPressureMaterialB->getVkDescriptorSet()
                : this->_pPressureMaterialA->getVkDescriptorSet();
      vkCmdBindDescriptorSets(
          commandBuffer,
          VK_PIPELINE_BIND_POINT_COMPUTE,
          this->_pPressurePass->getLayout(),
          0,
          2,
          materials,
          0,
          nullptr);
      vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
    }
  }

  // Update velocity pass
  {
    this->_pressureFieldA.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_velocityField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    // Note: The advected velocity field texture has already been transitioned
    // for reading by this point.

    this->_pUpdateVelocityPass->bindPipeline(commandBuffer);
    materials[1] = this->_pUpdateMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pUpdateVelocityPass->getLayout(),
        0,
        2,
        materials,
        0,
        nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Advect color field
  {
    this->_fractalTexture.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_velocityField.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_colorFieldA.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_colorFieldB.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    this->_pAdvectColorPass->bindPipeline(commandBuffer);
    materials[1] = this->_pAdvectColorMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pAdvectColorPass->getLayout(),
        0,
        2,
        materials,
        0,
        nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Copy color texture
  {
    this->_colorFieldA.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    this->_colorFieldB.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    this->_pUpdateColorPass->bindPipeline(commandBuffer);
    materials[1] = this->_pUpdateColorMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pUpdateColorPass->getLayout(),
        0,
        2,
        materials,
        0,
        nullptr);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Transition resources for visualizing in fragment shader
  this->_iterationCounts.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  this->_fractalTexture.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  this->_pressureFieldA.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  this->_divergenceField.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  this->_velocityField.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  this->_colorFieldA.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void Simulation::tryRecompileShaders(Application& app) {
  if (this->_pFractalPass->recompileStaleShaders()) {
    if (this->_pFractalPass->hasShaderRecompileErrors()) {
      std::cout << this->_pFractalPass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pFractalPass->recreatePipeline(app);
    }
  }

   if (this->_pAdvectPass->recompileStaleShaders()) {
    if (this->_pAdvectPass->hasShaderRecompileErrors()) {
      std::cout << this->_pAdvectPass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pAdvectPass->recreatePipeline(app);
    }
  }

   if (this->_pFractalPass->recompileStaleShaders()) {
    if (this->_pFractalPass->hasShaderRecompileErrors()) {
      std::cout << this->_pFractalPass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pFractalPass->recreatePipeline(app);
    }
  }

   if (this->_pDivergencePass->recompileStaleShaders()) {
    if (this->_pDivergencePass->hasShaderRecompileErrors()) {
      std::cout << this->_pDivergencePass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pDivergencePass->recreatePipeline(app);
    }
  }

   if (this->_pPressurePass->recompileStaleShaders()) {
    if (this->_pPressurePass->hasShaderRecompileErrors()) {
      std::cout << this->_pPressurePass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pPressurePass->recreatePipeline(app);
    }
  }

   if (this->_pUpdateVelocityPass->recompileStaleShaders()) {
    if (this->_pUpdateVelocityPass->hasShaderRecompileErrors()) {
      std::cout << this->_pUpdateVelocityPass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pUpdateVelocityPass->recreatePipeline(app);
    }
  }

   if (this->_pAdvectColorPass->recompileStaleShaders()) {
    if (this->_pAdvectColorPass->hasShaderRecompileErrors()) {
      std::cout << this->_pAdvectColorPass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pAdvectColorPass->recreatePipeline(app);
    }
  }
  
   if (this->_pUpdateColorPass->recompileStaleShaders()) {
    if (this->_pUpdateColorPass->hasShaderRecompileErrors()) {
      std::cout << this->_pUpdateColorPass->getShaderRecompileErrors() << "\n";
    } else {
      this->_pUpdateColorPass->recreatePipeline(app);
    }
  }

  // if (this->_pUpdateParticlesPass->recompileStaleShaders()) {
  //   if (this->_pUpdateParticlesPass->hasShaderRecompileErrors()) {
  //     std::cout << this->_pUpdateParticlesPass->getShaderRecompileErrors() << "\n";
  //   } else {
  //     this->_pUpdateParticlesPass->recreatePipeline(app);
  //   }
  // }
}
} // namespace StableFluids