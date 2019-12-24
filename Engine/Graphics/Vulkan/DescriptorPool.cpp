#include <Graphics/Vulkan/DescriptorPool.h>

#include <Common/Assert.h>
#include <Common/Vulkan.h>

#include <Graphics/Vulkan/Device.h>
#include <Graphics/Vulkan/PipelineStash.h>

#include <array>

namespace vk
{
DescriptorPool::DescriptorPool( const Device& device ) : _device( device )
{
   std::array<VkDescriptorPoolSize, 2> poolSizes = {};

   poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   poolSizes[0].descriptorCount = 32;

   poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   poolSizes[1].descriptorCount = 32;

   VkDescriptorPoolCreateInfo poolInfo = {};
   poolInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   poolInfo.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   poolInfo.poolSizeCount              = static_cast<uint32_t>( poolSizes.size() );
   poolInfo.pPoolSizes                 = poolSizes.data();
   poolInfo.maxSets                    = 32;

   VkResult result =
       vkCreateDescriptorPool( _device.getVKDevice(), &poolInfo, nullptr, &_vkDescPool );

   CYDASSERT( result == VK_SUCCESS && "CommandPool: Could not create descriptor pool" );
}

VkDescriptorSet DescriptorPool::findOrAllocate( const cyd::DescriptorSetLayoutInfo& layout )
{
   const auto descSetIt = _descSets.find( layout );
   if( descSetIt != _descSets.end() )
   {
      return descSetIt->second;
   }

   const VkDescriptorSetLayout vkDescSetLayout = _device.getPipelineStash().findOrCreate( layout );

   VkDescriptorSetAllocateInfo allocInfo = {};
   allocInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocInfo.descriptorPool              = _vkDescPool;
   allocInfo.descriptorSetCount          = 1;
   allocInfo.pSetLayouts                 = &vkDescSetLayout;

   VkDescriptorSet vkDescSet;
   VkResult result = vkAllocateDescriptorSets( _device.getVKDevice(), &allocInfo, &vkDescSet );
   CYDASSERT( result == VK_SUCCESS && "DescriptorPool: Failed to solo allocate descriptor set" );

   return _descSets.insert( {layout, vkDescSet} ).first->second;
}

void DescriptorPool::free( const VkDescriptorSet& descSet )
{
   vkFreeDescriptorSets( _device.getVKDevice(), _vkDescPool, 1, &descSet );
}

DescriptorPool::~DescriptorPool()
{
   vkDestroyDescriptorPool( _device.getVKDevice(), _vkDescPool, nullptr );
}
}