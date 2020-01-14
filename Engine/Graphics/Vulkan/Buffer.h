#pragma once

#include <Common/Include.h>

#include <Graphics/GraphicsTypes.h>

// ================================================================================================
// Forwards
// ================================================================================================
FWDHANDLE( VkBuffer );
FWDHANDLE( VkDeviceMemory );
FWDHANDLE( VkDescriptorSet );

namespace vk
{
class Device;
}

// ================================================================================================
// Definition
// ================================================================================================
namespace vk
{
class Buffer final
{
  public:
   Buffer() = default;
   MOVABLE( Buffer );
   ~Buffer() = default;

   void acquire(
       const Device& device,
       size_t size,
       cyd::BufferUsageFlag usage,
       cyd::MemoryTypeFlag memoryType );
   void release();

   size_t getSize() const noexcept { return m_size; }
   VkBuffer getVKBuffer() const noexcept { return m_vkBuffer; }
   const VkDescriptorSet& getVKDescSet() const noexcept { return m_vkDescSet; }
   bool inUse() const { return m_inUse; }

   void setUnused() { m_inUse = false; }

   void
   updateDescriptorSet( const cyd::ShaderObjectInfo& info, VkDescriptorSet descSet );
   void mapMemory( const void* pData );

  private:
   void _allocateMemory();

   const Device* m_pDevice = nullptr;

   // Used for staging buffers
   void* m_data = nullptr;

   // Common
   size_t m_size             = 0;
   VkBuffer m_vkBuffer       = nullptr;
   VkDeviceMemory m_vkMemory = nullptr;
   cyd::MemoryTypeFlag m_memoryType;

   // Optional for shader accessible buffers
   VkDescriptorSet m_vkDescSet = nullptr;
   
   bool m_inUse = false;
};
}
