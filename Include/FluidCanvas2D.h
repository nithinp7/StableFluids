#pragma once

#include "Simulation.h"

#include <Althea/Allocator.h>
#include <Althea/CameraController.h>
#include <Althea/ComputePipeline.h>
#include <Althea/DeferredRendering.h>
#include <Althea/DescriptorSet.h>
#include <Althea/FrameBuffer.h>
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

#include <vector>

using namespace AltheaEngine;

namespace AltheaEngine {
class Application;
} // namespace AltheaEngine

namespace StableFluids {

struct GlobalUniforms {
  float time;
};

struct SimResources {
  // Velocity advection pass
  ImageResource velocityField{};
  ImageResource advectedVelocityField{};
  std::unique_ptr<DescriptorSetAllocator> _pAdvectPassMaterialAllocator;
  std::unique_ptr<ComputePipeline> _pAdvectPass;

  // Divergence calculation pass
  ImageResource divergenceField{};
  std::unique_ptr<ComputePipeline> _pDivergencePass;

  // Pressure calculation pass
  // Ping-pong buffers for pressure computation
  ImageResource pressureFieldA{};
  ImageResource pressureFieldB{};
  std::unique_ptr<ComputePipeline> _pPressurePass;

  // Velocity update pass
  std::unique_ptr<ComputePipeline> _pUpdateVelocityPass;
};

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
  void
  _createSimulation(Application& app, SingleTimeCommandBuffer& commandBuffer);
  std::unique_ptr<Simulation> _pSimulation;

  void _createGlobalResources(
      Application& app,
      SingleTimeCommandBuffer& commandBuffer);
  std::unique_ptr<PerFrameResources> _pGlobalResources;
  std::unique_ptr<TransientUniforms<GlobalUniforms>> _pGlobalUniforms;

  void _createRenderPass(Application& app);
  std::unique_ptr<RenderPass> _pRenderPass;
  SwapChainFrameBufferCollection _swapChainFrameBuffers;
};
} // namespace StableFluids
