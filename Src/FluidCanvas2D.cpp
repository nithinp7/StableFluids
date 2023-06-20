#include "FluidCanvas2D.h"

#include <Althea/Application.h>
#include <Althea/Camera.h>
#include <Althea/Cubemap.h>
#include <Althea/DescriptorSet.h>
#include <Althea/GraphicsPipeline.h>
#include <Althea/InputManager.h>
#include <Althea/ModelViewProjection.h>
#include <Althea/Primitive.h>
#include <Althea/SingleTimeCommandBuffer.h>
#include <Althea/Skybox.h>
#include <Althea/Utilities.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace AltheaEngine;

namespace StableFluids {

FluidCanvas2D::FluidCanvas2D() {}

void FluidCanvas2D::initGame(Application& app) {
  const VkExtent2D& windowDims = app.getSwapChainExtent();

  // Recreate any stale pipelines (shader hot-reload)
  app.getInputManager().addKeyBinding(
      {GLFW_KEY_R, GLFW_PRESS, GLFW_MOD_CONTROL},
      [&app, that = this]() {
        for (Subpass& subpass : that->_pRenderPass->getSubpasses()) {
          GraphicsPipeline& pipeline = subpass.getPipeline();
          if (pipeline.recompileStaleShaders()) {
            if (pipeline.hasShaderRecompileErrors()) {
              std::cout << pipeline.getShaderRecompileErrors() << "\n";
            } else {
              pipeline.recreatePipeline(app);
            }
          }
        }
      });

  app.getInputManager().addKeyBinding(
      {GLFW_KEY_C, GLFW_PRESS, 0},
      [&app, that = this]() { that->_pSimulation->clear = true; });

  app.getInputManager().addKeyBinding(
      {GLFW_KEY_W, GLFW_PRESS, 0},
      [&app, that = this]() { that->_pSimulation->zoom *= 2.0f; });

  app.getInputManager().addKeyBinding(
      {GLFW_KEY_S, GLFW_PRESS, 0},
      [&app, that = this]() { that->_pSimulation->zoom *= 0.5f; });
}

void FluidCanvas2D::shutdownGame(Application& app) {}

void FluidCanvas2D::createRenderState(Application& app) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  SingleTimeCommandBuffer commandBuffer(app);
  this->_pSimulation = std::make_unique<Simulation>(app, commandBuffer);
  this->_createGlobalResources(app, commandBuffer);
  this->_createRenderPass(app);
}

void FluidCanvas2D::destroyRenderState(Application& app) {
  this->_pRenderPass.reset();
  this->_swapChainFrameBuffers = {};

  this->_pGlobalResources.reset();
  this->_pGlobalUniforms.reset();

  this->_pSimulation.reset();
}

void FluidCanvas2D::tick(Application& app, const FrameContext& frame) {
  GlobalUniforms globalUniforms;
  globalUniforms.time = frame.currentTime;

  this->_pGlobalUniforms->updateUniforms(globalUniforms, frame);
}

void FluidCanvas2D::_createGlobalResources(
    Application& app,
    SingleTimeCommandBuffer& commandBuffer) {
  // Global resources
  {
    DescriptorSetLayoutBuilder globalResourceLayout;

    // Add texture slots for simulation data
    globalResourceLayout
        // Add texture slot for fractal colors
        .addTextureBinding()
        // Add texture slot for velocity field
        .addTextureBinding()
        // Add texture slot for divergence field
        .addTextureBinding()
        // Add texture slot for pressure field
        .addTextureBinding()
        // Add texture slot for color dye
        .addTextureBinding()
        // Global uniforms
        .addUniformBufferBinding();

    this->_pGlobalResources =
        std::make_unique<PerFrameResources>(app, globalResourceLayout);
    this->_pGlobalUniforms =
        std::make_unique<TransientUniforms<GlobalUniforms>>(app, commandBuffer);

    ResourcesAssignment assignment = this->_pGlobalResources->assign();

    assignment
        // Bind simulation resources
        .bindTexture(this->_pSimulation->getFractalTexture())
        .bindTexture(this->_pSimulation->getVelocityTexture())
        .bindTexture(this->_pSimulation->getDivergenceTexture())
        .bindTexture(this->_pSimulation->getPressureTexture())
        .bindTexture(this->_pSimulation->getColorTexture())

        // Bind global uniforms
        .bindTransientUniforms(*this->_pGlobalUniforms);
  }
}

void FluidCanvas2D::_createRenderPass(Application& app) {
  std::vector<SubpassBuilder> subpassBuilders;

  // Render pass
  {
    SubpassBuilder& subpassBuilder = subpassBuilders.emplace_back();
    subpassBuilder.colorAttachments = {0};

    subpassBuilder
        .pipelineBuilder
        // TODO: Fix the full-screen quad render
        .setFrontFace(VK_FRONT_FACE_CLOCKWISE)
        // Vertex shader
        .addVertexShader(GProjectDirectory + "/Shaders/Fluid2D.vert")
        // Fragment shader
        .addFragmentShader(GProjectDirectory + "/Shaders/Fluid2D.frag")

        // Pipeline resource layouts
        .layoutBuilder
        // Global resources
        .addDescriptorSet(this->_pGlobalResources->getLayout());
  }

  VkClearValue colorClear;
  colorClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  std::vector<Attachment> attachments = {
      {ATTACHMENT_FLAG_COLOR,
       app.getSwapChainImageFormat(),
       colorClear,
       true,
       false}};

  const VkExtent2D& extent = app.getSwapChainExtent();
  this->_pRenderPass = std::make_unique<RenderPass>(
      app,
      extent,
      std::move(attachments),
      std::move(subpassBuilders));

  this->_swapChainFrameBuffers =
      SwapChainFrameBufferCollection(app, *this->_pRenderPass, {});
}

namespace {
struct DrawableEnvMap {
  void draw(const DrawContext& context) const {
    context.bindDescriptorSets();
    context.draw(3);
  }
};
} // namespace

void FluidCanvas2D::draw(
    Application& app,
    VkCommandBuffer commandBuffer,
    const FrameContext& frame) {
  this->_pSimulation->update(app, commandBuffer, frame);

  VkDescriptorSet globalDescriptorSet =
      this->_pGlobalResources->getCurrentDescriptorSet(frame);

  // Render simulation
  {
    ActiveRenderPass pass = this->_pRenderPass->begin(
        app,
        commandBuffer,
        frame,
        this->_swapChainFrameBuffers.getCurrentFrameBuffer(frame));
    // Bind global descriptor sets
    pass.setGlobalDescriptorSets(gsl::span(&globalDescriptorSet, 1));
    // Draw simulation
    pass.draw(DrawableEnvMap{});
  }
}
} // namespace StableFluids