#include "Simulation.h"

#include <Althea/InputMask.h>

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace AltheaEngine;

namespace StableFluids {

static ComputePipeline createComputePass(
    const char* shaderRelativePath,
    const Application& app,
    const GlobalHeap& heap) {
  ComputePipelineBuilder builder{};
  builder.setComputeShader(GProjectDirectory + shaderRelativePath);
  builder.layoutBuilder.addDescriptorSet(heap.getDescriptorSetLayout())
      .addPushConstants<SimulationPushConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

  return ComputePipeline(app, std::move(builder));
}

Simulation::Simulation(
    Application& app,
    SingleTimeCommandBuffer& commandBuffer,
    GlobalHeap& heap) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  this->_simulationUniforms = TransientUniforms<SimulationUniforms>(app);
  this->_simulationUniforms.registerToHeap(heap);

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

    this->_iterationCounts.registerToImageHeap(heap);
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

    this->_fractalTexture.registerToImageHeap(heap);
    this->_fractalTexture.registerToTextureHeap(heap);
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

    this->_velocityField.registerToImageHeap(heap);
    this->_velocityField.registerToTextureHeap(heap);
  }

  // Advected velocity field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_advectedVelocityField.image = Image(app, imageOptions);

    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R16G16_SFLOAT;
    this->_advectedVelocityField.view =
        ImageView(app, this->_advectedVelocityField.image, viewOptions);
    this->_advectedVelocityField.sampler = Sampler(app, {});

    this->_advectedVelocityField.registerToImageHeap(heap);
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

    this->_divergenceField.registerToImageHeap(heap);
    this->_divergenceField.registerToTextureHeap(heap);
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
    this->_pressureFieldB.image = Image(app, imageOptions);

    this->_pressureFieldA.view =
        ImageView(app, this->_pressureFieldA.image, viewOptions);
    this->_pressureFieldB.view =
        ImageView(app, this->_pressureFieldB.image, viewOptions);

    this->_pressureFieldA.sampler = Sampler(app, {});
    this->_pressureFieldB.sampler = Sampler(app, {});

    this->_pressureFieldA.registerToImageHeap(heap);
    this->_pressureFieldB.registerToImageHeap(heap);
    this->_pressureFieldA.registerToTextureHeap(heap);
    this->_pressureFieldB.registerToTextureHeap(heap);
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
    viewOptions.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    SamplerOptions samplerOptions{};
    samplerOptions.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerOptions.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    this->_colorFieldA.image = Image(app, imageOptions);
    this->_colorFieldB.image = Image(app, imageOptions);

    this->_colorFieldA.view =
        ImageView(app, this->_colorFieldA.image, viewOptions);
    this->_colorFieldB.view =
        ImageView(app, this->_colorFieldB.image, viewOptions);

    this->_colorFieldA.sampler = Sampler(app, samplerOptions);
    this->_colorFieldB.sampler = Sampler(app, samplerOptions);

    this->_colorFieldA.registerToImageHeap(heap);
    this->_colorFieldB.registerToImageHeap(heap);
    this->_colorFieldA.registerToTextureHeap(heap);
    this->_colorFieldB.registerToTextureHeap(heap);
  }

  // Auto exposure
  {
    _autoExposureBuffer = StructuredBuffer<AutoExposure>(app, 2048 * 2048 / 32);
    _autoExposureBuffer.zeroBuffer(commandBuffer);
    _autoExposureBuffer.registerToHeap(heap);
  }

  // Create compute passes

  this->_fractalPass = createComputePass("/Shaders/Mandelbrot.comp", app, heap);
  this->_advectPass =
      createComputePass("/Shaders/AdvectVelocity.comp", app, heap);
  this->_divergencePass =
      createComputePass("/Shaders/CalculateDivergence.comp", app, heap);
  this->_pressurePass =
      createComputePass("/Shaders/CalculatePressure.comp", app, heap);
  this->_updateVelocityPass =
      createComputePass("/Shaders/UpdateVelocity.comp", app, heap);
  this->_advectColorPass =
      createComputePass("/Shaders/AdvectColor.comp", app, heap);
  this->_fractalPass = createComputePass("/Shaders/Mandelbrot.comp", app, heap);
  this->_updateColorPass =
      createComputePass("/Shaders/CopyColors.comp", app, heap);
  this->_autoExposurePass =
      createComputePass("/Shaders/AutoExposure.comp", app, heap);

  // TODO:
  // Particle update pass
}

void Simulation::update(
    const Application& app,
    VkCommandBuffer commandBuffer,
    VkDescriptorSet heapSet,
    const FrameContext& frame) {
  // TODO: Refactor this out into generalized 2D controller
  float deltaTime = glm::clamp(frame.deltaTime, 0.0f, 1.0f / 30.0f);

  uint32_t inputMask = app.getInputManager().getCurrentInputMask();

  // this->_targetSpeed2D =
  //     glm::clamp(this->_targetSpeed2D + this->_accelerationMag2D * deltaTime,
  //     0.0f, this->_maxSpeed2D);
  this->targetPanDir = glm::vec2(0.0f);
  if (inputMask & INPUT_BIT_W)
    this->targetPanDir.y -= 1.0f;
  if (inputMask & INPUT_BIT_S)
    this->targetPanDir.y += 1.0f;
  if (inputMask & INPUT_BIT_A)
    this->targetPanDir.x -= 1.0f;
  if (inputMask & INPUT_BIT_D)
    this->targetPanDir.x += 1.0f;

  glm::vec2 targetVelocity;
  float dirMag = glm::length(this->targetPanDir);
  if (dirMag < 0.001f) {
    targetVelocity = glm::vec2(0.0f);
  } else {
    targetVelocity = this->_targetSpeed2D *
                     this->targetPanDir; // glm::normalize(this->targetPanDir);
  }

  float targetZoomVelocity;
  if (abs(targetZoomDir) < 0.001f) {
    targetZoomVelocity = 0.0f;
  } else {
    targetZoomVelocity =
        this->_targetZoomSpeed * glm::sign(this->targetZoomDir);
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
  uniforms.dt = 1.0f / 30.0f;
  uniforms.sorOmega = 1.f;
  uniforms.density = 0.5f;
  uniforms.vorticity = 0.5f;
  uniforms.flags = this->clear ? 1 : 0;
  uniforms.zoom = this->zoom;
  uniforms.lastZoom = this->_lastZoom;
  uniforms.offsetX = this->offset.x;
  uniforms.offsetY = this->offset.y;
  uniforms.lastOffsetX = this->_lastOffset.x;
  uniforms.lastOffsetY = this->_lastOffset.y;
  uniforms.inputMask = inputMask;

  uniforms.fractalTexture = _fractalTexture.textureHandle.index;
  uniforms.velocityFieldTexture = _velocityField.textureHandle.index;
  uniforms.colorFieldTexture = _colorFieldA.textureHandle.index;
  uniforms.divergenceFieldTexture = _divergenceField.textureHandle.index;

  uniforms.pressureFieldTexture = _pressureFieldA.textureHandle.index;
  uniforms.advectedColorFieldImage = _colorFieldB.imageHandle.index;
  uniforms.advectedVelocityFieldImage =
      _advectedVelocityField.imageHandle.index;
  uniforms.divergenceFieldImage = _divergenceField.imageHandle.index;

  uniforms.pressureFieldImage = _pressureFieldA.imageHandle.index;
  uniforms.fractalImage = _fractalTexture.imageHandle.index;
  uniforms.velocityFieldImage = _velocityField.imageHandle.index;
  uniforms.iterationCountsImage = _iterationCounts.imageHandle.index;

  uniforms.colorFieldImage = _colorFieldA.imageHandle.index;
  uniforms.autoExposureBuffer = _autoExposureBuffer.getHandle().index;

  this->_simulationUniforms.updateUniforms(uniforms, frame);

  this->clear = false;

  uint32_t groupCountX = (extent.width - 1) / 16 + 1;
  uint32_t groupCountY = (extent.height - 1) / 16 + 1;

  SimulationPushConstants push{};
  push.simUniforms = _simulationUniforms.getCurrentHandle(frame).index;

  auto bindCompute = [&](const ComputePipeline& c) {
    c.bindPipeline(commandBuffer);
    vkCmdPushConstants(
        commandBuffer,
        c.getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationPushConstants),
        &push);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        c.getLayout(),
        0,
        1,
        &heapSet,
        0,
        nullptr);
  };

  // Auto-exposure
  {
    this->_colorFieldA.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // groupSize = subgroupSize = 32

    push.params0 = 0;
    push.params1 = extent.width;
    push.params2 = extent.height;

    uint32_t exposureGroupCount = (extent.width * extent.height - 1) / 32 + 1;

    while (true) {
      bindCompute(_autoExposurePass);
      vkCmdDispatch(commandBuffer, exposureGroupCount, 1, 1);
      _autoExposureBarrier(commandBuffer);

      if (exposureGroupCount == 1)
        break;
      else if (exposureGroupCount < 32)
        exposureGroupCount = 1;
      else
        exposureGroupCount >>= 5;

      push.params0++;
    }
  }

  push.params0 = 0;
  push.params1 = 0;
  push.params2 = 0;

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

    bindCompute(_fractalPass);
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

    bindCompute(_advectPass);
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

    bindCompute(_divergencePass);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Calculate pressure passes
  {
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

      push.params0 = phase;
      bindCompute(_pressurePass);
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

    bindCompute(_updateVelocityPass);
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

    bindCompute(_advectColorPass);
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

    bindCompute(_updateColorPass);
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

  _autoExposureBarrier(commandBuffer);
}

void Simulation::_autoExposureBarrier(VkCommandBuffer commandBuffer) {
  VkBufferMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.buffer = _autoExposureBuffer.getAllocation().getBuffer();
  barrier.offset = 0;
  barrier.size = _autoExposureBuffer.getSize();
  barrier.srcAccessMask =
      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask =
      VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
      commandBuffer,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0,
      0,
      nullptr,
      1,
      &barrier,
      0,
      nullptr);
}

void Simulation::tryRecompileShaders(Application& app) {
  this->_fractalPass.tryRecompile(app);
  this->_advectPass.tryRecompile(app);
  this->_fractalPass.tryRecompile(app);
  this->_divergencePass.tryRecompile(app);
  this->_pressurePass.tryRecompile(app);
  this->_updateVelocityPass.tryRecompile(app);
  this->_advectColorPass.tryRecompile(app);
  this->_updateColorPass.tryRecompile(app);
  this->_autoExposurePass.tryRecompile(app);
}
} // namespace StableFluids