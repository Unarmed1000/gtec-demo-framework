//****************************************************************************************************************************************************
//* BSD 3-Clause License
//*
//* Copyright (c) 2026, Mana Battery
//* All rights reserved.
//*
//* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
//*    documentation and/or other materials provided with the distribution.
//* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
//*    software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
//* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
//* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//****************************************************************************************************************************************************


#include <FslBase/UnitTest/Helper/TestFixtureFslBase.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/Linux/DrmFdInfoParser.hpp>

using namespace Fsl;

namespace
{
  using Test_DrmFdInfoParser = TestFixtureFslBase;

  // What the kernel documentation ("DRM client usage stats") describes
  constexpr std::string_view Client =
    "pos:\t0\n"
    "flags:\t02100002\n"
    "mnt_id:\t26\n"
    "ino:\t284\n"
    "drm-driver:\ti915\n"
    "drm-client-id:\t7\n"
    "drm-pdev:\t0000:00:02.0\n"
    "drm-total-system0:\t3072 KiB\n"
    "drm-shared-system0:\t0\n"
    "drm-active-system0:\t0\n"
    "drm-resident-system0:\t2048 KiB\n"
    "drm-purgeable-system0:\t0\n"
    "drm-total-stolen-system0:\t1 MiB\n"
    "drm-resident-stolen-system0:\t512\n"
    "drm-engine-render:\t9288864723 ns\n"
    "drm-engine-copy:\t2035071108 ns\n"
    "drm-engine-video:\t0 ns\n"
    "drm-engine-capacity-video:\t2\n"
    "drm-engine-video-enhance:\t0 ns\n";
}


TEST_F(Test_DrmFdInfoParser, Parse_Client)
{
  const DrmFdInfo info = DrmFdInfoParser::Parse(Client);

  EXPECT_TRUE(info.IsDrmClient);
  EXPECT_EQ(7u, info.ClientId);

  // The capacity of a engine is not a engine
  ASSERT_EQ(4u, info.EngineCount);
  EXPECT_EQ("render", info.Engines[0].GetName());
  EXPECT_EQ(9288864723u, info.Engines[0].BusyNanoseconds);
  EXPECT_EQ("copy", info.Engines[1].GetName());
  EXPECT_EQ(2035071108u, info.Engines[1].BusyNanoseconds);
  EXPECT_EQ("video", info.Engines[2].GetName());
  EXPECT_EQ(0u, info.Engines[2].BusyNanoseconds);
  EXPECT_EQ("video-enhance", info.Engines[3].GetName());

  // What is resident, summed over the regions: 2048 KiB and 512 bytes
  EXPECT_TRUE(info.HasMemory);
  EXPECT_EQ((2048u * 1024u) + 512u, info.MemoryBytes);
}


TEST_F(Test_DrmFdInfoParser, Parse_TotalIsUsedWithoutResident)
{
  const DrmFdInfo info = DrmFdInfoParser::Parse("drm-client-id: 3\ndrm-total-memory: 5 MiB\ndrm-total-other: 100 KiB\n");

  EXPECT_TRUE(info.IsDrmClient);
  EXPECT_TRUE(info.HasMemory);
  EXPECT_EQ((5u * 1024u * 1024u) + (100u * 1024u), info.MemoryBytes);
  EXPECT_EQ(0u, info.EngineCount);
}


TEST_F(Test_DrmFdInfoParser, Parse_DeprecatedMemoryKeyIsIgnored)
{
  const DrmFdInfo info = DrmFdInfoParser::Parse("drm-client-id: 3\ndrm-memory-vram: 5 MiB\n");

  EXPECT_TRUE(info.IsDrmClient);
  EXPECT_FALSE(info.HasMemory);
  EXPECT_EQ(0u, info.MemoryBytes);
}


TEST_F(Test_DrmFdInfoParser, Parse_NotADrmClient)
{
  // A normal file, and something that only looks a bit like a client
  EXPECT_FALSE(DrmFdInfoParser::Parse("pos:\t0\nflags:\t0100002\nmnt_id:\t21\n").IsDrmClient);
  const DrmFdInfo info = DrmFdInfoParser::Parse("drm-engine-render: 100 ns\ndrm-resident-memory: 5 MiB\n");
  EXPECT_FALSE(info.IsDrmClient);
  EXPECT_EQ(0u, info.EngineCount);
  EXPECT_FALSE(info.HasMemory);
}


TEST_F(Test_DrmFdInfoParser, Parse_Empty)
{
  const DrmFdInfo info = DrmFdInfoParser::Parse("");

  EXPECT_FALSE(info.IsDrmClient);
  EXPECT_EQ(0u, info.EngineCount);
  EXPECT_FALSE(info.HasMemory);
}


TEST_F(Test_DrmFdInfoParser, Parse_BadValuesAreSkipped)
{
  const DrmFdInfo info = DrmFdInfoParser::Parse(
    "drm-client-id: 9\n"
    "drm-engine-render: fast ns\n"
    "drm-engine-copy: 100 ms\n"
    "drm-engine-compute: 100\n"
    "drm-engine-: 100 ns\n"
    "drm-engine-good: 100 ns\n"
    "drm-resident-a: 5 GiB\n"
    "drm-resident-b: -5 KiB\n"
    "drm-resident-c: 99999999999999999999999 KiB\n"
    "drm-resident-d: 7 KiB\n"
    "a line without a separator\n");

  EXPECT_TRUE(info.IsDrmClient);
  ASSERT_EQ(1u, info.EngineCount);
  EXPECT_EQ("good", info.Engines[0].GetName());
  EXPECT_TRUE(info.HasMemory);
  EXPECT_EQ(7u * 1024u, info.MemoryBytes);
}


TEST_F(Test_DrmFdInfoParser, Parse_NoLineEndAtTheEnd_WindowsLineEnds)
{
  const DrmFdInfo info = DrmFdInfoParser::Parse("drm-client-id: 12\r\ndrm-engine-render: 42 ns");

  EXPECT_TRUE(info.IsDrmClient);
  EXPECT_EQ(12u, info.ClientId);
  ASSERT_EQ(1u, info.EngineCount);
  EXPECT_EQ(42u, info.Engines[0].BusyNanoseconds);
}


TEST_F(Test_DrmFdInfoParser, Parse_MoreEnginesThanRoom)
{
  std::string content("drm-client-id: 1\n");
  for (uint32_t i = 0; i < (DrmFdInfo::MaxEngines + 4u); ++i)
  {
    content += "drm-engine-e" + std::to_string(i) + ": " + std::to_string(i) + " ns\n";
  }
  const DrmFdInfo info = DrmFdInfoParser::Parse(content);

  EXPECT_EQ(DrmFdInfo::MaxEngines, info.EngineCount);
}
