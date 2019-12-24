#include <Graphics/Vulkan/PipelineStash.h>

#include <Common/Vulkan.h>
#include <Common/Assert.h>

#include <Graphics/GraphicsTypes.h>

#include <Graphics/Vulkan/Device.h>
#include <Graphics/Vulkan/Shader.h>
#include <Graphics/Vulkan/ShaderStash.h>
#include <Graphics/Vulkan/RenderPassStash.h>
#include <Graphics/Vulkan/TypeConversions.h>

#include <array>

static constexpr char DEFAULT_VERT[] = "default_vert.spv";
static constexpr char DEFAULT_FRAG[] = "default_frag.spv";

namespace vk
{
PipelineStash::PipelineStash( const Device& device ) : _device( device )
{
   _shaderStash = std::make_unique<ShaderStash>( _device );
}

static VkShaderStageFlagBits shaderTypeToVKShaderStage( Shader::Type shaderType )
{
   switch( shaderType )
   {
      case Shader::Type::VERTEX:
         return VK_SHADER_STAGE_VERTEX_BIT;
      case Shader::Type::FRAGMENT:
         return VK_SHADER_STAGE_FRAGMENT_BIT;
      case Shader::Type::COMPUTE:
         return VK_SHADER_STAGE_COMPUTE_BIT;
      default:
         return VK_SHADER_STAGE_ALL;
   }
}

const VkDescriptorSetLayout PipelineStash::findOrCreate( const cyd::DescriptorSetLayoutInfo& info )
{
   // Creating the descriptor set layout
   const auto layoutIt = _descSetLayouts.find( info );
   if( layoutIt != _descSetLayouts.end() )
   {
      return layoutIt->second;
   }

   std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings;
   descSetLayoutBindings.reserve( info.shaderObjects.size() );
   for( const auto& object : info.shaderObjects )
   {
      // TODO Add UBO arrays
      VkDescriptorSetLayoutBinding descSetLayoutBinding = {};
      descSetLayoutBinding.binding                      = object.binding;

      switch( object.type )
      {
         case cyd::ShaderObjectType::UNIFORM:
            descSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
         case cyd::ShaderObjectType::COMBINED_IMAGE_SAMPLER:
            descSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            break;
         default:
            CYDASSERT( !"PipelineStash: Descriptor type not yet implemented" );
      }

      descSetLayoutBinding.descriptorCount = 1;  // For arrays
      descSetLayoutBinding.stageFlags =
          TypeConversions::cydShaderStagesToVkShaderStages( object.stages );
      descSetLayoutBinding.pImmutableSamplers = nullptr;

      descSetLayoutBindings.push_back( std::move( descSetLayoutBinding ) );
   }

   VkDescriptorSetLayoutCreateInfo layoutInfo = {};
   layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = static_cast<uint32_t>( descSetLayoutBindings.size() );
   layoutInfo.pBindings    = descSetLayoutBindings.data();

   VkDescriptorSetLayout descSetLayout;
   VkResult result =
       vkCreateDescriptorSetLayout( _device.getVKDevice(), &layoutInfo, nullptr, &descSetLayout );
   CYDASSERT( result == VK_SUCCESS && "PipelineStash: Could not create descriptor set layout" );

   return _descSetLayouts.insert( {info, descSetLayout} ).first->second;
}

const VkPipelineLayout PipelineStash::findOrCreate( const cyd::PipelineLayoutInfo& info )
{
   const auto layoutIt = _pipLayouts.find( info );
   if( layoutIt != _pipLayouts.end() )
   {
      return layoutIt->second;
   }

   std::vector<VkPushConstantRange> vkRanges;
   vkRanges.reserve( info.ranges.size() );
   for( const auto& range : info.ranges )
   {
      VkPushConstantRange vkRange = {};
      vkRange.stageFlags = TypeConversions::cydShaderStagesToVkShaderStages( range.stages );
      vkRange.offset     = range.offset;
      vkRange.size       = range.size;

      vkRanges.push_back( std::move( vkRange ) );
   }

   // TODO This vector contains multiple copies of the same layout. Why is this necessary? The two
   // layouts will have information about the same multiple bindings. This seems redundant.
   std::vector<VkDescriptorSetLayout> descSetLayouts(
       info.descSetLayout.shaderObjects.size(), findOrCreate( info.descSetLayout ) );

   VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};

   pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>( descSetLayouts.size() );
   pipelineLayoutInfo.pSetLayouts            = descSetLayouts.data();
   pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>( vkRanges.size() );
   pipelineLayoutInfo.pPushConstantRanges    = vkRanges.data();

   VkPipelineLayout pipLayout;
   VkResult result =
       vkCreatePipelineLayout( _device.getVKDevice(), &pipelineLayoutInfo, nullptr, &pipLayout );
   CYDASSERT( result == VK_SUCCESS && "PipelineStash: Could not create pipeline layout" );

   return _pipLayouts.insert( {info, pipLayout} ).first->second;
}

const VkPipeline PipelineStash::findOrCreate( const cyd::PipelineInfo& info )
{
   // Attempting to find pipeline
   const auto pipIt = _pipelines.find( info );
   if( pipIt != _pipelines.end() )
   {
      return pipIt->second;
   }

   VkResult result;

   // Fetching shaders
   std::vector<VkPipelineShaderStageCreateInfo> shaderCreateInfos;
   shaderCreateInfos.reserve( info.shaders.size() );
   for( const std::string& shaderName : info.shaders )
   {
      const Shader* shader = _shaderStash->getShader( shaderName );

      VkPipelineShaderStageCreateInfo stageInfo = {};
      stageInfo.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stageInfo.stage               = shaderTypeToVKShaderStage( shader->getType() );
      stageInfo.module              = shader->getModule();
      stageInfo.pName               = "main";
      stageInfo.pSpecializationInfo = nullptr;  // TODO SPEC CONSTS

      shaderCreateInfos.push_back( std::move( stageInfo ) );
   }

   // Fetching render pass
   const VkRenderPass renderPass = _device.getRenderPassStash().findOrCreate( info.renderPass );

   // Vertex input description
   // TODO Instancing
   VkVertexInputBindingDescription vertexBindingDesc = {};
   vertexBindingDesc.binding                         = 0;
   vertexBindingDesc.stride                          = sizeof( cyd::Vertex );
   vertexBindingDesc.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

   // Vertex attributes
   std::array<VkVertexInputAttributeDescription, 4> attributeDescs = {};

   // Position
   attributeDescs[0].binding  = 0;
   attributeDescs[0].location = 0;
   attributeDescs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
   attributeDescs[0].offset   = offsetof( cyd::Vertex, pos );

   // Color
   attributeDescs[1].binding  = 0;
   attributeDescs[1].location = 1;
   attributeDescs[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
   attributeDescs[1].offset   = offsetof( cyd::Vertex, col );

   // Texture Coordinates
   attributeDescs[3].binding  = 0;
   attributeDescs[3].location = 2;
   attributeDescs[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
   attributeDescs[3].offset   = offsetof( cyd::Vertex, uv );

   // Normals
   attributeDescs[2].binding  = 0;
   attributeDescs[2].location = 3;
   attributeDescs[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
   attributeDescs[2].offset   = offsetof( cyd::Vertex, normal );

   VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertexInputInfo.vertexBindingDescriptionCount   = 1;
   vertexInputInfo.pVertexBindingDescriptions      = &vertexBindingDesc;
   vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>( attributeDescs.size() );
   vertexInputInfo.pVertexAttributeDescriptions    = attributeDescs.data();

   // Input assembly
   VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
   inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAssembly.topology = TypeConversions::cydDrawPrimToVkDrawPrim( info.drawPrim );
   inputAssembly.primitiveRestartEnable = VK_FALSE;

   // Viewport and scissor
   VkViewport viewport = {};
   viewport.x          = 0.0f;
   viewport.y          = 0.0f;
   viewport.width      = static_cast<float>( info.extent.width );
   viewport.height     = static_cast<float>( info.extent.height );
   viewport.minDepth   = 0.0f;
   viewport.maxDepth   = 1.0f;

   VkRect2D scissor = {};
   scissor.offset   = {0, 0};
   scissor.extent   = {info.extent.width, info.extent.height};

   VkPipelineViewportStateCreateInfo viewportState = {};
   viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewportState.viewportCount = 1;
   viewportState.pViewports    = &viewport;
   viewportState.scissorCount  = 1;
   viewportState.pScissors     = &scissor;

   // Rasterizer
   VkPipelineRasterizationStateCreateInfo rasterizer = {};
   rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterizer.depthClampEnable        = VK_FALSE;
   rasterizer.rasterizerDiscardEnable = VK_FALSE;
   rasterizer.polygonMode             = TypeConversions::cydPolyModeToVkPolyMode( info.polyMode );
   rasterizer.lineWidth               = 1.0f;
   rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
   rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
   rasterizer.depthBiasEnable         = VK_FALSE;

   // Multisampling
   VkPipelineMultisampleStateCreateInfo multisampling = {};
   multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampling.sampleShadingEnable  = VK_FALSE;
   multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   // Color blending
   VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
   colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorBlendAttachment.blendEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo colorBlending = {};
   colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   colorBlending.logicOpEnable     = VK_FALSE;
   colorBlending.logicOp           = VK_LOGIC_OP_COPY;
   colorBlending.attachmentCount   = 1;
   colorBlending.pAttachments      = &colorBlendAttachment;
   colorBlending.blendConstants[0] = 0.0f;
   colorBlending.blendConstants[1] = 0.0f;
   colorBlending.blendConstants[2] = 0.0f;
   colorBlending.blendConstants[3] = 0.0f;

   // Pipeline layout
   const VkPipelineLayout pipLayout = findOrCreate( info.pipLayout );

   // Dynamic state
   std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT};

   VkPipelineDynamicStateCreateInfo dynamicCreateInfo = {};
   dynamicCreateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamicCreateInfo.dynamicStateCount = static_cast<uint32_t>( dynamicStates.size() );
   dynamicCreateInfo.pDynamicStates    = dynamicStates.data();

   // Depth stencil state
   // TODO Maybe not create a depth state when we don't have any depth attachment? Probably has
   // little to no effect on performance though
   VkPipelineDepthStencilStateCreateInfo depthStencil = {};
   depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
   depthStencil.depthTestEnable       = VK_TRUE;
   depthStencil.depthWriteEnable      = VK_TRUE;
   depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
   depthStencil.depthBoundsTestEnable = VK_FALSE;
   depthStencil.stencilTestEnable     = VK_FALSE;
   depthStencil.minDepthBounds        = 0.0f;
   depthStencil.maxDepthBounds        = 1.0f;
   depthStencil.front                 = {};
   depthStencil.back                  = {};

   // Pipeline
   VkGraphicsPipelineCreateInfo pipelineInfo = {};
   pipelineInfo.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipelineInfo.stageCount                   = static_cast<uint32_t>( shaderCreateInfos.size() );
   pipelineInfo.pStages                      = shaderCreateInfos.data();
   pipelineInfo.pVertexInputState            = &vertexInputInfo;
   pipelineInfo.pInputAssemblyState          = &inputAssembly;
   pipelineInfo.pViewportState               = &viewportState;
   pipelineInfo.pRasterizationState          = &rasterizer;
   pipelineInfo.pMultisampleState            = &multisampling;
   pipelineInfo.pColorBlendState             = &colorBlending;
   pipelineInfo.pDynamicState                = &dynamicCreateInfo;
   pipelineInfo.pDepthStencilState           = &depthStencil;
   pipelineInfo.layout                       = pipLayout;
   pipelineInfo.renderPass                   = renderPass;
   pipelineInfo.subpass                      = 0;
   pipelineInfo.basePipelineHandle           = VK_NULL_HANDLE;

   VkPipeline pipeline;
   result = vkCreateGraphicsPipelines(
       _device.getVKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline );
   CYDASSERT( result == VK_SUCCESS && "PipelineStash: Could not create default pipeline" );

   return _pipelines.insert( {info, pipeline} ).first->second;
}

PipelineStash::~PipelineStash()
{
   for( const auto& pipeline : _pipelines )
   {
      vkDestroyPipeline( _device.getVKDevice(), pipeline.second, nullptr );
   }
   for( const auto& pipLayout : _pipLayouts )
   {
      vkDestroyPipelineLayout( _device.getVKDevice(), pipLayout.second, nullptr );
   }
   for( const auto& descSetLayout : _descSetLayouts )
   {
      vkDestroyDescriptorSetLayout( _device.getVKDevice(), descSetLayout.second, nullptr );
   }
}
}