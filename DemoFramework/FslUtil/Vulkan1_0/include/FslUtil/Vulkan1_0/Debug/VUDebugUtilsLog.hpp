#ifndef FSLUTIL_VULKAN1_0_DEBUG_VUDEBUGUTILSLOG_HPP
#define FSLUTIL_VULKAN1_0_DEBUG_VUDEBUGUTILSLOG_HPP
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
#include <FslBase/Log/Logger0.hpp>
#include <FslUtil/Vulkan1_0/Common.hpp>
#include <vulkan/vulkan.h>
#include <string>

//! Sends the messages of a VK_EXT_debug_utils messenger (validation layer, loader and driver) to the framework log.
namespace Fsl::Vulkan::VUDebugUtilsLog
{
  //! @brief Build the create info of a messenger that writes its messages to the framework log.
  //!        It can be used for VUDebugUtilsMessenger and as the pNext of VkInstanceCreateInfo (which covers the creation and destruction of the
  //!        instance).
  //! @param logLevel errors and warnings are always requested, the info and verbose messages are only requested at the higher verbosity levels.
  //! @param forceInfo request the info messages no matter what the logLevel is (the output of debugPrintf is a info message).
  VkDebugUtilsMessengerCreateInfoEXT BuildCreateInfo(const LogType logLevel, const bool forceInfo = false) noexcept;

  //! @brief Convert the message to the text that is written to the log
  //! @note  This includes the names given to the objects and the labels that were active when the message was sent.
  std::string ToLogString(const VkDebugUtilsMessageTypeFlagsEXT messageTypes, const VkDebugUtilsMessengerCallbackDataEXT& callbackData);
}

#endif
