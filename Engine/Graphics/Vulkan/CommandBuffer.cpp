#include <Graphics/Vulkan/CommandBuffer.h>

#include <Common/Assert.h>
#include <Common/Vulkan.h>

#include <Graphics/Vulkan/CommandPool.h>
#include <Graphics/Vulkan/Device.h>
#include <Graphics/Vulkan/PipelineStash.h>
#include <Graphics/Vulkan/RenderPassStash.h>
#include <Graphics/Vulkan/Swapchain.h>
#include <Graphics/Vulkan/Buffer.h>
#include <Graphics/Vulkan/Texture.h>
#include <Graphics/Vulkan/TypeConversions.h>

#include <array>

namespace vk
{
void CommandBuffer::seize(
    const Device& device,
    const CommandPool& pool,
    cyd::QueueUsageFlag usage )
{
   _device = &device;
   _pool   = &pool;
   _usage  = usage;

   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool                 = _pool->getVKCommandPool();
   allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandBufferCount          = 1;

   VkResult result = vkAllocateCommandBuffers( _device->getVKDevice(), &allocInfo, &_vkCmdBuffer );
   CYDASSERT( result == VK_SUCCESS && "CommandBuffer: Could not allocate command buffer" );

   VkFenceCreateInfo fenceInfo = {};
   fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

   result = vkCreateFence( _device->getVKDevice(), &fenceInfo, nullptr, &_vkFence );
   CYDASSERT( result == VK_SUCCESS && "CommandBuffer: Could not create fence" );
}

void CommandBuffer::release()
{
   if( _device )
   {
      _usage        = 0;
      _isRecording  = false;
      _wasSubmitted = false;

      _semsToWait.clear();
      _semsToSignal.clear();

      _boundPip.reset();
      _boundPipInfo.reset();
      _boundPipLayout.reset();
      _boundRenderPass.reset();

      vkDestroyFence( _device->getVKDevice(), _vkFence, nullptr );
      vkFreeCommandBuffers( _device->getVKDevice(), _pool->getVKCommandPool(), 1, &_vkCmdBuffer );

      _vkCmdBuffer = nullptr;
      _vkFence     = nullptr;
      _device      = nullptr;
      _pool        = nullptr;
   }
}

bool CommandBuffer::isCompleted() const
{
   return vkGetFenceStatus( _device->getVKDevice(), _vkFence ) == VK_SUCCESS;
}

void CommandBuffer::waitForCompletion() const
{
   vkWaitForFences( _device->getVKDevice(), 1, &_vkFence, VK_TRUE, UINTMAX_MAX );
}

void CommandBuffer::startRecording()
{
   if( _isRecording )
   {
      CYDASSERT( !"CommandBuffer: Already started recording" );
      return;
   }

   VkCommandBufferBeginInfo beginInfo = {};
   beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

   VkResult result = vkBeginCommandBuffer( _vkCmdBuffer, &beginInfo );
   CYDASSERT(
       result == VK_SUCCESS && "CommandBuffer: Failed to begin recording of command buffer" );
   _isRecording = true;
}

void CommandBuffer::endRecording()
{
   if( !_isRecording )
   {
      CYDASSERT( !"CommandBuffer: Trying to stop recording but was not in recording state" );
      return;
   }

   VkResult result = vkEndCommandBuffer( _vkCmdBuffer );
   CYDASSERT( result == VK_SUCCESS && "CommandBuffer: Failed to end recording of command buffer" );

   _boundPip        = std::nullopt;
   _boundPipLayout  = std::nullopt;
   _boundRenderPass = std::nullopt;
   _boundPipInfo    = std::nullopt;
   _isRecording     = false;
}

void CommandBuffer::updatePushConstants( const cyd::PushConstantRange& range, void* data )
{
   if( !_boundPipLayout.has_value() )
   {
      CYDASSERT( !"CommandBuffer: No currently bound pipeline layout" );
      return;
   }

   vkCmdPushConstants(
       _vkCmdBuffer,
       _boundPipLayout.value(),
       TypeConversions::cydShaderStagesToVkShaderStages( range.stages ),
       range.offset,
       range.size,
       data );
}

void CommandBuffer::bindPipeline( const cyd::PipelineInfo& info )
{
   VkPipeline pipeline        = _device->getPipelineStash().findOrCreate( info );
   VkPipelineLayout pipLayout = _device->getPipelineStash().findOrCreate( info.pipLayout );
   VkRenderPass renderPass    = _device->getRenderPassStash().findOrCreate( info.renderPass );

   if( !pipeline || !pipLayout || !renderPass )
   {
      CYDASSERT( !"CommandBuffer: Could not find or create pipeline in pipeline stash" );
      return;
   }

   vkCmdBindPipeline( _vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
   _boundPip        = pipeline;
   _boundPipLayout  = pipLayout;
   _boundRenderPass = renderPass;
   _boundPipInfo    = info;
}

void CommandBuffer::bindVertexBuffer( const Buffer* vertexBuffer )
{
   VkBuffer vertexBuffers[] = { vertexBuffer->getVKBuffer() };
   VkDeviceSize offsets[]   = { 0 };
   vkCmdBindVertexBuffers( _vkCmdBuffer, 0, 1, vertexBuffers, offsets );
}

template <>
void CommandBuffer::bindIndexBuffer<uint16_t>( const Buffer* indexBuffer )
{
   vkCmdBindIndexBuffer( _vkCmdBuffer, indexBuffer->getVKBuffer(), 0, VK_INDEX_TYPE_UINT16 );
}

template <>
void CommandBuffer::bindIndexBuffer<uint32_t>( const Buffer* indexBuffer )
{
   vkCmdBindIndexBuffer( _vkCmdBuffer, indexBuffer->getVKBuffer(), 0, VK_INDEX_TYPE_UINT32 );
}

void CommandBuffer::bindBuffer( const Buffer* buffer )
{
   if( !_boundPipLayout.has_value() )
   {
      CYDASSERT( !"CommandBuffer: Not currently bound pipeline" );
      return;
   }

   vkCmdBindDescriptorSets(
       _vkCmdBuffer,
       VK_PIPELINE_BIND_POINT_GRAPHICS,
       _boundPipLayout.value(),
       0,
       1,
       &buffer->getVKDescSet(),
       0,
       nullptr );
}

void CommandBuffer::bindTexture( Texture* texture )
{
   if( !_boundPipLayout.has_value() )
   {
      CYDASSERT( !"CommandBuffer: Not currently bound pipeline" );
      return;
   }

   if( texture->getLayout() != cyd::ImageLayout::SHADER_READ )
   {
      VkImageMemoryBarrier barrier = {};
      barrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = TypeConversions::cydImageLayoutToVKImageLayout( texture->getLayout() );
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                           = texture->getVKImage();
      barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barrier.subresourceRange.baseMipLevel   = 0;
      barrier.subresourceRange.levelCount     = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount     = 1;
      barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(
          _vkCmdBuffer,
          VK_PIPELINE_STAGE_TRANSFER_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
          0,
          0,
          nullptr,
          0,
          nullptr,
          1,
          &barrier );

      texture->setLayout( cyd::ImageLayout::SHADER_READ );
   }

   vkCmdBindDescriptorSets(
       _vkCmdBuffer,
       VK_PIPELINE_BIND_POINT_GRAPHICS,
       _boundPipLayout.value(),
       0,
       1,
       &texture->getVKDescSet(),
       0,
       nullptr );
}

void CommandBuffer::setViewport( const cyd::Rectangle& viewport )
{
   VkViewport vkViewport = {
       viewport.offset.x,
       viewport.offset.y,
       static_cast<float>( viewport.extent.width ),
       static_cast<float>( viewport.extent.height ),
       0.0f,
       1.0f };
   vkCmdSetViewport( _vkCmdBuffer, 0, 1, &vkViewport );
}

void CommandBuffer::beginPass( Swapchain& swapchain )
{
   if( !_boundPip.has_value() || !_boundRenderPass.has_value() || !_boundPipInfo.has_value() )
   {
      CYDASSERT( !"CommandBuffer: Could not start render pass because no pipeline was bound" );
      return;
   }

   swapchain.initFramebuffers( _boundPipInfo->renderPass, _boundRenderPass.value() );
   swapchain.acquireImage( this );

   _semsToWait.push_back( swapchain.getSemToWait() );
   _semsToSignal.push_back( swapchain.getSemToSignal() );

   VkRenderPassBeginInfo renderPassInfo = {};
   renderPassInfo.sType                 = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   renderPassInfo.renderPass            = _boundRenderPass.value();
   renderPassInfo.framebuffer           = swapchain.getCurrentFramebuffer();
   renderPassInfo.renderArea.offset     = { 0, 0 };
   renderPassInfo.renderArea.extent     = swapchain.getVKExtent();

   std::array<VkClearValue, 2> clearValues = {};
   clearValues[0]                          = { 0.0f, 0.0f, 0.0f, 1.0f };
   clearValues[1]                          = { 1.0f, 0 };

   renderPassInfo.clearValueCount = static_cast<uint32_t>( clearValues.size() );
   renderPassInfo.pClearValues    = clearValues.data();

   vkCmdBeginRenderPass( _vkCmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );
}

void CommandBuffer::draw( size_t vertexCount )
{
   if( _usage & cyd::QueueUsage::GRAPHICS )
   {
      vkCmdDraw( _vkCmdBuffer, static_cast<uint32_t>( vertexCount ), 1, 0, 0 );
   }
   else
   {
      CYDASSERT( !"CommandBuffer: Command Buffer does not support graphics usage" );
   }
}

void CommandBuffer::drawIndexed( size_t indexCount )
{
   if( _usage & cyd::QueueUsage::GRAPHICS )
   {
      vkCmdDrawIndexed( _vkCmdBuffer, static_cast<uint32_t>( indexCount ), 1, 0, 0, 0 );
   }
   else
   {
      CYDASSERT( !"CommandBuffer: Command Buffer does not support graphics usage" );
   }
}

void CommandBuffer::endPass() { vkCmdEndRenderPass( _vkCmdBuffer ); }

void CommandBuffer::copyBuffer( const Buffer* src, const Buffer* dst )
{
   CYDASSERT(
       src->getSize() == dst->getSize() &&
       "CommandBuffer: Source and destination sizes are not the same" );

   VkBufferCopy copyRegion = {};
   copyRegion.size         = dst->getSize();
   vkCmdCopyBuffer( _vkCmdBuffer, src->getVKBuffer(), dst->getVKBuffer(), 1, &copyRegion );
}

void CommandBuffer::uploadBufferToTex( const Buffer* src, Texture* dst )
{
   CYDASSERT(
       src->getSize() == dst->getSize() &&
       "CommandBuffer: Source and destination sizes are not the same" );

   // Transition image layout to transfer destination optimal
   VkImageMemoryBarrier barrier = {};
   barrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.oldLayout           = TypeConversions::cydImageLayoutToVKImageLayout( dst->getLayout() );
   barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.image               = dst->getVKImage();
   barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.baseMipLevel   = 0;
   barrier.subresourceRange.levelCount     = 1;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount     = 1;
   barrier.srcAccessMask                   = 0;
   barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

   vkCmdPipelineBarrier(
       _vkCmdBuffer,
       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
       VK_PIPELINE_STAGE_TRANSFER_BIT,
       0,
       0,
       nullptr,
       0,
       nullptr,
       1,
       &barrier );

   // Updating current image layout
   dst->setLayout( cyd::ImageLayout::TRANSFER_DST );

   // Copying data from buffer to texture
   VkBufferImageCopy region               = {};
   region.bufferOffset                    = 0;
   region.bufferRowLength                 = 0;
   region.bufferImageHeight               = 0;
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel       = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount     = 1;
   region.imageOffset                     = { 0, 0, 0 };
   region.imageExtent                     = { dst->getWidth(), dst->getHeight(), 1 };

   vkCmdCopyBufferToImage(
       _vkCmdBuffer,
       src->getVKBuffer(),
       dst->getVKImage(),
       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
       1,
       &region );
}

void CommandBuffer::submit()
{
   VkSubmitInfo submitInfo       = {};
   submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers    = &_vkCmdBuffer;

   VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT };
   submitInfo.waitSemaphoreCount     = static_cast<uint32_t>( _semsToWait.size() );
   submitInfo.pWaitSemaphores        = _semsToWait.data();
   submitInfo.pWaitDstStageMask      = waitStages;
   submitInfo.signalSemaphoreCount   = static_cast<uint32_t>( _semsToSignal.size() );
   submitInfo.pSignalSemaphores      = _semsToSignal.data();

   const VkQueue* queue = _device->getQueueFromFamily( _pool->getFamilyIndex() );
   if( !queue )
   {
      CYDASSERT( !"CommandBuffer: Could not find queue to submit to" );
      return;
   }

   if( _wasSubmitted )
   {
      // CPU Lock
      vkWaitForFences( _device->getVKDevice(), 1, &_vkFence, true, 0 );
      vkResetFences( _device->getVKDevice(), 1, &_vkFence );
   }

   vkQueueSubmit( *queue, 1, &submitInfo, _vkFence );
   _wasSubmitted = true;

   _semsToWait.clear();
   _semsToSignal.clear();
}
}