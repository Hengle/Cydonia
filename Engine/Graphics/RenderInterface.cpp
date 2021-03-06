#include <Graphics/RenderInterface.h>

#include <Common/Assert.h>

#include <Graphics/StaticPipelines.h>
#include <Graphics/PipelineInfos.h>
#include <Graphics/Backends/VKRenderBackend.h>

#include <cstdio>

namespace CYD::GRIS
{
static RenderBackend* b = nullptr;

// =================================================================================================
// Initialization
//
static void CommonInit() { StaticPipelines::Initialize(); }

template <>
bool InitRenderBackend<VK>( const Window& window )
{
   printf( "======= Initializing Vulkan Rendering Backend =======\n" );
   delete b;
   b = new VKRenderBackend( window );

   CommonInit();

   return true;
}

template <>
bool InitRenderBackend<GL>( const Window& )
{
   printf( "======= OpenGL Rendering Backend Not Yet Implemented =======\n" );
   delete b;

   CommonInit();

   return false;
}

void UninitRenderBackend()
{
   printf( "======= Reports of my death have been greatly exaggerated =======\n" );

   StaticPipelines::Uninitialize();

   delete b;
}

void RenderBackendCleanup() { b->cleanup(); }

// =================================================================================================
// Command Buffers/Lists
//
CmdListHandle CreateCommandList( QueueUsageFlag usage, bool presentable )
{
   return b->createCommandList( usage, presentable );
}

void StartRecordingCommandList( CmdListHandle cmdList ) { b->startRecordingCommandList( cmdList ); }
void EndRecordingCommandList( CmdListHandle cmdList ) { b->endRecordingCommandList( cmdList ); }
void SubmitCommandList( CmdListHandle cmdList ) { b->submitCommandList( cmdList ); }
void ResetCommandList( CmdListHandle cmdList ) { b->resetCommandList( cmdList ); }
void WaitOnCommandList( CmdListHandle cmdList ) { b->waitOnCommandList( cmdList ); }
void DestroyCommandList( CmdListHandle cmdList ) { b->destroyCommandList( cmdList ); }

// =================================================================================================
// Pipeline Specification
//
void SetViewport( CmdListHandle cmdList, const Viewport& viewport )
{
   b->setViewport( cmdList, viewport );
}

void SetScissor( CmdListHandle cmdList, const Rectangle& scissor )
{
   b->setScissor( cmdList, scissor );
}

void BindPipeline( CmdListHandle cmdList, const GraphicsPipelineInfo& pipInfo )
{
   b->bindPipeline( cmdList, pipInfo );
}

void BindPipeline( CmdListHandle cmdList, const ComputePipelineInfo& pipInfo )
{
   b->bindPipeline( cmdList, pipInfo );
}

void BindPipeline( CmdListHandle cmdList, StaticPipelines::Type pipType )
{
   const PipelineInfo* pPipInfo = StaticPipelines::Get( pipType );

   if( pPipInfo )
   {
      switch( pPipInfo->type )
      {
         case PipelineType::GRAPHICS:
            b->bindPipeline( cmdList, *static_cast<const GraphicsPipelineInfo*>( pPipInfo ) );
            break;
         case PipelineType::COMPUTE:
            b->bindPipeline( cmdList, *static_cast<const ComputePipelineInfo*>( pPipInfo ) );
            break;
      }
   }
   else
   {
      CYDASSERT( !"RenderInterface: Could not find static pipeline" );
   }
}

void BindVertexBuffer( CmdListHandle cmdList, VertexBufferHandle bufferHandle )
{
   b->bindVertexBuffer( cmdList, bufferHandle );
}

template <>
void BindIndexBuffer<uint16_t>( CmdListHandle cmdList, IndexBufferHandle bufferHandle )
{
   b->bindIndexBuffer( cmdList, bufferHandle, IndexType::UNSIGNED_INT16 );
}

template <>
void BindIndexBuffer<uint32_t>( CmdListHandle cmdList, IndexBufferHandle bufferHandle )
{
   b->bindIndexBuffer( cmdList, bufferHandle, IndexType::UNSIGNED_INT32 );
}

void BindTexture( CmdListHandle cmdList, TextureHandle texHandle, uint32_t set, uint32_t binding )
{
   b->bindTexture( cmdList, texHandle, set, binding );
}

void BindImage( CmdListHandle cmdList, TextureHandle texHandle, uint32_t set, uint32_t binding )
{
   b->bindImage( cmdList, texHandle, set, binding );
}

void BindBuffer( CmdListHandle cmdList, BufferHandle bufferHandle, uint32_t set, uint32_t binding )
{
   b->bindBuffer( cmdList, bufferHandle, set, binding );
}

void BindUniformBuffer(
    CmdListHandle cmdList,
    BufferHandle bufferHandle,
    uint32_t set,
    uint32_t binding )
{
   b->bindUniformBuffer( cmdList, bufferHandle, set, binding );
}

void UpdateConstantBuffer(
    CmdListHandle cmdList,
    ShaderStageFlag stages,
    size_t offset,
    size_t size,
    const void* pData )
{
   b->updateConstantBuffer( cmdList, stages, offset, size, pData );
}

// =================================================================================================
// Resources
//
TextureHandle CreateTexture( CmdListHandle transferList, const TextureDescription& desc )
{
   return b->createTexture( transferList, desc );
}

TextureHandle
CreateTexture( CmdListHandle transferList, const TextureDescription& desc, const std::string& path )
{
   return b->createTexture( transferList, desc, path );
}

TextureHandle CreateTexture(
    CmdListHandle transferList,
    const TextureDescription& desc,
    const std::vector<std::string>& paths )
{
   return b->createTexture( transferList, desc, paths );
}

TextureHandle
CreateTexture( CmdListHandle transferList, const TextureDescription& desc, const void* pTexels )
{
   return b->createTexture( transferList, desc, pTexels );
}

VertexBufferHandle CreateVertexBuffer(
    CmdListHandle transferList,
    uint32_t count,
    uint32_t stride,
    const void* pVertices )
{
   return b->createVertexBuffer( transferList, count, stride, pVertices );
}

IndexBufferHandle
CreateIndexBuffer( CmdListHandle transferList, uint32_t count, const void* pIndices )
{
   return b->createIndexBuffer( transferList, count, pIndices );
}

BufferHandle CreateUniformBuffer( size_t size ) { return b->createUniformBuffer( size ); }

BufferHandle CreateBuffer( size_t size ) { return b->createBuffer( size ); }

void CopyToBuffer( BufferHandle bufferHandle, const void* pData, size_t offset, size_t size )
{
   return b->copyToBuffer( bufferHandle, pData, offset, size );
}

void DestroyTexture( TextureHandle texHandle ) { b->destroyTexture( texHandle ); }

void DestroyVertexBuffer( VertexBufferHandle bufferHandle )
{
   b->destroyVertexBuffer( bufferHandle );
}

void DestroyIndexBuffer( IndexBufferHandle bufferHandle ) { b->destroyIndexBuffer( bufferHandle ); }

void DestroyBuffer( BufferHandle bufferHandle ) { b->destroyBuffer( bufferHandle ); }

// =================================================================================================
// Drawing
//
void PrepareFrame() { b->prepareFrame(); }

void BeginRenderPassSwapchain( CmdListHandle cmdList, bool wantDepth )
{
   b->beginRenderSwapchain( cmdList, wantDepth );
}

void BeginRenderPassTargets(
    CmdListHandle cmdList,
    const RenderPassInfo& renderPassInfo,
    const std::vector<TextureHandle>& textures )
{
   b->beginRenderTargets( cmdList, renderPassInfo, textures );
}

void EndRenderPass( CmdListHandle cmdList ) { b->endRenderPass( cmdList ); }

void DrawVertices( CmdListHandle cmdList, uint32_t vertexCount )
{
   b->drawVertices( cmdList, vertexCount );
}

void DrawVerticesIndexed( CmdListHandle cmdList, uint32_t indexCount )
{
   b->drawVerticesIndexed( cmdList, indexCount );
}

void Dispatch( CmdListHandle cmdList, uint32_t workX, uint32_t workY, uint32_t workZ )
{
   b->dispatch( cmdList, workX, workY, workZ );
}

void PresentFrame() { b->presentFrame(); }
}
