#include "Simulation.h"

using namespace AltheaEngine;

namespace StableFluids {

Simulation::Simulation(
    Application& app,
    SingleTimeCommandBuffer& commandBuffer) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  // Create texture resources

  // TODO: More efficient texture formats? Where can we afford less precision?

  // Velocity field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16B16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    this->_velocityField.image = Image(app, imageOptions);
    this->_velocityField.view = ImageView(app, this->_velocityField.image, {});
    this->_velocityField.sampler = Sampler(app, {});
  }

  // Advected velocity field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16B16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_advectedVelocityField.image = Image(app, imageOptions);

    // TODO: Are views and samplers needed for storage-only usage?
    this->_advectedVelocityField.view =
        ImageView(app, this->_advectedVelocityField.image, {});
    this->_advectedVelocityField.sampler = Sampler(app, {});
  }

  // Divergence field texture
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16B16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_divergenceField.image = Image(app, imageOptions);

    // TODO: Are views and samplers needed for storage-only usage?
    this->_divergenceField.view =
        ImageView(app, this->_divergenceField.image, {});
    this->_divergenceField.sampler = Sampler(app, {});
  }

  // Pressure field textures
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R16G16B16_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    this->_pressureFieldA.image = Image(app, imageOptions);
    this->_pressureFieldA.view =
        ImageView(app, this->_pressureFieldA.image, {});
    this->_pressureFieldA.sampler = Sampler(app, {});
    this->_pressureFieldB.image = Image(app, imageOptions);
    this->_pressureFieldB.view =
        ImageView(app, this->_pressureFieldB.image, {});
    this->_pressureFieldB.sampler = Sampler(app, {});
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
    builder.layoutBuilder.addDescriptorSet(
        this->_pAdvectMaterialAllocator->getLayout());

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
    builder.layoutBuilder.addDescriptorSet(
        this->_pDivergenceMaterialAllocator->getLayout());

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
    builder.layoutBuilder.addDescriptorSet(
        this->_pPressureMaterialAllocator->getLayout());

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
    builder.layoutBuilder.addDescriptorSet(
        this->_pUpdateMaterialAllocator->getLayout());

    this->_pUpdateVelocityPass =
        std::make_unique<ComputePipeline>(app, std::move(builder));
  }
}

} // namespace StableFluids