/****************************************************************************************************************************************************
 * Copyright (c) 2014 Freescale Semiconductor, Inc.
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
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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

#include <FslBase/Exceptions.hpp>
#include <FslBase/IO/Path.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslGraphics/Font/BitmapFontConverter.hpp>
#include <FslGraphics/Sprite/Font/TextureAtlasSpriteFont.hpp>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace Fsl
{
  namespace
  {
    static_assert(sizeof(char) == 1, "we expect a char to be one byte if not the cast in Utf8StringViewReader will not work as expected");

    class Utf8StrViewReader
    {
      // NOLINTNEXTLINE(readability-identifier-naming)
      const uint8_t* m_pStr;
      // NOLINTNEXTLINE(readability-identifier-naming)
      const uint8_t* const m_pStrEnd;

    public:
      explicit Utf8StrViewReader(StringViewLite strView) noexcept
        : m_pStr(reinterpret_cast<const uint8_t*>(strView.data()))
        , m_pStrEnd(m_pStr != nullptr ? m_pStr + strView.size() : nullptr)
      {
      }

      bool IsEmpty() const noexcept
      {
        return m_pStr == m_pStrEnd;
      }

      uint32_t NextChar() noexcept
      {
        assert(m_pStr < m_pStrEnd);
        const uint32_t char0 = *m_pStr;
        ++m_pStr;
        uint32_t result = 0;
        // check if its a encoded character
        if (char0 < 0x80)
        {
          result = char0;
        }
        else
        {
          if (m_pStr < m_pStrEnd && (char0 & 0xE0) == 0xC0)
          {
            const uint32_t char1 = *m_pStr;
            ++m_pStr;
            result = ((char0 & 0x1F) << 6) | (char1 & 0x3f);
            FSLLOG3_DEBUG_WARNING_IF((char1 & 0xC0) != 0x80, "invalid UTF8 encoding encountered");
          }
          else if ((m_pStr + 1) < m_pStrEnd && (char0 & 0xF0) == 0xE0)
          {
            const uint32_t char1 = m_pStr[0];
            const uint32_t char2 = m_pStr[1];
            m_pStr += 2;
            result = ((char0 & 0x1F) << 12) | ((char1 & 0x3f) << 6) | (char2 & 0x3f);
            FSLLOG3_DEBUG_WARNING_IF((char1 & 0xC0) != 0x80 || (char2 & 0xC0) != 0x80, "invalid UTF8 encoding encountered");
          }
          else if ((m_pStr + 2) < m_pStrEnd && (char0 & 0xF8) == 0xF0 && char0 <= 0xF4)
          {
            const uint32_t char1 = m_pStr[0];
            const uint32_t char2 = m_pStr[1];
            const uint32_t char3 = m_pStr[2];
            m_pStr += 3;
            result = ((char0 & 0x1F) << 18) | ((char1 & 0x3f) << 12) | ((char2 & 0x3f) << 6) | (char3 & 0x3f);
            FSLLOG3_DEBUG_WARNING_IF((char1 & 0xC0) != 0x80 || (char2 & 0xC0) != 0x80 || (char3 & 0xC0) != 0x80, "invalid UTF8 encoding encountered");
          }
          else
          {
            result = 0;
            FSLLOG3_DEBUG_WARNING("unsupported UTF8 encoding");
          }
        }
        return result;
      }
    };


    inline PxValueU16 ScaledBaseLinePx(const PxValueU16 baseLinePx, const float fontScale)
    {
      return PxValueU16(
        UncheckedNumericCast<uint16_t>(std::max(static_cast<int32_t>(std::round(static_cast<float>(baseLinePx.Value) * fontScale)), 0)));
    }

    inline PxSize2D DoMeasureString(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar, const StringViewLite& strView)
    {
      auto renderRightPx = PxValue::Create(0);
      auto renderBottomPx = PxValue::Create(0);
      if (!strView.empty())
      {
        // extract information about the characters we are rendering
        auto layoutXOffsetPx = PxValue::Create(0);
        Utf8StrViewReader reader(strView);
        while (!reader.IsEmpty())
        {
          const CoreFontCharInfo& charInfo = lookup.GetChar(reader.NextChar(), unknownChar).CharInfo;
          const PxValue currentRightPx = layoutXOffsetPx + charInfo.OffsetPx.X + charInfo.SrcTextureRectPx.Width;
          const PxValue currentBottomPx = charInfo.OffsetPx.Y + charInfo.SrcTextureRectPx.Height;
          renderRightPx = (currentRightPx > renderRightPx ? currentRightPx : renderRightPx);
          renderBottomPx = (currentBottomPx > renderBottomPx ? currentBottomPx : renderBottomPx);
          layoutXOffsetPx += charInfo.XAdvancePx;
        }
      }
      return {renderRightPx, renderBottomPx};
    }

    void PadResult(Span<SpriteFontGlyphPosition> dst, const uint32_t dstIndex, const uint32_t endDstIndex)
    {
      assert(endDstIndex <= dst.size());
      uint32_t index = dstIndex;
      while (index < endDstIndex)
      {
        dst[index] = {};
        ++index;
      }
    }

    bool DoExtractRenderRules(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar, Span<SpriteFontGlyphPosition> dst,
                              const StringViewLite& strView)
    {
      if (strView.empty())
      {
        return false;
      }
      if (strView.length() > std::numeric_limits<uint32_t>::max())
      {
        throw std::invalid_argument("length can not be larger than a uint32");
      }
      if (dst.size() < strView.length())
      {
        throw std::invalid_argument("rDst is too small");
      }

      // extract information about the characters we are rendering
      int32_t layoutXOffsetPx = 0;
      Utf8StrViewReader reader(strView);
      uint32_t dstIndex = 0;
      while (!reader.IsEmpty())
      {
        const SpriteFontCharInfo& fontCharInfo = lookup.GetChar(reader.NextChar(), unknownChar);

        dst[dstIndex] = SpriteFontGlyphPosition(PxAreaRectangleF::Create(static_cast<float>(layoutXOffsetPx + fontCharInfo.CharInfo.OffsetPx.X.Value),
                                                                         static_cast<float>(fontCharInfo.CharInfo.OffsetPx.Y.Value),
                                                                         static_cast<float>(fontCharInfo.CharInfo.SrcTextureRectPx.Width.Value),
                                                                         static_cast<float>(fontCharInfo.CharInfo.SrcTextureRectPx.Height.Value)),
                                                fontCharInfo.RenderInfo.TextureArea);
        layoutXOffsetPx += fontCharInfo.CharInfo.XAdvancePx.Value;
        ++dstIndex;
      }
      PadResult(dst, dstIndex, UncheckedNumericCast<uint32_t>(strView.length()));
      return true;
    }

    inline PxSize2D DoMeasureStringWithKerning(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar,
                                               const StringViewLite& strView)
    {
      auto renderRightPx = PxValue::Create(0);
      auto renderBottomPx = PxValue::Create(0);
      if (!strView.empty())
      {
        // extract information about the characters we are rendering
        auto layoutXOffsetPx = PxValue::Create(0);
        uint32_t previousChar = 0;
        Utf8StrViewReader reader(strView);
        while (!reader.IsEmpty())
        {
          const uint32_t currentChar = reader.NextChar();
          const CoreFontCharInfo& charInfo = lookup.GetChar(currentChar, unknownChar).CharInfo;
          // apply kerning
          layoutXOffsetPx += lookup.GetKerning(previousChar, currentChar);
          const PxValue currentRightPx = layoutXOffsetPx + charInfo.OffsetPx.X + charInfo.SrcTextureRectPx.Width;
          const PxValue currentBottomPx = charInfo.OffsetPx.Y + charInfo.SrcTextureRectPx.Height;
          renderRightPx = (currentRightPx > renderRightPx ? currentRightPx : renderRightPx);
          renderBottomPx = (currentBottomPx > renderBottomPx ? currentBottomPx : renderBottomPx);
          layoutXOffsetPx += charInfo.XAdvancePx;
          previousChar = currentChar;
        }
      }
      return {renderRightPx, renderBottomPx};
    }

    bool DoExtractRenderRulesWithKerning(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar, Span<SpriteFontGlyphPosition> dst,
                                         const StringViewLite& strView)
    {
      if (strView.empty())
      {
        return false;
      }
      if (strView.length() > std::numeric_limits<uint32_t>::max())
      {
        throw std::invalid_argument("length can not be larger than a uint32");
      }
      if (dst.size() < strView.length())
      {
        throw std::invalid_argument("rDst is too small");
      }

      // extract information about the characters we are rendering
      int32_t layoutXOffsetPx = 0;
      uint32_t previousChar = 0;
      Utf8StrViewReader reader(strView);
      uint32_t dstIndex = 0;
      while (!reader.IsEmpty())
      {
        const uint32_t currentChar = reader.NextChar();
        const SpriteFontCharInfo& fontCharInfo = lookup.GetChar(currentChar, unknownChar);
        // apply kerning
        layoutXOffsetPx += lookup.GetKerning(previousChar, currentChar).Value;

        dst[dstIndex] = SpriteFontGlyphPosition(PxAreaRectangleF::Create(static_cast<float>(layoutXOffsetPx + fontCharInfo.CharInfo.OffsetPx.X.Value),
                                                                         static_cast<float>(fontCharInfo.CharInfo.OffsetPx.Y.Value),
                                                                         static_cast<float>(fontCharInfo.CharInfo.SrcTextureRectPx.Width.Value),
                                                                         static_cast<float>(fontCharInfo.CharInfo.SrcTextureRectPx.Height.Value)),
                                                fontCharInfo.RenderInfo.TextureArea);
        layoutXOffsetPx += fontCharInfo.CharInfo.XAdvancePx.Value;
        previousChar = currentChar;
        ++dstIndex;
      }
      PadResult(dst, dstIndex, UncheckedNumericCast<uint32_t>(strView.length()));
      return true;
    }


    inline PxSize2D DoMeasureStringWithKerning(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar,
                                               const StringViewLite& strView, const float fontScale)
    {
      int32_t renderRightPx = 0;
      int32_t renderBottomPx = 0;
      if (!strView.empty() && fontScale >= 0.0f)
      {
        const auto baseLinePx = lookup.GetBaseLinePx();
        const auto scaledBaseLinePx = ScaledBaseLinePx(baseLinePx, fontScale);

        // extract information about the characters we are rendering

        float layoutXOffsetPxf = 0;
        uint32_t previousChar = 0;
        Utf8StrViewReader reader(strView);
        while (!reader.IsEmpty())
        {
          const uint32_t currentChar = reader.NextChar();
          const CoreFontCharInfo& charInfo = lookup.GetChar(currentChar, unknownChar).CharInfo;

          // apply kerning
          layoutXOffsetPxf += static_cast<float>(lookup.GetKerning(previousChar, currentChar).Value) * fontScale;

          // This should match the algorithm in ExtractRenderRules
          if (charInfo.SrcTextureRectPx.Width.Value > 0u && charInfo.SrcTextureRectPx.Height.Value > 0u)
          {
            // calc distance from original baseline to the startY, then scale it
            // then add the distance to the scaled baseline and round it (this ensures we have high accuracy)
            // we store the kerning offset in a int32_t to ensure that the "-" operation doesn't underflow (due to unsigned subtraction)
            const int32_t glyphHeight = charInfo.SrcTextureRectPx.Height.Value;
            // auto scaledYStartPx = int32_t(std::round((scaledBaseLinePx + (float(charInfo.OffsetPx.Y - baseLinePx) * fontScale))));
            const auto scaledYEndPx =
              static_cast<int32_t>(std::round((static_cast<float>(scaledBaseLinePx.Value) +
                                               (static_cast<float>(charInfo.OffsetPx.Y.Value + glyphHeight - baseLinePx.Value) * fontScale))));

            const auto dstXPx = static_cast<int32_t>(std::round(layoutXOffsetPxf + (static_cast<float>(charInfo.OffsetPx.X.Value) * fontScale)));
            auto dstXEndPx = static_cast<int32_t>(
              std::round(layoutXOffsetPxf + (static_cast<float>((charInfo.OffsetPx.X + charInfo.SrcTextureRectPx.Width).Value) * fontScale)));

            dstXEndPx = dstXEndPx > dstXPx ? dstXEndPx : (dstXEndPx + 1);
            // scaledYStartPx = scaledYStartPx < scaledYEndPx ? scaledYStartPx : (scaledYEndPx - 1);

            const int32_t currentRightPx = dstXEndPx;
            const int32_t currentBottomPx = scaledYEndPx;
            renderRightPx = (currentRightPx >= renderRightPx ? currentRightPx : renderRightPx);
            renderBottomPx = (currentBottomPx > renderBottomPx ? currentBottomPx : renderBottomPx);
          }

          layoutXOffsetPxf += static_cast<float>(charInfo.XAdvancePx.Value) * fontScale;
          previousChar = currentChar;
        }
      }
      return PxSize2D::Create(renderRightPx, renderBottomPx);
    }

    inline PxSize2D DoMeasureStringWithKerningSdf(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar,
                                                  const StringViewLite& strView, const float fontScale)
    {
      float renderRightPxf = 0;
      float renderBottomPxf = 0;
      if (!strView.empty() && fontScale >= 0.0f)
      {
        const auto baseLinePx = lookup.GetBaseLinePx();
        const auto scaledBaseLinePx = ScaledBaseLinePx(baseLinePx, fontScale);

        // extract information about the characters we are rendering

        float layoutXOffsetPxf = 0;
        uint32_t previousChar = 0;
        Utf8StrViewReader reader(strView);
        while (!reader.IsEmpty())
        {
          const uint32_t currentChar = reader.NextChar();
          const CoreFontCharInfo& charInfo = lookup.GetChar(currentChar, unknownChar).CharInfo;

          // apply kerning
          layoutXOffsetPxf += static_cast<float>(lookup.GetKerning(previousChar, currentChar).Value) * fontScale;

          // This should match the algorithm in ExtractRenderRules
          {
            // calc distance from original baseline to the startY, then scale it
            // then add the distance to the scaled baseline and round it (this ensures we have high accuracy)
            // we store the kerning offset in a int32_t to ensure that the "-" operation doesn't underflow (due to unsigned subtraction)
            const int32_t glyphHeight = charInfo.SrcTextureRectPx.Height.Value;
            const float scaledYEndPxf = static_cast<float>(scaledBaseLinePx.Value) +
                                        (static_cast<float>(charInfo.OffsetPx.Y.Value + glyphHeight - baseLinePx.Value) * fontScale);

            const float dstXPxf = layoutXOffsetPxf + (static_cast<float>(charInfo.OffsetPx.X.Value) * fontScale);
            float dstXEndPxf = layoutXOffsetPxf + (static_cast<float>(charInfo.OffsetPx.X.Value + charInfo.SrcTextureRectPx.Width.Value) * fontScale);

            dstXEndPxf = dstXEndPxf > dstXPxf ? dstXEndPxf : (dstXEndPxf + 1);
            // scaledYStartPx = scaledYStartPx < scaledYEndPx ? scaledYStartPx : (scaledYEndPx - 1);

            const float currentRightPxf = dstXEndPxf;
            const float currentBottomPxf = scaledYEndPxf;
            renderRightPxf = (currentRightPxf >= renderRightPxf ? currentRightPxf : renderRightPxf);
            renderBottomPxf = (currentBottomPxf > renderBottomPxf ? currentBottomPxf : renderBottomPxf);
          }

          layoutXOffsetPxf += static_cast<float>(charInfo.XAdvancePx.Value) * fontScale;
          previousChar = currentChar;
        }
      }
      return PxSize2D::Create(static_cast<int32_t>(std::ceil(renderRightPxf)), static_cast<int32_t>(std::ceil(renderBottomPxf)));
    }


    bool DoExtractRenderRules(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar, Span<SpriteFontGlyphPosition> dst,
                              const StringViewLite& strView, const float fontScale)
    {
      if (strView.empty() || fontScale <= 0.0f)
      {
        // Fill(rDst, strView.length(), {});
        return false;
      }
      if (strView.length() > std::numeric_limits<uint32_t>::max())
      {
        throw std::invalid_argument("length can not be larger than a uint32");
      }
      if (dst.size() < strView.length())
      {
        throw std::invalid_argument("rDst is too small");
      }

      const auto baseLinePx = lookup.GetBaseLinePx();
      const auto scaledBaseLinePx = ScaledBaseLinePx(baseLinePx, fontScale);
      // extract information about the characters we are rendering
      float layoutXOffsetPxf = 0.0f;
      uint32_t dstIndex = 0;
      Utf8StrViewReader reader(strView);
      while (!reader.IsEmpty())
      {
        const SpriteFontCharInfo& fontCharInfo = lookup.GetChar(reader.NextChar(), unknownChar);

        // calc distance from original baseline to the startY, then scale it
        // then add the distance to the scaled baseline and round it (this ensures we have high accuracy)
        // we store the kerning offset in a int32_t to ensure that the "-" operation doesn't underflow (due to unsigned subtraction)
        const int32_t glyphYOffsetPx = fontCharInfo.CharInfo.OffsetPx.Y.Value;
        const auto glyphHeight = UncheckedNumericCast<int32_t>(fontCharInfo.CharInfo.SrcTextureRectPx.Height.Value);
        auto scaledYStartPx = static_cast<int32_t>(
          std::round((static_cast<float>(scaledBaseLinePx.Value) + (static_cast<float>(glyphYOffsetPx - baseLinePx.Value) * fontScale))));
        const auto scaledYEndPx = static_cast<int32_t>(std::round(
          (static_cast<float>(scaledBaseLinePx.Value) + (static_cast<float>(glyphYOffsetPx + glyphHeight - baseLinePx.Value) * fontScale))));

        const auto dstXPx =
          static_cast<int32_t>(std::round(layoutXOffsetPxf + (static_cast<float>(fontCharInfo.CharInfo.OffsetPx.X.Value) * fontScale)));
        auto dstXEndPx = static_cast<int32_t>(
          std::round(layoutXOffsetPxf +
                     (static_cast<float>(fontCharInfo.CharInfo.OffsetPx.X.Value + fontCharInfo.CharInfo.SrcTextureRectPx.Width.Value) * fontScale)));

        if (dstXPx >= dstXEndPx)
        {
          dstXEndPx = fontCharInfo.CharInfo.SrcTextureRectPx.Width.Value > 0u ? (dstXEndPx + 1) : dstXEndPx;
        }
        if (scaledYStartPx >= scaledYEndPx)
        {
          scaledYStartPx = glyphHeight > 0 ? (scaledYEndPx - 1) : scaledYEndPx;
        }

        dst[dstIndex] =
          SpriteFontGlyphPosition(PxAreaRectangleF::CreateFromLeftTopRightBottom(static_cast<float>(dstXPx), static_cast<float>(scaledYStartPx),
                                                                                 static_cast<float>(dstXEndPx), static_cast<float>(scaledYEndPx)),
                                  fontCharInfo.RenderInfo.TextureArea);
        layoutXOffsetPxf += static_cast<float>(fontCharInfo.CharInfo.XAdvancePx.Value) * fontScale;
        ++dstIndex;
      }
      PadResult(dst, dstIndex, UncheckedNumericCast<uint32_t>(strView.length()));
      return true;
    }

    inline PxSize2D DoMeasureString(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar, const StringViewLite& strView,
                                    const float fontScale)
    {
      int32_t renderRightPx = 0;
      int32_t renderBottomPx = 0;
      if (!strView.empty() || fontScale <= 0.0f)
      {
        const auto baseLinePx = lookup.GetBaseLinePx();
        const auto scaledBaseLinePx = ScaledBaseLinePx(baseLinePx, fontScale);

        // extract information about the characters we are rendering

        float layoutXOffsetPxf = 0;
        Utf8StrViewReader reader(strView);
        while (!reader.IsEmpty())
        {
          const CoreFontCharInfo& charInfo = lookup.GetChar(reader.NextChar(), unknownChar).CharInfo;

          // This should match the algorithm in ExtractRenderRules
          {
            // calc distance from original baseline to the startY, then scale it
            // then add the distance to the scaled baseline and round it (this ensures we have high accuracy)
            // we store the kerning offset in a int32_t to ensure that the "-" operation doesn't underflow (due to unsigned subtraction)
            const int32_t glyphHeight = charInfo.SrcTextureRectPx.Height.Value;
            // auto scaledYStartPx = int32_t(std::round((scaledBaseLinePx + (float(charInfo.OffsetPx.Y - baseLinePx) * fontScale))));
            const auto scaledYEndPx =
              static_cast<int32_t>(std::round((static_cast<float>(scaledBaseLinePx.Value) +
                                               (static_cast<float>(charInfo.OffsetPx.Y.Value + glyphHeight - baseLinePx.Value) * fontScale))));

            const auto dstXPx = static_cast<int32_t>(std::round(layoutXOffsetPxf + (static_cast<float>(charInfo.OffsetPx.X.Value) * fontScale)));
            auto dstXEndPx = static_cast<int32_t>(
              std::round(layoutXOffsetPxf + (static_cast<float>((charInfo.OffsetPx.X + charInfo.SrcTextureRectPx.Width).Value) * fontScale)));

            dstXEndPx = dstXEndPx > dstXPx ? dstXEndPx : (dstXEndPx + 1);
            // scaledYStartPx = scaledYStartPx < scaledYEndPx ? scaledYStartPx : (scaledYEndPx - 1);

            const int32_t currentRightPx = dstXEndPx;
            const int32_t currentBottomPx = scaledYEndPx;
            renderRightPx = (currentRightPx >= renderRightPx ? currentRightPx : renderRightPx);
            renderBottomPx = (currentBottomPx > renderBottomPx ? currentBottomPx : renderBottomPx);
          }

          layoutXOffsetPxf += static_cast<float>(charInfo.XAdvancePx.Value) * fontScale;
        }
      }
      return PxSize2D::Create(renderRightPx, renderBottomPx);
    }

    bool DoExtractRenderRulesWithKerning(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar, Span<SpriteFontGlyphPosition> dst,
                                         const StringViewLite& strView, const float fontScale)
    {
      if (strView.empty() || fontScale <= 0.0f)
      {
        // Fill(rDst, strView.length(), {});
        return false;
      }
      if (strView.length() > std::numeric_limits<uint32_t>::max())
      {
        throw std::invalid_argument("length can not be larger than a uint32");
      }
      if (dst.size() < strView.length())
      {
        throw std::invalid_argument("rDst is too small");
      }

      const auto baseLinePx = lookup.GetBaseLinePx();
      const auto scaledBaseLinePx = ScaledBaseLinePx(baseLinePx, fontScale);
      // extract information about the characters we are rendering
      float layoutXOffsetPxf = 0.0f;
      uint32_t previousChar = 0;
      uint32_t dstIndex = 0;
      Utf8StrViewReader reader(strView);
      while (!reader.IsEmpty())
      {
        const uint32_t currentChar = reader.NextChar();
        const SpriteFontCharInfo& fontCharInfo = lookup.GetChar(currentChar, unknownChar);
        // apply kerning
        layoutXOffsetPxf += static_cast<float>(lookup.GetKerning(previousChar, currentChar).Value) * fontScale;

        // calc distance from original baseline to the startY, then scale it
        // then add the distance to the scaled baseline and round it (this ensures we have high accuracy)
        // we store the kerning offset in a int32_t to ensure that the "-" operation doesn't underflow (due to unsigned subtraction)
        const int32_t glyphYOffsetPx = fontCharInfo.CharInfo.OffsetPx.Y.Value;
        const auto glyphHeight = UncheckedNumericCast<int32_t>(fontCharInfo.CharInfo.SrcTextureRectPx.Height.Value);
        auto scaledYStartPx = static_cast<int32_t>(
          std::round((static_cast<float>(scaledBaseLinePx.Value) + (static_cast<float>(glyphYOffsetPx - baseLinePx.Value) * fontScale))));
        const auto scaledYEndPx = static_cast<int32_t>(std::round(
          (static_cast<float>(scaledBaseLinePx.Value) + (static_cast<float>(glyphYOffsetPx + glyphHeight - baseLinePx.Value) * fontScale))));

        const auto dstXPx =
          static_cast<int32_t>(std::round(layoutXOffsetPxf + (static_cast<float>(fontCharInfo.CharInfo.OffsetPx.X.Value) * fontScale)));
        auto dstXEndPx = static_cast<int32_t>(
          std::round(layoutXOffsetPxf +
                     (static_cast<float>((fontCharInfo.CharInfo.OffsetPx.X + fontCharInfo.CharInfo.SrcTextureRectPx.Width).Value) * fontScale)));


        if (dstXPx >= dstXEndPx)
        {
          dstXEndPx = fontCharInfo.CharInfo.SrcTextureRectPx.Width.Value > 0u ? (dstXEndPx + 1) : dstXEndPx;
        }
        if (scaledYStartPx >= scaledYEndPx)
        {
          scaledYStartPx = glyphHeight > 0 ? (scaledYEndPx - 1) : scaledYEndPx;
        }

        dst[dstIndex] =
          SpriteFontGlyphPosition(PxAreaRectangleF::CreateFromLeftTopRightBottom(static_cast<float>(dstXPx), static_cast<float>(scaledYStartPx),
                                                                                 static_cast<float>(dstXEndPx), static_cast<float>(scaledYEndPx)),
                                  fontCharInfo.RenderInfo.TextureArea);
        layoutXOffsetPxf += static_cast<float>(fontCharInfo.CharInfo.XAdvancePx.Value) * fontScale;
        previousChar = currentChar;
        ++dstIndex;
      }
      PadResult(dst, dstIndex, UncheckedNumericCast<uint32_t>(strView.length()));
      return true;
    }

    /// <summary>
    /// For now the SDF rendering layout engine only aligns the baseline to the pixel grid.
    /// </summary>
    /// <param name="lookup"></param>
    /// <param name="unknownChar"></param>
    /// <param name="rDst"></param>
    /// <param name="strView"></param>
    /// <param name="fontScale"></param>
    /// <returns></returns>
    bool DoExtractRenderRulesWithKerningSdf(const SpriteFontFastLookup& lookup, const SpriteFontCharInfo& unknownChar,
                                            Span<SpriteFontGlyphPosition> dst, const StringViewLite& strView, const float fontScale)
    {
      if (strView.empty() || fontScale <= 0.0f)
      {
        // Fill(rDst, strView.length(), {});
        return false;
      }
      if (strView.length() > std::numeric_limits<uint32_t>::max())
      {
        throw std::invalid_argument("length can not be larger than a uint32");
      }
      if (dst.size() < strView.length())
      {
        throw std::invalid_argument("rDst is too small");
      }

      const auto baseLinePx = lookup.GetBaseLinePx();
      const auto scaledBaseLinePx = ScaledBaseLinePx(baseLinePx, fontScale);
      // extract information about the characters we are rendering
      float layoutXOffsetPxf = 0.0f;
      uint32_t previousChar = 0;
      uint32_t dstIndex = 0;
      Utf8StrViewReader reader(strView);
      while (!reader.IsEmpty())
      {
        const uint32_t currentChar = reader.NextChar();
        const SpriteFontCharInfo& fontCharInfo = lookup.GetChar(currentChar, unknownChar);
        // apply kerning
        layoutXOffsetPxf += static_cast<float>(lookup.GetKerning(previousChar, currentChar).Value) * fontScale;

        // calc distance from original baseline to the startY, then scale it
        // then add the distance to the scaled baseline and round it (this ensures we have high accuracy)
        // we store the kerning offset in a int32_t to ensure that the "-" operation doesn't underflow (due to unsigned subtraction)
        const int32_t glyphYOffsetPx = fontCharInfo.CharInfo.OffsetPx.Y.Value;
        const auto glyphHeight = UncheckedNumericCast<int32_t>(fontCharInfo.CharInfo.SrcTextureRectPx.Height.Value);
        float scaledYStartPxf = static_cast<float>(scaledBaseLinePx.Value) + (static_cast<float>(glyphYOffsetPx - baseLinePx.Value) * fontScale);
        const float scaledYEndPxf =
          static_cast<float>(scaledBaseLinePx.Value) + (static_cast<float>(glyphYOffsetPx + glyphHeight - baseLinePx.Value) * fontScale);

        const float dstXPxf = layoutXOffsetPxf + (static_cast<float>(fontCharInfo.CharInfo.OffsetPx.X.Value) * fontScale);
        float dstXEndPxf = layoutXOffsetPxf +
                           (static_cast<float>((fontCharInfo.CharInfo.OffsetPx.X + fontCharInfo.CharInfo.SrcTextureRectPx.Width).Value) * fontScale);

        if (dstXPxf >= dstXEndPxf)
        {
          dstXEndPxf = fontCharInfo.CharInfo.SrcTextureRectPx.Width.Value > 0u ? (dstXEndPxf + 1) : dstXEndPxf;
        }
        if (scaledYStartPxf >= scaledYEndPxf)
        {
          scaledYStartPxf = glyphHeight > 0 ? (scaledYEndPxf - 1) : scaledYEndPxf;
        }

        dst[dstIndex] = SpriteFontGlyphPosition(PxAreaRectangleF::CreateFromLeftTopRightBottom(dstXPxf, scaledYStartPxf, dstXEndPxf, scaledYEndPxf),
                                                fontCharInfo.RenderInfo.TextureArea);
        layoutXOffsetPxf += static_cast<float>(fontCharInfo.CharInfo.XAdvancePx.Value) * fontScale;
        previousChar = currentChar;
        ++dstIndex;
      }
      PadResult(dst, dstIndex, UncheckedNumericCast<uint32_t>(strView.length()));
      return true;
    }
  }

  TextureAtlasSpriteFont::TextureAtlasSpriteFont(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const PxExtent2D textureExtentPx,
                                                 const ITextureAtlas& textureAtlas, const IFontBasicKerning& basicFontKerning,
                                                 const uint32_t densityDpi)
    : TextureAtlasSpriteFont(spriteNativeAreaCalc, textureExtentPx, BitmapFontConverter::ToBitmapFont(textureAtlas, basicFontKerning), densityDpi)
  {
  }


  TextureAtlasSpriteFont::TextureAtlasSpriteFont(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const PxExtent2D textureExtentPx,
                                                 const BitmapFont& bitmapFont, const uint32_t densityDpi)
    : m_lookup(spriteNativeAreaCalc, textureExtentPx, bitmapFont, densityDpi)
    , m_charPaddingPx(bitmapFont.GetPaddingPx())
    , m_fontType(bitmapFont.GetFontType())
    , m_sdfParams(bitmapFont.GetSdfParams())
  {
    // dump kerning info
    // auto kerningSpan = bitmapFont.GetKernings();
    // for (std::size_t i = 0; i < kerningSpan.size(); ++i)
    //{
    //  FSLLOG3_INFO("{} {}", (char)kerningSpan[i].First, (char)kerningSpan[i].Second);
    //}
  }

  void TextureAtlasSpriteFont::Reset(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const PxExtent2D textureExtentPx, const BitmapFont& bitmapFont,
                                     const uint32_t densityDpi)
  {
    Reset();
    m_lookup = SpriteFontFastLookup(spriteNativeAreaCalc, textureExtentPx, bitmapFont, densityDpi);
    m_charPaddingPx = bitmapFont.GetPaddingPx();
    m_fontType = bitmapFont.GetFontType();
    m_sdfParams = bitmapFont.GetSdfParams();
  }

  void TextureAtlasSpriteFont::Reset(const SpriteNativeAreaCalc& spriteNativeAreaCalc, const PxExtent2D textureExtentPx,
                                     const ITextureAtlas& textureAtlas, const IFontBasicKerning& basicFontKerning, const uint32_t densityDpi)
  {
    Reset(spriteNativeAreaCalc, textureExtentPx, BitmapFontConverter::ToBitmapFont(textureAtlas, basicFontKerning), densityDpi);
  }


  PxValueU16 TextureAtlasSpriteFont::BaseLinePx(const BitmapFontConfig& fontConfig) const
  {
    return ScaledBaseLinePx(m_lookup.GetBaseLinePx(), fontConfig.Scale);
  }


  PxValueU16 TextureAtlasSpriteFont::LineSpacingPx(const BitmapFontConfig& fontConfig) const
  {
    return PxValueU16(UncheckedNumericCast<uint16_t>(
      static_cast<int32_t>(std::round(static_cast<float>(m_lookup.GetLineSpacingPx().Value) * std::max(fontConfig.Scale, 0.0f)))));
  }


  PxSize2D TextureAtlasSpriteFont::MeasureString(const StringViewLite& strView) const
  {
    return m_lookup.HasKerning() ? DoMeasureStringWithKerning(m_lookup, m_unknownChar, strView) : DoMeasureString(m_lookup, m_unknownChar, strView);
  }


  PxSize2D TextureAtlasSpriteFont::MeasureString(const StringViewLite& strView, const BitmapFontConfig& fontConfig) const
  {
    PxSize2D result;
    if (fontConfig.Kerning && m_lookup.HasKerning())
    {
      if (fontConfig.Scale == 1.0f)
      {
        result = DoMeasureStringWithKerning(m_lookup, m_unknownChar, strView);
      }
      else
      {
        result = m_fontType == BitmapFontType::Bitmap ? DoMeasureStringWithKerning(m_lookup, m_unknownChar, strView, fontConfig.Scale)
                                                      : DoMeasureStringWithKerningSdf(m_lookup, m_unknownChar, strView, fontConfig.Scale);
      }
    }
    else
    {
      result = fontConfig.Scale == 1.0f ? DoMeasureString(m_lookup, m_unknownChar, strView)
                                        : DoMeasureString(m_lookup, m_unknownChar, strView, fontConfig.Scale);
    }
    return result;
  }

  bool TextureAtlasSpriteFont::ExtractRenderRules(Span<SpriteFontGlyphPosition> dst, const StringViewLite& strView) const
  {
    return ExtractRenderRules(dst, strView, BitmapFontConfig(1.0f, m_lookup.HasKerning()));
  }

  bool TextureAtlasSpriteFont::ExtractRenderRules(Span<SpriteFontGlyphPosition> dst, const StringViewLite& strView,
                                                  const BitmapFontConfig& fontConfig) const
  {
    bool result = false;
    if (fontConfig.Kerning && m_lookup.HasKerning())
    {
      if (fontConfig.Scale == 1.0f)
      {
        result = DoExtractRenderRulesWithKerning(m_lookup, m_unknownChar, dst, strView);
      }
      else
      {
        result = m_fontType == BitmapFontType::Bitmap ? DoExtractRenderRulesWithKerning(m_lookup, m_unknownChar, dst, strView, fontConfig.Scale)
                                                      : DoExtractRenderRulesWithKerningSdf(m_lookup, m_unknownChar, dst, strView, fontConfig.Scale);
      }
    }
    else
    {
      result = fontConfig.Scale == 1.0f ? DoExtractRenderRules(m_lookup, m_unknownChar, dst, strView)
                                        : DoExtractRenderRules(m_lookup, m_unknownChar, dst, strView, fontConfig.Scale);
    }
    return result;
  }
}
