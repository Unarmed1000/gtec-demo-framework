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
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <algorithm>
#include <stdexcept>

namespace Fsl::Vulkan
{
  namespace
  {
    template <typename TArray>
    VkDeviceSize SumDeviceLocal(const VUMemoryBudget& budget, const TArray& values) noexcept
    {
      VkDeviceSize sum = 0;
      for (uint32_t i = 0; i < budget.HeapCount; ++i)
      {
        if ((budget.Heaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u)
        {
          sum += values[i];
        }
      }
      return sum;
    }
  }


  VkDeviceSize VUMemoryBudget::GetDeviceLocalUsage() const noexcept
  {
    return SumDeviceLocal(*this, HeapUsage);
  }


  VkDeviceSize VUMemoryBudget::GetDeviceLocalBudget() const noexcept
  {
    return SumDeviceLocal(*this, HeapBudget);
  }


  namespace MemoryBudgetUtil
  {
#ifdef VK_EXT_memory_budget

    bool IsSupported(const VkPhysicalDevice physicalDevice)
    {
      const char* const pszExtensionName = VK_EXT_MEMORY_BUDGET_EXTENSION_NAME;
      return PhysicalDeviceUtil::IsDeviceExtensionsAvailable(physicalDevice, 1, &pszExtensionName);
    }


    VUMemoryBudget GetMemoryBudget(const VkPhysicalDevice physicalDevice)
    {
      if (physicalDevice == VK_NULL_HANDLE)
      {
        throw std::invalid_argument("physicalDevice can not be VK_NULL_HANDLE");
      }

      VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProperties{};
      budgetProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;

      VkPhysicalDeviceMemoryProperties2 memoryProperties{};
      memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
      memoryProperties.pNext = &budgetProperties;
      vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memoryProperties);

      VUMemoryBudget result;
      result.HeapCount = std::min(memoryProperties.memoryProperties.memoryHeapCount, static_cast<uint32_t>(VK_MAX_MEMORY_HEAPS));
      for (uint32_t i = 0; i < result.HeapCount; ++i)
      {
        result.Heaps[i] = memoryProperties.memoryProperties.memoryHeaps[i];
        result.HeapBudget[i] = budgetProperties.heapBudget[i];
        result.HeapUsage[i] = budgetProperties.heapUsage[i];
      }
      return result;
    }

#else

    bool IsSupported(const VkPhysicalDevice /*physicalDevice*/)
    {
      return false;
    }


    VUMemoryBudget GetMemoryBudget(const VkPhysicalDevice /*physicalDevice*/)
    {
      return {};
    }

#endif

    bool TryGetMemoryBudget(const VkPhysicalDevice physicalDevice, VUMemoryBudget& rBudget)
    {
      if (physicalDevice == VK_NULL_HANDLE || !IsSupported(physicalDevice))
      {
        rBudget = {};
        return false;
      }
      rBudget = GetMemoryBudget(physicalDevice);
      return true;
    }
  }
}
