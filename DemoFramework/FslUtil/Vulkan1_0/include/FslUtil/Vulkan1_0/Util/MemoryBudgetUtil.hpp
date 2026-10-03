#ifndef FSLUTIL_VULKAN1_0_UTIL_MEMORYBUDGETUTIL_HPP
#define FSLUTIL_VULKAN1_0_UTIL_MEMORYBUDGETUTIL_HPP
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

// Make sure Common.hpp is the first include file (to make the error message as helpful as possible when disabled)
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>

namespace Fsl::Vulkan
{
  //! The memory heaps of a physical device and how much of them this process uses (VK_EXT_memory_budget).
  struct VUMemoryBudget
  {
    //! The number of valid entries in the arrays
    uint32_t HeapCount{0};
    std::array<VkMemoryHeap, VK_MAX_MEMORY_HEAPS> Heaps{};
    //! How much this process can allocate from the heap before allocations may fail or get slow (it changes with what the rest of the system uses)
    std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> HeapBudget{};
    //! How much this process is currently using of the heap
    std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> HeapUsage{};

    //! @return the sum of the usage of the heaps that are local to the device (the GPU memory).
    [[nodiscard]] VkDeviceSize GetDeviceLocalUsage() const noexcept;

    //! @return the sum of the budget of the heaps that are local to the device (the GPU memory).
    [[nodiscard]] VkDeviceSize GetDeviceLocalBudget() const noexcept;
  };


  namespace MemoryBudgetUtil
  {
    //! @return true if the physical device supports VK_EXT_memory_budget (and the Vulkan headers this was built with know it).
    bool IsSupported(const VkPhysicalDevice physicalDevice);

    //! @brief Get the current memory budget and usage.
    //! @note  Only call this for a physical device that IsSupported, as the extension support is not checked here. The values are cheap to
    //!        query, but they are a snapshot so query them again when a current value is needed.
    VUMemoryBudget GetMemoryBudget(const VkPhysicalDevice physicalDevice);

    //! @brief Get the current memory budget and usage if the physical device supports it.
    //! @note  This checks IsSupported on every call, so prefer IsSupported once + GetMemoryBudget for repeated queries.
    bool TryGetMemoryBudget(const VkPhysicalDevice physicalDevice, VUMemoryBudget& rBudget);
  }
}

#endif
