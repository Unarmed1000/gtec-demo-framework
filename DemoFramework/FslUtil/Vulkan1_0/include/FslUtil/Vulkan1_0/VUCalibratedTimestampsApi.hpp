#ifndef FSLUTIL_VULKAN1_0_VUCALIBRATEDTIMESTAMPSAPI_HPP
#define FSLUTIL_VULKAN1_0_VUCALIBRATEDTIMESTAMPSAPI_HPP
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

namespace Fsl::Vulkan
{
  //! The calibrated timestamps extension that is enabled on a device. VK_KHR_calibrated_timestamps is VK_EXT_calibrated_timestamps
  //! under a new name: the same structures and values, and the same entry points with KHR in place of EXT in their names.
  //! A device has the entry points of the extension it was created with. The ones of the other name are not asked for: a driver need
  //! not know them, and what it hands out for a name of a extension that is not enabled can not be relied on.
  enum class VUCalibratedTimestampsApi
  {
    //! No calibrated timestamps extension is enabled
    Disabled,
    //! VK_EXT_calibrated_timestamps: vkGetCalibratedTimestampsEXT and vkGetPhysicalDeviceCalibrateableTimeDomainsEXT
    Ext,
    //! VK_KHR_calibrated_timestamps: vkGetCalibratedTimestampsKHR and vkGetPhysicalDeviceCalibrateableTimeDomainsKHR
    Khr
  };
}

#endif
