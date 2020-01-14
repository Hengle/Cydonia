#include <Graphics/Vulkan/Swapchain.h>

#include <Common/Vulkan.h>
#include <Common/Assert.h>

#include <Graphics/Vulkan/Device.h>
#include <Graphics/Vulkan/Surface.h>
#include <Graphics/Vulkan/CommandBuffer.h>
#include <Graphics/Vulkan/TypeConversions.h>

#include <algorithm>

// Double-buffered
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

namespace vk
{
Swapchain::Swapchain( Device& device, const Surface& surface, const cyd::SwapchainInfo& info )
    : m_device( device ), m_surface( surface )
{
   _createSwapchain( info );
   _createImageViews();
   _createDepthResources();
   _createSyncObjects();
}

static uint32_t chooseImageCount( const VkSurfaceCapabilitiesKHR& caps )
{
   uint32_t imageCount = caps.minImageCount;

   if( caps.maxImageCount > 0 && imageCount > caps.maxImageCount )
   {
      imageCount = caps.maxImageCount;
   }

   return imageCount;
}

static VkExtent2D chooseExtent( const cyd::Extent& extent, const VkSurfaceCapabilitiesKHR& caps )
{
   if( caps.currentExtent.width != UINT32_MAX )
   {
      return caps.currentExtent;
   }
   else
   {
      // Use the window extent
      VkExtent2D actualExtent = { extent.width, extent.height };

      actualExtent.width =
          std::clamp( actualExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width );
      actualExtent.height =
          std::clamp( actualExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height );

      return actualExtent;
   }
}

static VkSurfaceFormatKHR chooseFormat(
    cyd::PixelFormat format,
    cyd::ColorSpace space,
    const VkPhysicalDevice& physDevice,
    const VkSurfaceKHR& vkSurface )
{
   std::vector<VkSurfaceFormatKHR> formats;

   uint32_t formatCount;
   vkGetPhysicalDeviceSurfaceFormatsKHR( physDevice, vkSurface, &formatCount, nullptr );

   if( formatCount > 0 )
   {
      formats.resize( formatCount );
      vkGetPhysicalDeviceSurfaceFormatsKHR( physDevice, vkSurface, &formatCount, formats.data() );
   }

   VkSurfaceFormatKHR desiredFormat;
   desiredFormat.format     = TypeConversions::cydFormatToVkFormat( format );
   desiredFormat.colorSpace = TypeConversions::cydSpaceToVkSpace( space );

   auto it = std::find_if(
       formats.begin(), formats.end(), [&desiredFormat]( const VkSurfaceFormatKHR& format ) {
          return format.format == desiredFormat.format &&
                 format.colorSpace == desiredFormat.colorSpace;
       } );

   if( it != formats.end() )
   {
      return *it;
   }

   return formats[0];
}

static VkPresentModeKHR choosePresentMode(
    cyd::PresentMode mode,
    const VkPhysicalDevice& physDevice,
    const VkSurfaceKHR& vkSurface )
{
   std::vector<VkPresentModeKHR> presentModes;

   uint32_t presentModeCount;
   vkGetPhysicalDeviceSurfacePresentModesKHR( physDevice, vkSurface, &presentModeCount, nullptr );

   if( presentModeCount != 0 )
   {
      presentModes.resize( presentModeCount );
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          physDevice, vkSurface, &presentModeCount, presentModes.data() );
   }

   VkPresentModeKHR desiredMode;
   switch( mode )
   {
      case cyd::PresentMode::FIFO:
         desiredMode = VK_PRESENT_MODE_FIFO_KHR;
         break;
      case cyd::PresentMode::FIFO_RELAXED:
         desiredMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
         break;
      case cyd::PresentMode::IMMEDIATE:
         desiredMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
         break;
      case cyd::PresentMode::MAILBOX:
         desiredMode = VK_PRESENT_MODE_MAILBOX_KHR;
         break;
      default:
         CYDASSERT( !"Swapchain: Present mode not supported" );
   }

   auto it = std::find( presentModes.begin(), presentModes.end(), desiredMode );

   if( it != presentModes.end() )
   {
      return *it;
   }

   return presentModes[0];
}

void Swapchain::_createSwapchain( const cyd::SwapchainInfo& info )
{
   const VkDevice& vkDevice           = m_device.getVKDevice();
   const VkPhysicalDevice& physDevice = m_device.getPhysicalDevice();
   const VkSurfaceKHR& vkSurface      = m_surface.getVKSurface();

   VkSurfaceCapabilitiesKHR caps;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physDevice, vkSurface, &caps );

   m_surfaceFormat = std::make_unique<VkSurfaceFormatKHR>(
       chooseFormat( info.format, info.space, physDevice, vkSurface ) );
   m_extent      = std::make_unique<VkExtent2D>( chooseExtent( info.extent, caps ) );
   m_imageCount  = chooseImageCount( caps );
   m_presentMode = choosePresentMode( info.mode, physDevice, vkSurface );

   VkSwapchainCreateInfoKHR createInfo = {};
   createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
   createInfo.surface                  = vkSurface;

   createInfo.minImageCount    = m_imageCount;
   createInfo.imageFormat      = m_surfaceFormat->format;
   createInfo.imageColorSpace  = m_surfaceFormat->colorSpace;
   createInfo.imageExtent      = *m_extent;
   createInfo.imageArrayLayers = 1;
   createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

   // TODO Sharing mode concurrent
   createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

   createInfo.preTransform   = caps.currentTransform;
   createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   createInfo.presentMode    = m_presentMode;
   createInfo.clipped        = VK_TRUE;

   createInfo.oldSwapchain = VK_NULL_HANDLE;

   VkResult result = vkCreateSwapchainKHR( vkDevice, &createInfo, nullptr, &m_vkSwapchain );
   CYDASSERT( result == VK_SUCCESS && "Swapchain: Could not create swapchain" );

   vkGetSwapchainImagesKHR( vkDevice, m_vkSwapchain, &m_imageCount, nullptr );
   m_images.resize( m_imageCount );

   vkGetSwapchainImagesKHR( vkDevice, m_vkSwapchain, &m_imageCount, m_images.data() );
}

void Swapchain::_createImageViews()
{
   m_imageViews.resize( m_imageCount );
   for( size_t i = 0; i < m_imageCount; ++i )
   {
      VkImageViewCreateInfo createInfo           = {};
      createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image                           = m_images[i];
      createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format                          = m_surfaceFormat->format;
      createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel   = 0;
      createInfo.subresourceRange.levelCount     = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount     = 1;

      VkResult result =
          vkCreateImageView( m_device.getVKDevice(), &createInfo, nullptr, &m_imageViews[i] );
      CYDASSERT( result == VK_SUCCESS && "Swapchain: Could not create image views" );
   }
}

void Swapchain::_createDepthResources()
{
   VkResult result;

   VkImageCreateInfo imageInfo = {};
   imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   imageInfo.imageType         = VK_IMAGE_TYPE_2D;
   imageInfo.extent.width      = m_extent->width;
   imageInfo.extent.height     = m_extent->height;
   imageInfo.extent.depth      = 1;
   imageInfo.mipLevels         = 1;
   imageInfo.arrayLayers       = 1;
   imageInfo.format            = VK_FORMAT_D32_SFLOAT;
   imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
   imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
   imageInfo.usage             = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
   imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
   imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;

   result = vkCreateImage( m_device.getVKDevice(), &imageInfo, nullptr, &m_depthImage );
   CYDASSERT( result == VK_SUCCESS && "Swapchain: Could not create depth image" );

   VkMemoryRequirements memRequirements;
   vkGetImageMemoryRequirements( m_device.getVKDevice(), m_depthImage, &memRequirements );

   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize       = memRequirements.size;
   allocInfo.memoryTypeIndex      = m_device.findMemoryType(
       memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

   result = vkAllocateMemory( m_device.getVKDevice(), &allocInfo, nullptr, &m_depthImageMemory );
   CYDASSERT( result == VK_SUCCESS && "Swapchain: Could not allocate depth image memory" );

   vkBindImageMemory( m_device.getVKDevice(), m_depthImage, m_depthImageMemory, 0 );

   VkImageViewCreateInfo imageviewInfo           = {};
   imageviewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   imageviewInfo.image                           = m_depthImage;
   imageviewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   imageviewInfo.format                          = VK_FORMAT_D32_SFLOAT;
   imageviewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
   imageviewInfo.subresourceRange.baseMipLevel   = 0;
   imageviewInfo.subresourceRange.levelCount     = 1;
   imageviewInfo.subresourceRange.baseArrayLayer = 0;
   imageviewInfo.subresourceRange.layerCount     = 1;

   result = vkCreateImageView( m_device.getVKDevice(), &imageviewInfo, nullptr, &m_depthImageView );
   CYDASSERT( result == VK_SUCCESS && "Swapchain: Could not create depth image view" );
}

void Swapchain::_createSyncObjects()
{
   m_availableSems.resize( MAX_FRAMES_IN_FLIGHT );
   m_renderDoneSems.resize( MAX_FRAMES_IN_FLIGHT );

   VkSemaphoreCreateInfo semaphoreInfo = {};
   semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

   VkFenceCreateInfo fenceInfo = {};
   fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fenceInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

   for( size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
   {
      if( vkCreateSemaphore(
              m_device.getVKDevice(), &semaphoreInfo, nullptr, &m_availableSems[i] ) !=
              VK_SUCCESS ||
          vkCreateSemaphore(
              m_device.getVKDevice(), &semaphoreInfo, nullptr, &m_renderDoneSems[i] ) !=
              VK_SUCCESS )
      {
         CYDASSERT( !"Swapchain: Could not create sync objects" );
      }
   }
}

void Swapchain::initFramebuffers( const cyd::RenderPassInfo& info, const VkRenderPass renderPass )
{
   if( renderPass != m_prevRenderPass )
   {
      m_prevRenderPass = renderPass;

      m_frameBuffers.resize( m_imageCount );
      for( size_t i = 0; i < m_imageCount; i++ )
      {
         std::vector<VkImageView> attachments;
         attachments.push_back( m_imageViews[i] );

         bool hasDepth = std::find_if(
                             info.attachments.begin(),
                             info.attachments.end(),
                             []( const cyd::Attachment& attachment ) {
                                return attachment.type == cyd::AttachmentType::DEPTH_STENCIL ||
                                       attachment.type == cyd::AttachmentType::DEPTH;
                             } ) != info.attachments.end();

         if( hasDepth )
         {
            attachments.push_back( m_depthImageView );
         }

         VkFramebufferCreateInfo framebufferInfo = {};
         framebufferInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         framebufferInfo.renderPass              = renderPass;
         framebufferInfo.attachmentCount         = static_cast<uint32_t>( attachments.size() );
         framebufferInfo.pAttachments            = attachments.data();
         framebufferInfo.width                   = m_extent->width;
         framebufferInfo.height                  = m_extent->height;
         framebufferInfo.layers                  = 1;

         VkResult result = vkCreateFramebuffer(
             m_device.getVKDevice(), &framebufferInfo, nullptr, &m_frameBuffers[i] );
         CYDASSERT( result == VK_SUCCESS && "Swapchain: Could not create framebuffer" );
      }
   }
}

void Swapchain::acquireImage( const CommandBuffer* buffer )
{
   vkAcquireNextImageKHR(
       m_device.getVKDevice(),
       m_vkSwapchain,
       UINT64_MAX,
       m_availableSems[m_currentFrame],
       VK_NULL_HANDLE,
       &m_imageIndex );

   m_inFlightCmdBuffer = buffer;
}

void Swapchain::present()
{
   const VkQueue* presentQueue = m_device.getQueueFromUsage( cyd::QueueUsage::GRAPHICS, true );
   if( presentQueue )
   {
      VkSwapchainKHR swapChains[]    = { m_vkSwapchain };
      VkSemaphore signalSemaphores[] = { m_renderDoneSems[m_currentFrame] };

      VkPresentInfoKHR presentInfo   = {};
      presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
      presentInfo.waitSemaphoreCount = 1;
      presentInfo.pWaitSemaphores    = signalSemaphores;
      presentInfo.swapchainCount     = 1;
      presentInfo.pSwapchains        = swapChains;
      presentInfo.pImageIndices      = &m_imageIndex;

      vkQueuePresentKHR( *presentQueue, &presentInfo );
      m_currentFrame = ( m_currentFrame + 1 ) % MAX_FRAMES_IN_FLIGHT;
   }
   else
   {
      CYDASSERT( !"Swapchain: Could not get a present queue" );
   }
}

VkFramebuffer Swapchain::getCurrentFramebuffer() const { return m_frameBuffers[m_currentFrame]; }

Swapchain::~Swapchain()
{
   for( uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++ )
   {
      vkDestroySemaphore( m_device.getVKDevice(), m_renderDoneSems[i], nullptr );
      vkDestroySemaphore( m_device.getVKDevice(), m_availableSems[i], nullptr );
   }

   for( auto frameBuffer : m_frameBuffers )
   {
      vkDestroyFramebuffer( m_device.getVKDevice(), frameBuffer, nullptr );
   }

   for( auto imageView : m_imageViews )
   {
      vkDestroyImageView( m_device.getVKDevice(), imageView, nullptr );
   }

   vkDestroyImageView( m_device.getVKDevice(), m_depthImageView, nullptr );
   vkDestroyImage( m_device.getVKDevice(), m_depthImage, nullptr );
   vkFreeMemory( m_device.getVKDevice(), m_depthImageMemory, nullptr );

   vkDestroySwapchainKHR( m_device.getVKDevice(), m_vkSwapchain, nullptr );
}
}
