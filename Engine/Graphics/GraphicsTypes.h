#pragma once

#include <Common/Include.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace cyd
{
// ================================================================================================
// Types & Enums

using Flag = uint32_t;

enum QueueUsage : Flag
{
   GRAPHICS = 1 << 0,
   COMPUTE  = 1 << 1,
   TRANSFER = 1 << 2
};
using QueueUsageFlag = Flag;

namespace BufferUsage
{
enum BufferUsage : Flag
{
   TRANSFER_SRC = 1 << 0,
   TRANSFER_DST = 1 << 1,
   UNIFORM      = 1 << 2,
   STORAGE      = 1 << 3,
   INDEX        = 1 << 4,
   VERTEX       = 1 << 5
};
}
using BufferUsageFlag = Flag;

namespace ImageUsage
{
enum ImageUsage : Flag
{
   TRANSFER_SRC  = 1 << 0,
   TRANSFER_DST  = 1 << 1,
   SAMPLED       = 1 << 2,
   STORAGE       = 1 << 3,
   COLOR         = 1 << 4,
   DEPTH_STENCIL = 1 << 5
};
}
using ImageUsageFlag = Flag;

enum MemoryType : Flag
{
   DEVICE_LOCAL  = 1 << 0,
   HOST_VISIBLE  = 1 << 1,
   HOST_COHERENT = 1 << 2
};
using MemoryTypeFlag = Flag;

enum ShaderStage : Flag
{
   VERTEX_STAGE        = 1 << 0,
   GEOMETRY_STAGE      = 1 << 1,
   FRAGMENT_STAGE      = 1 << 2,
   COMPUTE_STAGE       = 1 << 3,
   ALL_GRAPHICS_STAGES = 1 << 4,
   ALL_STAGES          = 1 << 5
};
using ShaderStageFlag = Flag;

enum class PixelFormat
{
   BGRA8_UNORM,
   D32_SFLOAT,
};

enum class ColorSpace
{
   SRGB_NONLINEAR
};

enum class PresentMode
{
   IMMEDIATE,
   FIFO,
   FIFO_RELAXED,
   MAILBOX
};

enum class LoadOp
{
   LOAD,
   CLEAR,
   DONT_CARE
};

enum class StoreOp
{
   STORE,
   DONT_CARE
};

enum class AttachmentType
{
   COLOR,
   DEPTH,
   DEPTH_STENCIL
};

enum class DrawPrimitive
{
   POINTS,
   LINES,
   LINE_STRIPS,
   TRIANGLES,
   TRIANGLE_STRIPS,
};

enum class PolygonMode
{
   FILL,
   LINE,
   POINT
};

enum class ImageLayout
{
   UNDEFINED,
   GENERAL,
   COLOR,
   PRESENTATION,
   TRANSFER_SRC,
   TRANSFER_DST,
   SHADER_READ,
   DEPTH_STENCIL
};

enum class ImageType
{
   TEXTURE_1D,
   TEXTURE_2D,
   TEXTURE_3D
};

enum class ShaderObjectType
{
   UNIFORM,
   COMBINED_IMAGE_SAMPLER
};

enum class Filter
{
   NEAREST,
   LINEAR,
   CUBIC
};

enum class AddressMode
{
   REPEAT,
   MIRRORED_REPEAT,
   CLAMP_TO_EDGE,
   CLAMP_TO_BORDER,
   MIRROR_CLAMP_TO_EDGE,
};

// ================================================================================================
// Basic structs

struct TextureDescription
{
   size_t size;
   uint32_t width;
   uint32_t height;
   ImageType type;
   PixelFormat format;
   ImageUsageFlag usage;
};

struct Extent
{
   bool operator==( const Extent& other ) const;
   uint32_t width;
   uint32_t height;
};

struct Rectangle
{
   glm::vec2 offset;
   Extent extent;
};

struct Vertex
{
   // Keep in sync with the VkVertexInputAttributeDescription in PipelineStash
   bool operator==( const Vertex& other ) const;
   glm::vec3 pos;
   glm::vec4 col;
   glm::vec3 uv;
   glm::vec3 normal;
};

struct ShaderObjectInfo
{
   bool operator==( const ShaderObjectInfo& other ) const;
   ShaderObjectType type;
   ShaderStageFlag stages;
   uint32_t binding;
};

struct Attachment
{
   bool operator==( const Attachment& other ) const;
   PixelFormat format;
   LoadOp loadOp;
   StoreOp storeOp;
   AttachmentType type;
   ImageLayout layout;
};

struct PushConstantRange
{
   bool operator==( const PushConstantRange& other ) const;
   ShaderStageFlag stages;
   uint32_t offset;
   uint32_t size;
};

// ================================================================================================
// Pipeline Description

struct SamplerInfo
{
   bool operator==( const SamplerInfo& other ) const;
   bool useAnisotropy           = true;
   float maxAnisotropy          = 16.0f;
   cyd::Filter magFilter        = cyd::Filter::NEAREST;
   cyd::Filter minFilter        = cyd::Filter::NEAREST;
   cyd::AddressMode addressMode = cyd::AddressMode::REPEAT;
};

struct RenderPassInfo
{
   bool operator==( const RenderPassInfo& other ) const;
   std::vector<cyd::Attachment> attachments;
};

struct DescriptorSetLayoutInfo
{
   bool operator==( const DescriptorSetLayoutInfo& other ) const;
   std::vector<cyd::ShaderObjectInfo> shaderObjects;
};

struct PipelineLayoutInfo
{
   bool operator==( const PipelineLayoutInfo& other ) const;
   std::vector<cyd::PushConstantRange> ranges;
   DescriptorSetLayoutInfo descSetLayout;
};

struct PipelineInfo
{
   bool operator==( const PipelineInfo& other ) const;
   std::vector<std::string> shaders;
   RenderPassInfo renderPass;
   PipelineLayoutInfo pipLayout;
   cyd::DrawPrimitive drawPrim;
   cyd::PolygonMode polyMode;
   cyd::Extent extent;
};

struct SwapchainInfo
{
   cyd::Extent extent;
   cyd::PixelFormat format;
   cyd::ColorSpace space;
   cyd::PresentMode mode;
};
}

// ================================================================================================
// Hashing Functions

template <>
struct std::hash<cyd::Vertex>
{
   size_t operator()( const cyd::Vertex& vertex ) const
   {
      size_t seed = 0;
      hash_combine( seed, vertex.pos.x );
      hash_combine( seed, vertex.pos.y );
      hash_combine( seed, vertex.pos.z );
      hash_combine( seed, vertex.col.r );
      hash_combine( seed, vertex.col.g );
      hash_combine( seed, vertex.col.b );
      hash_combine( seed, vertex.col.a );
      hash_combine( seed, vertex.uv.x );
      hash_combine( seed, vertex.uv.y );

      return seed;
   }
};

template <>
struct std::hash<cyd::Extent>
{
   size_t operator()( const cyd::Extent& extent ) const
   {
      size_t seed = 0;
      hash_combine( seed, extent.width );
      hash_combine( seed, extent.height );
      return seed;
   }
};

template <>
struct std::hash<cyd::Attachment>
{
   size_t operator()( const cyd::Attachment& attachment ) const
   {
      size_t seed = 0;
      hash_combine( seed, attachment.format );
      hash_combine( seed, attachment.loadOp );
      hash_combine( seed, attachment.storeOp );
      hash_combine( seed, attachment.type );
      hash_combine( seed, attachment.layout );

      return seed;
   }
};

template <>
struct std::hash<cyd::ShaderObjectInfo>
{
   size_t operator()( const cyd::ShaderObjectInfo& shaderObject ) const
   {
      size_t seed = 0;
      hash_combine( seed, shaderObject.type );
      hash_combine( seed, shaderObject.binding );
      hash_combine( seed, shaderObject.stages );
      return seed;
   }
};

template <>
struct std::hash<cyd::PushConstantRange>
{
   size_t operator()( const cyd::PushConstantRange& range ) const
   {
      size_t seed = 0;
      hash_combine( seed, range.stages );
      hash_combine( seed, range.offset );
      hash_combine( seed, range.size );
      return seed;
   }
};

template <>
struct std::hash<cyd::RenderPassInfo>
{
   size_t operator()( const cyd::RenderPassInfo& renderPass ) const
   {
      const std::vector<cyd::Attachment>& attachments = renderPass.attachments;

      size_t seed = 0;
      for( const auto& attachment : attachments )
      {
         hash_combine( seed, attachment );
      }
      return seed;
   }
};

template <>
struct std::hash<cyd::DescriptorSetLayoutInfo>
{
   size_t operator()( const cyd::DescriptorSetLayoutInfo& descSetLayoutInfo ) const
   {
      size_t seed = 0;
      for( const auto& ubo : descSetLayoutInfo.shaderObjects )
      {
         hash_combine( seed, ubo );
      }
      return seed;
   }
};

template <>
struct std::hash<cyd::PipelineLayoutInfo>
{
   size_t operator()( const cyd::PipelineLayoutInfo& pipLayoutInfo ) const
   {
      size_t seed = 0;
      for( const auto& range : pipLayoutInfo.ranges )
      {
         hash_combine( seed, range );
      }
      return seed;
   }
};

template <>
struct std::hash<cyd::PipelineInfo>
{
   size_t operator()( const cyd::PipelineInfo& pipInfo ) const
   {
      const std::vector<std::string>& shaders = pipInfo.shaders;

      size_t seed = 0;
      hash_combine( seed, pipInfo.pipLayout );
      hash_combine( seed, pipInfo.drawPrim );
      hash_combine( seed, pipInfo.polyMode );
      hash_combine( seed, pipInfo.extent );
      for( const auto& shader : shaders )
      {
         hash_combine( seed, shader );
      }

      return seed;
   }
};

template <>
struct std::hash<cyd::SamplerInfo>
{
   size_t operator()( const cyd::SamplerInfo& samplerInfo ) const
   {
      size_t seed = 0;
      hash_combine( seed, samplerInfo.useAnisotropy );
      hash_combine( seed, samplerInfo.maxAnisotropy );
      hash_combine( seed, samplerInfo.magFilter );
      hash_combine( seed, samplerInfo.minFilter );
      hash_combine( seed, samplerInfo.addressMode );

      return seed;
   }
};