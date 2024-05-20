#include "FluidCanvas2D.h"

#include <Althea/Application.h>
#include <Althea/Camera.h>
#include <Althea/Cubemap.h>
#include <Althea/DescriptorSet.h>
#include <Althea/GraphicsPipeline.h>
#include <Althea/InputManager.h>
#include <Althea/InputMask.h>
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
#include <thread>
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

  // app.getInputManager().addMousePositionCallback(
  //     [that = this, &app](double mPosX, double mPosY, bool clicked) {
  //       glm::vec2 fromCenter(mPosX - 1.0, 1.0 - mPosY);
  //       float d = glm::length(fromCenter);
  //       that->_simulation.targetPanDir =
  //           (app.getInputManager().getMouseCursorHidden() && d > 0.4f)
  //               ? d * fromCenter
  //               : glm::vec2(0.0f);
  //     });

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

  // hdr buffers
  {
    ImageOptions imageOptions{};
    imageOptions.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageOptions.width = extent.width;
    imageOptions.height = extent.height;
    imageOptions.usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    ImageViewOptions viewOptions{};
    viewOptions.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewOptions.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    SamplerOptions samplerOptions{};

    _hdrImage.image = Image(app, imageOptions);
    _hdrImage.view = ImageView(app, _hdrImage.image, viewOptions);
    _hdrImage.sampler = Sampler(app, samplerOptions);

    VmaAllocationCreateInfo stagingInfo{};
    stagingInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    stagingInfo.usage = VMA_MEMORY_USAGE_AUTO;

    size_t hdrImageSize = sizeof(glm::vec4) * extent.width * extent.height;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      _hdrStagingBuffers[i] = BufferUtilities::createBuffer(
          app,
          hdrImageSize,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          stagingInfo);
    }
  }

  std::vector<SubpassBuilder> subpassBuilders;

  // Render pass
  {
    SubpassBuilder& subpassBuilder = subpassBuilders.emplace_back();
    subpassBuilder.colorAttachments = {0, 1};

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
       false},
      {ATTACHMENT_FLAG_COLOR,
       VK_FORMAT_R32G32B32A32_SFLOAT,
       colorClear,
       false,
       false}};

  this->_renderPass = RenderPass(
      app,
      extent,
      std::move(attachments),
      std::move(subpassBuilders));

  _swapChainFrameBuffers =
      SwapChainFrameBufferCollection(app, _renderPass, {_hdrImage.view});
}

void FluidCanvas2D::destroyRenderState(Application& app) {
  _renderPass = {};
  _swapChainFrameBuffers = {};
  _hdrImage = {};
  _hdrStagingBuffers = {};

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
  VkExtent2D extent = app.getSwapChainExtent();

  VkDescriptorSet heapSet = _heap.getDescriptorSet();
  _simulation.update(app, commandBuffer, heapSet, frame);

  _hdrImage.image.transitionLayout(
      commandBuffer,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

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
    push.params0 = extent.width;
    push.params1 = extent.height;
    pass.getDrawContext().updatePushConstants(push, 0);

    // Draw simulation
    pass.draw(DrawableEnvMap{});
  }

  uint32_t inputMask = app.getInputManager().getCurrentInputMask();
  if (inputMask & INPUT_BIT_SPACE) {
    static uint32_t frameNum = 0;

    BufferAllocation* pStaging =
        &_hdrStagingBuffers[frame.frameRingBufferIndex];

    _hdrImage.image.transitionLayout(
        commandBuffer,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT);
    _hdrImage.image.copyMipToBuffer(commandBuffer, pStaging->getBuffer(), 0, 0);

    app.addDeletiontask(DeletionTask{
        [pStaging, extent, n = frameNum]() {
          size_t bufSize = 4 * sizeof(float) * extent.width * extent.height;
          std::byte* pBuf = (std::byte*)malloc(bufSize);

          void* pSrc = pStaging->mapMemory();
          memcpy(pBuf, pSrc, bufSize);
          pStaging->unmapMemory();

          std::thread([pBuf, bufSize, extent, n]() {
            std::string path = GProjectDirectory + "/HdrCaptures/0/" +
                               std::to_string(n) + ".exr";
            Utilities::saveExr(
                path,
                extent.width,
                extent.height,
                gsl::span(pBuf, bufSize));
            delete pBuf;
          }).detach();
        },
        frame.frameRingBufferIndex});

    frameNum++;
  }
}
} // namespace StableFluids