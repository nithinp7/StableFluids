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
        that->_renderPass.tryRecompile(app);
        that->_simulation.tryRecompileShaders(app);
      });

  app.getInputManager().addKeyBinding(
      {GLFW_KEY_C, GLFW_PRESS, 0},
      [&app, that = this]() { that->_simulation.clear = true; });

  app.getInputManager().addMousePositionCallback(
      [that = this, &app](double mPosX, double mPosY, bool clicked) {
        glm::vec2 fromCenter(mPosX - 1.0, 1.0 - mPosY);
        float d = glm::length(fromCenter);
        that->_simulation.targetPanDir =
            (app.getInputManager().getMouseCursorHidden() && d > 0.4f)
                ? d * fromCenter
                : glm::vec2(0.0f);
      });

  app.getInputManager().addKeyBinding(
      {GLFW_KEY_E, GLFW_PRESS, 0},
      [&app, that = this]() {
        that->_simulation.targetZoomDir =
            glm::clamp(that->_simulation.targetZoomDir + 1.0f, -1.0f, 1.0f);
      });

  app.getInputManager().addKeyBinding(
      {GLFW_KEY_Q, GLFW_PRESS, 0},
      [&app, that = this]() {
        that->_simulation.targetZoomDir =
            glm::clamp(that->_simulation.targetZoomDir - 1.0f, -1.0f, 1.0f);
      });
}

void FluidCanvas2D::shutdownGame(Application& app) {}

void FluidCanvas2D::createRenderState(Application& app) {
  const VkExtent2D& extent = app.getSwapChainExtent();

  SingleTimeCommandBuffer commandBuffer(app);

  _heap = GlobalHeap(app);
  _simulation = Simulation(app, commandBuffer, _heap);

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
        .addDescriptorSet(_heap.getDescriptorSetLayout())
        .addPushConstants<SimulationPushConstants>(
            VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  VkClearValue colorClear;
  colorClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

  std::vector<Attachment> attachments = {
      {ATTACHMENT_FLAG_COLOR,
       app.getSwapChainImageFormat(),
       colorClear,
       true,
       false}};

  this->_renderPass = RenderPass(
      app,
      extent,
      std::move(attachments),
      std::move(subpassBuilders));

  this->_swapChainFrameBuffers =
      SwapChainFrameBufferCollection(app, _renderPass, {});
}

void FluidCanvas2D::destroyRenderState(Application& app) {
  _renderPass = {};
  _swapChainFrameBuffers = {};

  _simulation = {};

  _heap = {};
}

void FluidCanvas2D::tick(Application& app, const FrameContext& frame) {}

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
  VkDescriptorSet heapSet = _heap.getDescriptorSet();
  _simulation.update(app, commandBuffer, heapSet, frame);

  // Render simulation
  {
    ActiveRenderPass pass = _renderPass.begin(
        app,
        commandBuffer,
        frame,
        this->_swapChainFrameBuffers.getCurrentFrameBuffer(frame));
    // Bind global descriptor sets
    pass.setGlobalDescriptorSets(gsl::span(&heapSet, 1));

    SimulationPushConstants push{};
    push.simUniforms = _simulation.getSimUniforms(frame).index;
    pass.getDrawContext().updatePushConstants(push, 0);

    // Draw simulation
    pass.draw(DrawableEnvMap{});
  }
}
} // namespace StableFluids