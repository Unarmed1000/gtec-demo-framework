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
#include <FslUtil/Vulkan1_0/VUSwapchainPresentWait.hpp>
#include <stdexcept>

using namespace Fsl::Vulkan;

// A swapchain needs a surface, which a unit test does not have. So what is tested is what the class does without one: nothing, and
// it says so.

TEST(TestFslUtil_Vulkan1_0_VUSwapchainPresentWait, Default)
{
  const VUSwapchainPresentWait presentWait;

  EXPECT_FALSE(presentWait.IsEnabled());
  EXPECT_EQ(VK_ERROR_FEATURE_NOT_PRESENT, presentWait.Wait(1, 0));
}


TEST(TestFslUtil_Vulkan1_0_VUSwapchainPresentWait, PreparePresent_NotEnabled)
{
  VUSwapchainPresentWait presentWait;
  VUPresentWaitPresentInfo presentInfo;
  const int next = 0;

  // The chain is returned as it was given, no present id is added
  EXPECT_EQ(&next, presentWait.PreparePresent(presentInfo, 1, &next));
  EXPECT_EQ(nullptr, presentWait.PreparePresent(presentInfo, 2, nullptr));
}


TEST(TestFslUtil_Vulkan1_0_VUSwapchainPresentWait, GetSwapchainCreateFlags_NullHandles)
{
  EXPECT_EQ(0u, VUSwapchainPresentWait::GetSwapchainCreateFlags(VK_NULL_HANDLE, VK_NULL_HANDLE));
}


TEST(TestFslUtil_Vulkan1_0_VUSwapchainPresentWait, Reset_NoArguments)
{
  VUSwapchainPresentWait presentWait;

  presentWait.Reset();

  EXPECT_FALSE(presentWait.IsEnabled());
}

#ifdef FSL_VULKAN_PRESENT_WAIT_SUPPORTED

TEST(TestFslUtil_Vulkan1_0_VUSwapchainPresentWait, Reset_NullHandles)
{
  VUSwapchainPresentWait presentWait;

  EXPECT_THROW(presentWait.Reset(VK_NULL_HANDLE, VK_NULL_HANDLE), std::invalid_argument);
  EXPECT_FALSE(presentWait.IsEnabled());
}

#endif
