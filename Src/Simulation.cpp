#include "Simulation.h"

#include <cassert>
#include <cstdint>

using namespace AltheaEngine;

namespace {
struct SimulationConstants {
  int width;
  int height;
  float time;
  float dt;
  float sorOmega;
  float density;
  float vorticity;
  bool clear;
};
} // namespace

namespace StableFluids {

Simulation::Simulation(
    Application& app,
    SingleTimeCommandBuffer& commandBuffer) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  // Create texture resources

  // TODO: More efficient texture formats? Where can we afford less precision?
  // Is 16-bit enough floating-point precision for all these stages?

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
    imageOptions.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R8G8B8A8_UNORM;

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

  // Velocity advection pass
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        // Original velocity field
        .addTextureBinding(VK_SHADER_STAGE_COMPUTE_BIT)
        // Advected velocity field
        .addStorageImageBinding();

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
            this->_advectedVelocityField.sampler);

    // Build pipeine
    ComputePipelineBuilder builder{};
    builder.setComputeShader(
        GProjectDirectory + "/Shaders/AdvectVelocity.comp");
    builder.layoutBuilder
        .addDescriptorSet(this->_pAdvectMaterialAllocator->getLayout())
        .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

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
        .addDescriptorSet(this->_pDivergenceMaterialAllocator->getLayout())
        .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

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
        .addDescriptorSet(this->_pPressureMaterialAllocator->getLayout())
        .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

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
        .addDescriptorSet(this->_pUpdateMaterialAllocator->getLayout())
        .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

    this->_pUpdateVelocityPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }

  // Advect color field
  {
    // Build material
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
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
        .addDescriptorSet(this->_pAdvectColorMaterialAllocator->getLayout())
        .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

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
        .addDescriptorSet(this->_pUpdateColorMaterialAllocator->getLayout())
        .addPushConstants<SimulationConstants>(VK_SHADER_STAGE_COMPUTE_BIT);

    this->_pUpdateColorPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }
}

void Simulation::update(
    const Application& app,
    VkCommandBuffer commandBuffer,
    const FrameContext& frame) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  SimulationConstants constants{};
  constants.width = static_cast<int>(extent.width);
  constants.height = static_cast<int>(extent.height);
  constants.time = static_cast<float>(frame.currentTime);
  constants.dt = frame.deltaTime;
  constants.sorOmega = 1.f;
  constants.density = 0.5f;
  constants.vorticity = 0.5f;
  constants.clear = this->clear;

  this->clear = false;

  uint32_t groupCountX = (extent.width - 1) / 16 + 1;
  uint32_t groupCountY = (extent.height - 1) / 16 + 1;

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
    VkDescriptorSet material = this->_pAdvectMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pAdvectPass->getLayout(),
        0,
        1,
        &material,
        0,
        nullptr);
    vkCmdPushConstants(
        commandBuffer,
        this->_pAdvectPass->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationConstants),
        &constants);
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
    VkDescriptorSet material = this->_pDivergenceMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pDivergencePass->getLayout(),
        0,
        1,
        &material,
        0,
        nullptr);
    vkCmdPushConstants(
        commandBuffer,
        this->_pDivergencePass->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationConstants),
        &constants);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Calculate pressure passes
  {
    this->_pPressurePass->bindPipeline(commandBuffer);
    vkCmdPushConstants(
        commandBuffer,
        this->_pPressurePass->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationConstants),
        &constants);

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

      VkDescriptorSet material =
          phase ? this->_pPressureMaterialB->getVkDescriptorSet()
                : this->_pPressureMaterialA->getVkDescriptorSet();
      vkCmdBindDescriptorSets(
          commandBuffer,
          VK_PIPELINE_BIND_POINT_COMPUTE,
          this->_pPressurePass->getLayout(),
          0,
          1,
          &material,
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
    VkDescriptorSet material = this->_pUpdateMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pUpdateVelocityPass->getLayout(),
        0,
        1,
        &material,
        0,
        nullptr);
    vkCmdPushConstants(
        commandBuffer,
        this->_pUpdateVelocityPass->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationConstants),
        &constants);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Advect color field
  {
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
    VkDescriptorSet material =
        this->_pAdvectColorMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pAdvectColorPass->getLayout(),
        0,
        1,
        &material,
        0,
        nullptr);
    vkCmdPushConstants(
        commandBuffer,
        this->_pAdvectColorPass->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationConstants),
        &constants);
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
    VkDescriptorSet material =
        this->_pUpdateColorMaterial->getVkDescriptorSet();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        this->_pUpdateColorPass->getLayout(),
        0,
        1,
        &material,
        0,
        nullptr);
    vkCmdPushConstants(
        commandBuffer,
        this->_pUpdateColorPass->getLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(SimulationConstants),
        &constants);
    vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
  }

  // Transition resources for visualizing in fragment shader
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
} // namespace StableFluids