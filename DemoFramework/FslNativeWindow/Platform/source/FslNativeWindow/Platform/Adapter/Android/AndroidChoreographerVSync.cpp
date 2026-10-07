#ifdef __ANDROID__
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

#include "AndroidChoreographerVSync.hpp"
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/NanosecondTickCount.hpp>
#include <FslBase/Time/NanosecondTimeSpan.hpp>
#include <android/api-level.h>
#include <dlfcn.h>
#include <algorithm>

// The types of the choreographer. They are only used through pointers, and the functions are looked up at run time (see the class), so
// nothing here depends on what the header of the NDK declares for the minimum API level of the app.
struct AChoreographer;
struct AChoreographerFrameCallbackData;

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr auto LibraryName = "libandroid.so";
      //! The API level that has the vsync callback and its frame timelines
      constexpr int MinApiLevel = 33;
    }

    using VsyncCallback = void (*)(const AChoreographerFrameCallbackData* pCallbackData, void* pData);
    using RefreshRateCallback = void (*)(int64_t vsyncPeriodNanos, void* pData);

    //! The functions of the choreographer, looked up in libandroid
    struct ChoreographerApi
    {
      AChoreographer* (*GetInstance)(){nullptr};
      void (*PostVsyncCallback)(AChoreographer* pChoreographer, VsyncCallback callback, void* pData){nullptr};
      void (*RegisterRefreshRateCallback)(AChoreographer* pChoreographer, RefreshRateCallback callback, void* pData){nullptr};
      void (*UnregisterRefreshRateCallback)(AChoreographer* pChoreographer, RefreshRateCallback callback, void* pData){nullptr};
      int64_t (*GetFrameTimeNanos)(const AChoreographerFrameCallbackData* pCallbackData){nullptr};
      size_t (*GetFrameTimelinesLength)(const AChoreographerFrameCallbackData* pCallbackData){nullptr};
      size_t (*GetPreferredFrameTimelineIndex)(const AChoreographerFrameCallbackData* pCallbackData){nullptr};
      int64_t (*GetFrameTimelineExpectedPresentationTimeNanos)(const AChoreographerFrameCallbackData* pCallbackData, size_t index){nullptr};

      [[nodiscard]] bool IsComplete() const noexcept
      {
        return GetInstance != nullptr && PostVsyncCallback != nullptr && RegisterRefreshRateCallback != nullptr &&
               UnregisterRefreshRateCallback != nullptr && GetFrameTimeNanos != nullptr && GetFrameTimelinesLength != nullptr &&
               GetPreferredFrameTimelineIndex != nullptr && GetFrameTimelineExpectedPresentationTimeNanos != nullptr;
      }
    };

    template <typename TFunction>
    void Lookup(void* const pLibrary, const char* const pszName, TFunction& rFunction) noexcept
    {
      rFunction = reinterpret_cast<TFunction>(dlsym(pLibrary, pszName));
    }

    ChoreographerApi LookupApi(void* const pLibrary) noexcept
    {
      ChoreographerApi api;
      Lookup(pLibrary, "AChoreographer_getInstance", api.GetInstance);
      Lookup(pLibrary, "AChoreographer_postVsyncCallback", api.PostVsyncCallback);
      Lookup(pLibrary, "AChoreographer_registerRefreshRateCallback", api.RegisterRefreshRateCallback);
      Lookup(pLibrary, "AChoreographer_unregisterRefreshRateCallback", api.UnregisterRefreshRateCallback);
      Lookup(pLibrary, "AChoreographerFrameCallbackData_getFrameTimeNanos", api.GetFrameTimeNanos);
      Lookup(pLibrary, "AChoreographerFrameCallbackData_getFrameTimelinesLength", api.GetFrameTimelinesLength);
      Lookup(pLibrary, "AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex", api.GetPreferredFrameTimelineIndex);
      Lookup(pLibrary, "AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos",
             api.GetFrameTimelineExpectedPresentationTimeNanos);
      return api;
    }
  }


  struct AndroidChoreographerVSync::State
  {
    ChoreographerApi Api;
    AChoreographer* Choreographer{nullptr};
    //! False once the object that made the state is gone: a vsync callback that runs then does nothing
    bool IsAlive{true};
    bool IsVsyncCallbackPending{false};
    //! When the frame of the last vsync callback started being rendered (CLOCK_MONOTONIC, nanoseconds, zero: no callback yet)
    int64_t FrameTimeNs{0};
    //! When that frame is expected to be presented on the timeline the platform prefers
    int64_t ExpectedPresentationTimeNs{0};
    //! The vsync period of the refresh rate callback (zero: not called yet)
    int64_t VsyncPeriodNs{0};

    //! The data of a posted vsync callback: a owner of the state, so the state lives until the callback has run
    using CallbackOwner = std::shared_ptr<State>;

    static void OnVsync(const AChoreographerFrameCallbackData* const pCallbackData, void* const pData)
    {
      const std::unique_ptr<CallbackOwner> owner(static_cast<CallbackOwner*>(pData));
      State& rState = **owner;
      rState.IsVsyncCallbackPending = false;
      if (!rState.IsAlive || pCallbackData == nullptr)
      {
        return;
      }
      const ChoreographerApi& api = rState.Api;
      const size_t timelineCount = api.GetFrameTimelinesLength(pCallbackData);
      if (timelineCount == 0u)
      {
        return;
      }
      const size_t timelineIndex = std::min(api.GetPreferredFrameTimelineIndex(pCallbackData), timelineCount - 1u);
      rState.FrameTimeNs = api.GetFrameTimeNanos(pCallbackData);
      rState.ExpectedPresentationTimeNs = api.GetFrameTimelineExpectedPresentationTimeNanos(pCallbackData, timelineIndex);
    }

    static void OnRefreshRate(const int64_t vsyncPeriodNanos, void* const pData)
    {
      // Registered with the state itself and unregistered before the state is released, so the pointer is good
      auto* const pState = static_cast<State*>(pData);
      if (vsyncPeriodNanos > 0 && vsyncPeriodNanos != pState->VsyncPeriodNs)
      {
        FSLLOG3_VERBOSE("Android: the vsync period is {:.3f} ms", static_cast<double>(vsyncPeriodNanos) / 1000000.0);
        pState->VsyncPeriodNs = vsyncPeriodNanos;
      }
    }
  };


  AndroidChoreographerVSync::AndroidChoreographerVSync()
  {
    const int apiLevel = android_get_device_api_level();
    if (apiLevel < LocalConfig::MinApiLevel)
    {
      m_unavailableReason = fmt::format("the device has API level {}, the vsync callback of the choreographer needs {}", apiLevel, LocalConfig::MinApiLevel);
      return;
    }
    // libandroid is part of every app process, this only gives a handle to look the functions up with. It is not closed.
    void* const pLibrary = dlopen(LocalConfig::LibraryName, RTLD_NOW | RTLD_LOCAL);
    if (pLibrary == nullptr)
    {
      m_unavailableReason = fmt::format("{} could not be opened", LocalConfig::LibraryName);
      return;
    }
    auto state = std::make_shared<State>();
    state->Api = LookupApi(pLibrary);
    if (!state->Api.IsComplete())
    {
      m_unavailableReason = fmt::format("{} does not have the functions of the vsync callback", LocalConfig::LibraryName);
      return;
    }
    state->Choreographer = state->Api.GetInstance();
    if (state->Choreographer == nullptr)
    {
      m_unavailableReason = "the thread has no choreographer (it has no looper)";
      return;
    }
    // The platform calls it once after it was registered, and again when the refresh rate changes
    state->Api.RegisterRefreshRateCallback(state->Choreographer, State::OnRefreshRate, state.get());
    m_state = std::move(state);
    m_isAvailable = true;
    FSLLOG3_VERBOSE("Android: vsync source 'choreographer' (API level {})", apiLevel);
  }


  AndroidChoreographerVSync::~AndroidChoreographerVSync()
  {
    if (m_state)
    {
      // After this the refresh rate callback is not run with the state anymore. A vsync callback that was posted can still run: it
      // owns the state and finds it marked as dead.
      m_state->Api.UnregisterRefreshRateCallback(m_state->Choreographer, State::OnRefreshRate, m_state.get());
      m_state->IsAlive = false;
    }
  }


  uint32_t AndroidChoreographerVSync::GetDeviceApiLevel() noexcept
  {
    return static_cast<uint32_t>(std::max(android_get_device_api_level(), 0));
  }


  void AndroidChoreographerVSync::RequestVSyncTime()
  {
    if (!m_isAvailable || m_state->IsVsyncCallbackPending)
    {
      return;
    }
    // The callback takes the owner over and deletes it
    auto owner = std::make_unique<State::CallbackOwner>(m_state);
    m_state->IsVsyncCallbackPending = true;
    m_state->Api.PostVsyncCallback(m_state->Choreographer, State::OnVsync, owner.release());
  }


  NativeWindowVSyncInfo AndroidChoreographerVSync::GetVSyncInfo() const noexcept
  {
    if (!m_isAvailable)
    {
      return {};
    }
    const State& state = *m_state;
    if (state.ExpectedPresentationTimeNs <= 0 || state.VsyncPeriodNs <= 0)
    {
      return {};
    }
    // The expected presentation time is a vertical blank that is still to come. The vertical blanks are a vsync period apart, so the
    // one that is reported is the last one that is not after the time the frame of the callback started: a recent vertical blank.
    int64_t vsyncTimeNs = state.ExpectedPresentationTimeNs;
    if (vsyncTimeNs > state.FrameTimeNs)
    {
      const int64_t periodsAhead = ((vsyncTimeNs - state.FrameTimeNs) + (state.VsyncPeriodNs - 1)) / state.VsyncPeriodNs;
      vsyncTimeNs -= periodsAhead * state.VsyncPeriodNs;
    }
    // The system gives both in nanoseconds, and they are kept as that
    return {NanosecondTickCount(vsyncTimeNs), NanosecondTimeSpan(state.VsyncPeriodNs)};
  }
}

#endif
