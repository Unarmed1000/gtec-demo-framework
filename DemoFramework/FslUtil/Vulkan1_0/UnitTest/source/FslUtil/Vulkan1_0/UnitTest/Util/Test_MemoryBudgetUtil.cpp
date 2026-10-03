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

#include <FslUtil/Vulkan1_0/Util/MemoryBudgetUtil.hpp>
#include "TestFixtureFslUtil_Vulkan1_0_PhysicalDevice.hpp"

using namespace Fsl;
using namespace Fsl::Vulkan;

namespace
{
  using TestFixtureFslUtil_Vulkan1_0_MemoryBudgetUtil = TestFixtureFslUtil_Vulkan1_0_PhysicalDevice;
  using TestFixtureFslUtil_Vulkan1_0_VUMemoryBudget = TestFixtureFslUtil_Vulkan1_0;
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUMemoryBudget, Default)
{
  const VUMemoryBudget budget;

  EXPECT_EQ(0u, budget.HeapCount);
  EXPECT_EQ(0u, budget.GetDeviceLocalUsage());
  EXPECT_EQ(0u, budget.GetDeviceLocalBudget());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_VUMemoryBudget, DeviceLocalSums)
{
  VUMemoryBudget budget;
  budget.HeapCount = 3;
  budget.Heaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
  budget.Heaps[1].flags = 0;
  budget.Heaps[2].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
  // A entry beyond HeapCount is not part of the sums
  budget.Heaps[3].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
  budget.HeapUsage = {1, 10, 100, 1000};
  budget.HeapBudget = {2, 20, 200, 2000};

  EXPECT_EQ(101u, budget.GetDeviceLocalUsage());
  EXPECT_EQ(202u, budget.GetDeviceLocalBudget());
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_MemoryBudgetUtil, NullHandle)
{
  VUMemoryBudget budget;
  budget.HeapCount = 4;

  EXPECT_FALSE(MemoryBudgetUtil::TryGetMemoryBudget(VK_NULL_HANDLE, budget));
  EXPECT_EQ(0u, budget.HeapCount);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_MemoryBudgetUtil, GetMemoryBudget)
{
  if (!IsReady())
  {
    SkipTest("GetMemoryBudget");
    return;
  }
  if (!MemoryBudgetUtil::IsSupported(m_physicalDevice))
  {
    SkipTest("GetMemoryBudget", "VK_EXT_memory_budget is not supported");
    return;
  }

  VkPhysicalDeviceMemoryProperties memoryProperties{};
  vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memoryProperties);

  const VUMemoryBudget budget = MemoryBudgetUtil::GetMemoryBudget(m_physicalDevice);

  ASSERT_EQ(memoryProperties.memoryHeapCount, budget.HeapCount);
  for (uint32_t i = 0; i < budget.HeapCount; ++i)
  {
    EXPECT_EQ(memoryProperties.memoryHeaps[i].size, budget.Heaps[i].size);
    EXPECT_EQ(memoryProperties.memoryHeaps[i].flags, budget.Heaps[i].flags);
    EXPECT_GT(budget.HeapBudget[i], 0u);
  }
  EXPECT_GT(budget.GetDeviceLocalBudget(), 0u);
}


TEST_F(TestFixtureFslUtil_Vulkan1_0_MemoryBudgetUtil, TryGetMemoryBudget)
{
  if (!IsReady())
  {
    SkipTest("TryGetMemoryBudget");
    return;
  }

  VUMemoryBudget budget;
  const bool isSupported = MemoryBudgetUtil::IsSupported(m_physicalDevice);

  EXPECT_EQ(isSupported, MemoryBudgetUtil::TryGetMemoryBudget(m_physicalDevice, budget));
  EXPECT_EQ(isSupported, budget.HeapCount > 0u);
}
