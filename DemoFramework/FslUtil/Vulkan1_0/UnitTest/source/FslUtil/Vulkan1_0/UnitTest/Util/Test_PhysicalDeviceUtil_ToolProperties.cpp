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

// Include gtest before anything that includes the polluted standard X11 header that cause all kind of issues withs its bad defines.
#include <gtest/gtest.h>
// Then include the rest
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <algorithm>
#include <cstring>
#include "TestFixtureFslUtil_Vulkan1_0_PhysicalDevice.hpp"

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_PhysicalDeviceUtil_ToolProperties = TestFixtureFslUtil_Vulkan1_0_PhysicalDevice;
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_PhysicalDeviceUtil_ToolProperties, GetToolProperties)
{
  if (!IsReady())
  {
    SkipTest("GetToolProperties");
    return;
  }

  const auto tools = PhysicalDeviceUtil::GetToolProperties(m_physicalDevice);

  for (const auto& tool : tools)
  {
    EXPECT_EQ(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TOOL_PROPERTIES, tool.sType);
    EXPECT_GT(std::strlen(tool.name), 0u);
  }
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_PhysicalDeviceUtil_ToolProperties, GetToolProperties_ValidationLayerIsATool)
{
  if (!IsReady())
  {
    SkipTest("GetToolProperties_ValidationLayerIsATool");
    return;
  }
  if (!m_hasValidationLayer)
  {
    SkipTest("GetToolProperties_ValidationLayerIsATool", "VK_LAYER_KHRONOS_validation is not installed");
    return;
  }

  const auto tools = PhysicalDeviceUtil::GetToolProperties(m_physicalDevice);

  // The instance was created with the validation layer, so it is one of the attached tools
  EXPECT_TRUE(std::any_of(tools.begin(), tools.end(),
                          [](const VkPhysicalDeviceToolProperties& tool) { return (tool.purposes & VK_TOOL_PURPOSE_VALIDATION_BIT) != 0u; }));
}
