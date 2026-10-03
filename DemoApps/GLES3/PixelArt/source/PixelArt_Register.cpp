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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslDemoApp/OpenGLES3/Setup/RegisterDemoApp.hpp>
#include <FslUtil/EGL/EGLUtil.hpp>
#include <Shared/PixelArt/Base/PixelArtOptionParser.hpp>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <array>
#include <memory>
#include "PixelArt.hpp"

// The values of https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_gl_colorspace.txt, for EGL headers that do not have them
#ifndef EGL_GL_COLORSPACE
#ifdef EGL_GL_COLORSPACE_KHR
#define EGL_GL_COLORSPACE EGL_GL_COLORSPACE_KHR
#else
#define EGL_GL_COLORSPACE 0x309D
#endif
#endif
#ifndef EGL_GL_COLORSPACE_SRGB_KHR
#define EGL_GL_COLORSPACE_SRGB_KHR 0x3089
#endif

namespace Fsl
{
  namespace
  {
    // Custom EGL config (these will per default overwrite the custom settings. However a exact EGL config can be used)
    const std::array<EGLint, 1> g_eglConfigAttribs = {EGL_NONE};

    const std::array<EGLint, 3> g_eglCreateWindowAttribs = {EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB_KHR, EGL_NONE};

    //! Ask for a sRGB window surface if EGL can create one, the scenes are drawn with linear colors
    const EGLint* GetCreateWindowSurfaceAttribs(const EGLDisplay display, const DemoAppHostCreateWindowSurfaceInfoEGL& /*createInfo*/,
                                                const std::shared_ptr<ITag>& userTag)
    {
      const auto pixelArtTag = std::dynamic_pointer_cast<PixelArtUserTag>(userTag);
      if (!pixelArtTag || !EGLUtil::HasExtension(display, "EGL_KHR_gl_colorspace"))
      {
        FSLLOG3_INFO("EGL_KHR_gl_colorspace not available, the shaders apply the gamma");
        return nullptr;
      }
      pixelArtTag->SrgbFramebuffer = true;
      return g_eglCreateWindowAttribs.data();
    }
  }

  // Configure the demo environment to run this demo app in a OpenGLES3 host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    const auto userTag = std::make_shared<PixelArtUserTag>();

    DemoAppHostConfigEGL config(g_eglConfigAttribs.data());
    config.SetUserTag(userTag);
    config.SetCallbackGetCreateWindowSurfaceAttribs(GetCreateWindowSurfaceAttribs);
    // https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_gl_colorspace.txt
    config.AddExtensionRequest(ExtensionType::EGL, "EGL_KHR_gl_colorspace", ExtensionPrecense::Optional);
    // https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_color_buffer_float.txt (the buffers are RGBA16F render targets)
    config.AddExtensionRequest(ExtensionType::OpenGLES, "GL_EXT_color_buffer_float", ExtensionPrecense::Mandatory);

    CustomDemoAppConfig customDemoAppConfig(userTag);
    customDemoAppConfig.RestartFlags = CustomDemoAppConfigRestartFlags::Never;
    DemoAppRegister::GLES3::Register<PixelArt, PixelArtOptionParser>(rSetup, "GLES3.PixelArt", config, customDemoAppConfig);
  }
}
