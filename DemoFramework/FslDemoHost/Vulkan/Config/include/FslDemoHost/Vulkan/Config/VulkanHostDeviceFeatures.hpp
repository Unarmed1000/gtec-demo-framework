#ifndef FSLDEMOHOST_VULKAN_CONFIG_VULKANHOSTDEVICEFEATURES_HPP
#define FSLDEMOHOST_VULKAN_CONFIG_VULKANHOSTDEVICEFEATURES_HPP
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

#include <FslUtil/Vulkan1_0/Debug/VUDeviceFault.hpp>

namespace Fsl::Vulkan
{
  //! The optional device extensions the host enabled on the device (see HostDeviceExtensions).
  //! They are all optional, so check before using what they provide.
  struct VulkanHostDeviceFeatures
  {
    //! The device fault extension that was enabled with its deviceFault feature (Disabled if none)
    VUDeviceFaultApi DeviceFault{VUDeviceFaultApi::Disabled};
    //! True if VK_KHR_calibrated_timestamps (or the EXT version) was enabled, see VUCalibratedTimestamps
    bool CalibratedTimestamps{false};
    //! True if VK_EXT_present_timing and VK_KHR_present_id2 were enabled with their features. A swapchain can then report when its images were
    //! presented if its surface supports that too, see VUSwapchainPresentTiming.
    bool PresentTiming{false};
    //! True if the presentAtRelativeTime feature of VK_EXT_present_timing was enabled as well. A present can then be given a target time
    //! relative to the present before it if its surface supports that too, see VUSwapchainPresentTiming::TryEnablePresentAtRelativeTime.
    bool PresentAtRelativeTime{false};
    //! True if VK_KHR_present_mode_fifo_latest_ready (or the EXT version) was enabled with its feature, so a swapchain can use that present
    //! mode if its surface has it
    bool PresentModeFifoLatestReady{false};
  };
}

#endif
