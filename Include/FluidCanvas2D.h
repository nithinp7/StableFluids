#pragma once

#include "Simulation.h"

#include <Althea/Allocator.h>
#include <Althea/CameraController.h>
#include <Althea/ComputePipeline.h>
#include <Althea/DeferredRendering.h>
#include <Althea/DescriptorSet.h>
#include <Althea/FrameBuffer.h>
#include <Althea/GlobalHeap.h>
#include <Althea/IGameInstance.h>
#include <Althea/Image.h>
#include <Althea/ImageBasedLighting.h>
#include <Althea/ImageResource.h>
#include <Althea/ImageView.h>
#include <Althea/Model.h>
#include <Althea/PerFrameResources.h>
#include <Althea/RenderPass.h>
#include <Althea/Sampler.h>
#include <Althea/ScreenSpaceReflection.h>
#include <Althea/Texture.h>
#include <Althea/TransientUniforms.h>
#include <glm/glm.hpp>

#include <array>
#include <vector>

using namespace AltheaEngine;

namespace AltheaEngine {
class Application;
} // namespace AltheaEngine

namespace StableFluids {
class FluidCanvas2D : public IGameInstance {
public:
  FluidCanvas2D();
  // virtual ~FluidCanvas2D();

  void initGame(Application& app) override;
  void shutdownGame(Application& app) override;

  void createRenderState(Application& app) override;
  void destroyRenderState(Application& app) override;

  void tick(Application& app, const FrameContext& frame) override;
  void draw(
      Application& app,
      VkCommandBuffer commandBuffer,
      const FrameContext& frame) override;

private:
  GlobalHeap _heap;

  Simulation _simulation;
  ImageResource _hdrImage;
  std::array<BufferAllocation, MAX_FRAMES_IN_FLIGHT> _hdrStagingBuffers;
  
  RenderPass _renderPass;
  SwapChainFrameBufferCollection _swapChainFrameBuffers;
};
} // namespace StableFluids
