#ifndef FSLSIMPLEUI_RENDER_IMBATCH_PREPROCESS_LINEAR_PREPROCESSUTIL2_TWOQUEUES_HPP
#define FSLSIMPLEUI_RENDER_IMBATCH_PREPROCESS_LINEAR_PREPROCESSUTIL2_TWOQUEUES_HPP
/****************************************************************************************************************************************************
 * Copyright 2021-2024 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslBase/Span/Span.hpp>
#include <FslGraphics/Sprite/NineSliceSprite.hpp>
#include <FslGraphics/Sprite/OptimizedNineSliceSprite.hpp>
#include <FslSimpleUI/Render/Base/Command/EncodedCommand.hpp>
#include <vector>
#include "../../HandleCoding.hpp"
#include "../../Log/FmtRenderDrawSpriteType.hpp"
#include "../../MeshManager.hpp"
#include "../PreprocessConfig.hpp"
#include "../PreprocessResult.hpp"
#include "../ProcessedCommandRecord.hpp"
#include "MaterialCacheRecord.hpp"

namespace Fsl::UI::RenderIMBatch::PreprocessUtil2
{
  // Pre-process the draw commands
  // - Split multi material sprites so each queue gets one entry for it, thereby making it appear as a single material for the rest of the
  //   drawing system
  // - Remove dummy entries
  inline PreprocessResult PreprocessTwoQueues(std::vector<ProcessedCommandRecord>& rProcessedCommandRecords,
                                              Span<MaterialCacheRecord> opaqueMaterialCache, Span<MaterialCacheRecord> transparentMaterialCache,
                                              ReadOnlySpan<EncodedCommand> commandSpan, const MeshManager& meshManager, const PxSize2D clipSizePx)
  {
    assert(!commandSpan.empty());
    const MaterialLookup& materialLookup = meshManager.GetMaterialLookup();

    constexpr uint32_t InvalidMaterialCacheIndex = 0xFFFFFFFF;
    for (std::size_t i = 0; i < opaqueMaterialCache.size(); ++i)
    {
      opaqueMaterialCache[i] = MaterialCacheRecord(InvalidMaterialCacheIndex);
    }
    for (std::size_t i = 0; i < transparentMaterialCache.size(); ++i)
    {
      transparentMaterialCache[i] = MaterialCacheRecord(InvalidMaterialCacheIndex);
    }

    const std::size_t capacity = (commandSpan.size() * 2u);
    if (capacity > rProcessedCommandRecords.size())
    {
      rProcessedCommandRecords.resize(capacity + PreprocessConfig::ProcessedGrowBy);
    }

    // Pre-process the draw commands
    // - Split opaque / transparent materials into two queues
    //   - Opaque front to back
    //   - Transparent back to front
    // - Split multi material sprites so each queue gets one entry for it, thereby making it appear as a single material for the rest of the
    //   drawing system
    // - Remove dummy entries
    const auto count = UncheckedNumericCast<uint32_t>(commandSpan.size());
    assert(count > 0u);

    ProcessedCommandRecord* const pDst = rProcessedCommandRecords.data();
    uint32_t dstTransparentIndex = count;
    uint32_t dstOpaqueIndex = count - 1u;

    const auto clipSizeWidthPx = static_cast<float>(clipSizePx.RawWidth());
    const auto clipSizeHeightPx = static_cast<float>(clipSizePx.RawHeight());

    for (uint32_t i = 0; i < count; ++i)
    {
      const EncodedCommand& command = commandSpan[i];
      // The queue should never contain a Nop command
      assert(command.State.Type() != DrawCommandType::Nop);
      const int32_t hMesh = HandleCoding::GetOriginalHandle(command.Mesh);
      switch (HandleCoding::GetType(command.Mesh))
      {
      case RenderDrawSpriteType::Dummy:
        break;
      case RenderDrawSpriteType::BasicImageSprite:
        {
          const auto& meshRecord = meshManager.UncheckedGetImageSprite(hMesh);
          const PxAreaRectangleF dstRectanglePxf(command.DstPositionPxf.X, command.DstPositionPxf.Y, PxSize1DF(command.DstSizePx.Width()),
                                                 PxSize1DF(command.DstSizePx.Height()));
          if (dstRectanglePxf.RawLeft() < clipSizeWidthPx && dstRectanglePxf.RawRight() > 0.0f && dstRectanglePxf.RawTop() < clipSizeHeightPx &&
              dstRectanglePxf.RawBottom() > 0.0f)
          {
            if (!meshRecord.IsOpaque)
            {
              assert(dstTransparentIndex < capacity);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              ++dstTransparentIndex;
            }
            else
            {
              assert(dstOpaqueIndex < count);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstOpaqueIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (opaqueMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                opaqueMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              --dstOpaqueIndex;
            }
          }
          break;
        }
      case RenderDrawSpriteType::BasicNineSliceSprite:
        {
          const auto& meshRecord = meshManager.UncheckedGetNineSliceSprite(hMesh);
          const PxAreaRectangleF dstRectanglePxf(command.DstPositionPxf.X, command.DstPositionPxf.Y, PxSize1DF(command.DstSizePx.Width()),
                                                 PxSize1DF(command.DstSizePx.Height()));
          if (dstRectanglePxf.RawLeft() < clipSizeWidthPx && dstRectanglePxf.RawRight() > 0.0f && dstRectanglePxf.RawTop() < clipSizeHeightPx &&
              dstRectanglePxf.RawBottom() > 0.0f)
          {
            if (!meshRecord.IsOpaque)
            {
              assert(dstTransparentIndex < capacity);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              ++dstTransparentIndex;
            }
            else
            {
              assert(dstOpaqueIndex < count);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstOpaqueIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (opaqueMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                opaqueMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              --dstOpaqueIndex;
            }
          }
          break;
        }
      // case RenderDrawSpriteType::BasicOptimizedNineSliceSprite:
      //  currentMaterialHandle = meshManager.FastGet(hMesh).MaterialHandle;
      //  break;
      case RenderDrawSpriteType::ImageSprite:
        {
          const auto& meshRecord = meshManager.UncheckedGetImageSprite(hMesh);

          PxAreaRectangleF dstRectanglePxf;
          if (command.DstSizePx == meshRecord.Primitive.RenderInfo.ScaledSizePx)
          {
            dstRectanglePxf = PxAreaRectangleF(command.DstPositionPxf.X + meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf.Left(),
                                               command.DstPositionPxf.Y + meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf.Top(),
                                               meshRecord.Primitive.RenderInfo.ScaledTrimmedSizePxf.Width(),
                                               meshRecord.Primitive.RenderInfo.ScaledTrimmedSizePxf.Height());
          }
          else
          {
            const PxSize1DF dstWidthPxf(command.DstSizePx.Width());
            const PxSize1DF dstHeightPxf(command.DstSizePx.Height());
            // We need to apply the scaling and trim
            PxSize1DF finalScalingX = dstWidthPxf / PxSize1DF(meshRecord.Primitive.RenderInfo.ScaledSizePx.Width());
            PxSize1DF finalScalingY = dstHeightPxf / PxSize1DF(meshRecord.Primitive.RenderInfo.ScaledSizePx.Height());

            dstRectanglePxf =
              PxAreaRectangleF(command.DstPositionPxf.X + (meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf.Left() * finalScalingX),
                               command.DstPositionPxf.Y + (meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf.Top() * finalScalingY),
                               dstWidthPxf - (meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf.SumX() * finalScalingX),
                               dstHeightPxf - (meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf.SumY() * finalScalingY));
          }

          if (dstRectanglePxf.RawLeft() < clipSizeWidthPx && dstRectanglePxf.RawRight() > 0.0f && dstRectanglePxf.RawTop() < clipSizeHeightPx &&
              dstRectanglePxf.RawBottom() > 0.0f)
          {
            if (!meshRecord.IsOpaque)
            {
              assert(dstTransparentIndex < capacity);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              ++dstTransparentIndex;
            }
            else
            {
              assert(dstOpaqueIndex < count);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstOpaqueIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (opaqueMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                opaqueMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              --dstOpaqueIndex;
            }
          }
          break;
        }
      case RenderDrawSpriteType::NineSliceSprite:
        {
          const auto& meshRecord = meshManager.UncheckedGetNineSliceSprite(hMesh);
          assert(meshRecord.Sprite);
          const PxThicknessF& scaledImageTrimMarginPxf = meshRecord.Primitive.RenderInfo.ScaledTrimMarginPxf;
          const auto dstRectanglePxf =
            command.State.Type() != DrawCommandType::DrawRot90CWAtOffsetAndSize
              ? PxAreaRectangleF(command.DstPositionPxf.X + scaledImageTrimMarginPxf.Left(),
                                 command.DstPositionPxf.Y + scaledImageTrimMarginPxf.Top(),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Width()) - scaledImageTrimMarginPxf.SumX()),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Height()) - scaledImageTrimMarginPxf.SumY()))
              : PxAreaRectangleF(command.DstPositionPxf.X + scaledImageTrimMarginPxf.Top(),
                                 command.DstPositionPxf.Y + scaledImageTrimMarginPxf.Left(),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Width()) - scaledImageTrimMarginPxf.SumY()),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Height()) - scaledImageTrimMarginPxf.SumX()));
          if (dstRectanglePxf.RawLeft() < clipSizeWidthPx && dstRectanglePxf.RawRight() > 0.0f && dstRectanglePxf.RawTop() < clipSizeHeightPx &&
              dstRectanglePxf.RawBottom() > 0.0f)
          {
            if (!meshRecord.IsOpaque)
            {
              assert(dstTransparentIndex < capacity);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              ++dstTransparentIndex;
            }
            else
            {
              assert(dstOpaqueIndex < count);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstOpaqueIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (opaqueMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                opaqueMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              --dstOpaqueIndex;
            }
          }
          break;
        }
      case RenderDrawSpriteType::SpriteFont:
        {
          const auto& meshRecord = meshManager.UncheckedGetSpriteFont(hMesh);
          const PxAreaRectangleF dstRectanglePxf(command.DstPositionPxf.X, command.DstPositionPxf.Y, PxSize1DF(command.DstSizePx.Width()),
                                                 PxSize1DF(command.DstSizePx.Height()));
          if (dstRectanglePxf.RawLeft() < clipSizeWidthPx && dstRectanglePxf.RawRight() > 0.0f && dstRectanglePxf.RawTop() < clipSizeHeightPx &&
              dstRectanglePxf.RawBottom() > 0.0f)
          {
            if (!meshRecord.IsOpaque)
            {
              assert(dstTransparentIndex < capacity);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              ++dstTransparentIndex;
            }
            else
            {
              assert(dstOpaqueIndex < count);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.MaterialHandle);
              pDst[dstOpaqueIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
              if (opaqueMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                opaqueMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              --dstOpaqueIndex;
            }
          }
          break;
        }
      case RenderDrawSpriteType::OptimizedNineSliceSprite:
        {
          const auto& meshRecord = meshManager.UncheckedGetOptimizedNineSliceSprite(hMesh);
          assert(meshRecord.Transparency != MeshTransparencyFlags::NoFlags);
          assert(meshRecord.Sprite);
          const PxThicknessF& scaledImageTrimMarginPxf = meshRecord.Sprite->GetRenderInfo().ScaledTrimMarginPxf;
          const auto dstRectanglePxf =
            command.State.Type() != DrawCommandType::DrawRot90CWAtOffsetAndSize
              ? PxAreaRectangleF(command.DstPositionPxf.X + scaledImageTrimMarginPxf.Left(),
                                 command.DstPositionPxf.Y + scaledImageTrimMarginPxf.Top(),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Width()) - scaledImageTrimMarginPxf.SumX()),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Height()) - scaledImageTrimMarginPxf.SumY()))
              : PxAreaRectangleF(command.DstPositionPxf.X + scaledImageTrimMarginPxf.Top(),
                                 command.DstPositionPxf.Y + scaledImageTrimMarginPxf.Left(),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Width()) - scaledImageTrimMarginPxf.SumY()),
                                 PxSize1DF::UncheckedCreate(PxSize1DF(command.DstSizePx.Height()) - scaledImageTrimMarginPxf.SumX()));
          if (dstRectanglePxf.RawLeft() < clipSizeWidthPx && dstRectanglePxf.RawRight() > 0.0f && dstRectanglePxf.RawTop() < clipSizeHeightPx &&
              dstRectanglePxf.RawBottom() > 0.0f)
          {
            if (command.DstColor.IsOpaque())
            {
              if (MeshTransparencyFlagsUtil::IsEnabled(meshRecord.Transparency, MeshTransparencyFlags::Opaque))
              {
                assert(dstOpaqueIndex < count);
                const auto materialId = materialLookup.GetMaterialIndex(meshRecord.OpaqueMaterialHandle);
                pDst[dstOpaqueIndex] =
                  ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i, ProcessedCommandFlags::RenderOpaque);
                if (opaqueMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
                {
                  opaqueMaterialCache[materialId.Value].Index = dstTransparentIndex;
                }
                --dstOpaqueIndex;
              }
              if (MeshTransparencyFlagsUtil::IsEnabled(meshRecord.Transparency, MeshTransparencyFlags::Transparent))
              {
                assert(dstTransparentIndex < capacity);
                const auto materialId = materialLookup.GetMaterialIndex(meshRecord.TransparentMaterialHandle);
                pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i);
                if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
                {
                  transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
                }
                ++dstTransparentIndex;
              }
            }
            else
            {
              assert(dstTransparentIndex < capacity);
              const auto materialId = materialLookup.GetMaterialIndex(meshRecord.TransparentMaterialHandle);
              pDst[dstTransparentIndex] = ProcessedCommandRecord(materialId, dstRectanglePxf, command.DstColor, dstTransparentIndex, i,
                                                                 ProcessedCommandFlags::RenderIgnoreOpacity);
              if (transparentMaterialCache[materialId.Value].Index == InvalidMaterialCacheIndex)
              {
                transparentMaterialCache[materialId.Value].Index = dstTransparentIndex;
              }
              ++dstTransparentIndex;
            }
          }
          break;
        }
      default:
        FSLLOG3_WARNING("Unsupported type: {}", HandleCoding::GetType(command.Mesh));
        break;
      }
    }
    assert(count > 0u);
    assert(dstTransparentIndex >= count);
    assert(dstOpaqueIndex < count || dstOpaqueIndex == 0xFFFFFFFF);
    ++dstOpaqueIndex;
    return {dstOpaqueIndex, count - dstOpaqueIndex, count, dstTransparentIndex - count};
  }
}

#endif
