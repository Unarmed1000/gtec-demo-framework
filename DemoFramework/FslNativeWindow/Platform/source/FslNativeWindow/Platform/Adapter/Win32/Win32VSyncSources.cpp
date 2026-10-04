#ifdef _WIN32
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

#include "Win32VSyncSources.hpp"
#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Time/TickCount.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslNativeWindow/Base/NativeWindowVSyncSourceInfo.hpp>
#include <dxgi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cwchar>
#include <thread>
#include <vector>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      constexpr auto NameAuto = "auto";
      //! The one source: IDXGIOutput::WaitForVBlank is the documented way to follow the vertical blank of one display, and it is the
      //! output of the monitor the window is on that is waited for
      constexpr auto NameDxgi = "dxgi";
      //! The call of the platform the source is built on
      constexpr auto ApiNameDxgi = "IDXGIOutput::WaitForVBlank";
      constexpr auto DescriptionDxgi = "IDXGIOutput::WaitForVBlank on a thread";

      //! A wait for a vertical blank that returns sooner than this after the one before did not wait (the display is off): the thread
      //! sleeps then, so it does not spin
      constexpr double MinWaitSeconds = 0.0002;
      constexpr DWORD NoWaitSleepMilliseconds = 2;
    }

    int64_t ReadQpc() noexcept
    {
      LARGE_INTEGER value{};
      QueryPerformanceCounter(&value);
      return value.QuadPart;
    }

    //! The mode of a monitor: its refresh rate as a exact fraction, which is the refresh period that goes with the vertical blank waits
    struct ModeInfo
    {
      bool Valid{false};
      double RefreshHz{0.0};
    };

    //! The exact refresh rate of the mode of a GDI display ("\\.\DISPLAY1")
    ModeInfo TryQueryMode(const wchar_t* const pszGdiDeviceName)
    {
      UINT32 pathCount = 0;
      UINT32 modeCount = 0;
      if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS || pathCount == 0u || modeCount == 0u)
      {
        return {};
      }
      std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
      std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
      if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr) != ERROR_SUCCESS)
      {
        return {};
      }
      for (UINT32 i = 0; i < pathCount; ++i)
      {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = paths[i].sourceInfo.adapterId;
        sourceName.header.id = paths[i].sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS ||
            std::wcscmp(static_cast<const wchar_t*>(sourceName.viewGdiDeviceName), pszGdiDeviceName) != 0)
        {
          continue;
        }
        const UINT32 targetModeIndex = paths[i].targetInfo.modeInfoIdx;
        if (targetModeIndex >= modeCount || modes[targetModeIndex].infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
        {
          continue;
        }
        const DISPLAYCONFIG_VIDEO_SIGNAL_INFO& signal = modes[targetModeIndex].targetMode.targetVideoSignalInfo;
        ModeInfo info;
        info.Valid = signal.vSyncFreq.Numerator != 0u && signal.vSyncFreq.Denominator != 0u;
        if (info.Valid)
        {
          info.RefreshHz = static_cast<double>(signal.vSyncFreq.Numerator) / static_cast<double>(signal.vSyncFreq.Denominator);
        }
        return info;
      }
      return {};
    }

    IDXGIOutput* TryFindDxgiOutput(const HMONITOR hMonitor)
    {
      IDXGIFactory1* pFactory = nullptr;
      if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&pFactory))) || pFactory == nullptr)
      {
        return nullptr;
      }
      IDXGIOutput* pResult = nullptr;
      IDXGIAdapter1* pAdapter = nullptr;
      for (UINT adapterIndex = 0; pResult == nullptr && pFactory->EnumAdapters1(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
      {
        IDXGIOutput* pCandidate = nullptr;
        for (UINT outputIndex = 0; pResult == nullptr && pAdapter->EnumOutputs(outputIndex, &pCandidate) != DXGI_ERROR_NOT_FOUND; ++outputIndex)
        {
          DXGI_OUTPUT_DESC desc{};
          if (SUCCEEDED(pCandidate->GetDesc(&desc)) && desc.Monitor == hMonitor)
          {
            pResult = pCandidate;
          }
          else
          {
            pCandidate->Release();
          }
        }
        pAdapter->Release();
      }
      pFactory->Release();
      return pResult;
    }

    //! A thread that waits for the vertical blank of a monitor again and again and keeps the time the last wait returned
    class VBlankWaitThread
    {
      std::thread m_thread;
      std::atomic<bool> m_stop{false};
      std::atomic<int64_t> m_lastQpc{0};

    public:
      VBlankWaitThread(const VBlankWaitThread&) = delete;
      VBlankWaitThread& operator=(const VBlankWaitThread&) = delete;

      //! Wait on a output (DXGI). The thread releases it when it ends.
      VBlankWaitThread(IDXGIOutput* const pOutput, const int64_t qpcFrequency)
        : m_thread(
            [this, pOutput, qpcFrequency]()
            {
              Run(qpcFrequency, [pOutput]() { return SUCCEEDED(pOutput->WaitForVBlank()); });
              pOutput->Release();
            })
      {
      }

      ~VBlankWaitThread()
      {
        // The wait returns at the next vertical blank, so this takes a refresh at most
        m_stop.store(true);
        if (m_thread.joinable())
        {
          m_thread.join();
        }
      }

      [[nodiscard]] int64_t GetLastQpc() const noexcept
      {
        return m_lastQpc.load();
      }

    private:
      template <typename TWait>
      void Run(const int64_t qpcFrequency, TWait wait)
      {
        const auto minWaitQpc = static_cast<int64_t>(static_cast<double>(qpcFrequency) * LocalConfig::MinWaitSeconds);
        int64_t lastReturn = ReadQpc();
        while (!m_stop.load())
        {
          const bool waited = wait();
          const int64_t now = ReadQpc();
          if (waited && (now - lastReturn) >= minWaitQpc)
          {
            m_lastQpc.store(now);
          }
          else
          {
            // The wait failed or did not wait: the display is off or the output is gone
            Sleep(LocalConfig::NoWaitSleepMilliseconds);
          }
          lastReturn = now;
        }
      }
    };
  }


  struct Win32VSyncSources::State
  {
    std::string Requested;
    int64_t QpcFrequency{0};

    // What is known about the monitor the window is on
    HMONITOR Monitor{nullptr};
    std::array<wchar_t, CCHDEVICENAME> DeviceName{};
    ModeInfo Mode;
    //! True if the monitor has a DXGI output, which is what the vertical blank is waited for on
    bool HasDxgiOutput{false};
    std::unique_ptr<VBlankWaitThread> WaitThread;

    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;

    ~State()
    {
      ReleaseMonitor();
    }

    void ReleaseMonitor() noexcept
    {
      // The thread waits on the output of the monitor, so it ends first
      WaitThread.reset();
      Monitor = nullptr;
      Mode = {};
      HasDxgiOutput = false;
    }

    //! What the monitor of the window can do, read again when the window is on another monitor
    void UpdateMonitor(const HWND hWnd)
    {
      const HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
      if (monitor == Monitor && monitor != nullptr)
      {
        return;
      }
      ReleaseMonitor();
      Monitor = monitor;

      MONITORINFOEXW monitorInfo{};
      monitorInfo.cbSize = sizeof(monitorInfo);
      if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo) != 0)
      {
        std::copy(std::begin(monitorInfo.szDevice), std::end(monitorInfo.szDevice), DeviceName.begin());
        Mode = TryQueryMode(DeviceName.data());

        // The thread releases the output when it ends
        IDXGIOutput* const pOutput = TryFindDxgiOutput(monitor);
        HasDxgiOutput = pOutput != nullptr;
        if (pOutput != nullptr)
        {
          WaitThread = std::make_unique<VBlankWaitThread>(pOutput, QpcFrequency);
        }
      }
      FSLLOG3_VERBOSE("Win32: vsync source '{}'", HasDxgiOutput ? LocalConfig::NameDxgi : "none");
    }

    [[nodiscard]] TickCount ToTickCount(const int64_t qpc) const noexcept
    {
      return TickCount(
        static_cast<int64_t>(static_cast<double>(qpc) * (static_cast<double>(TickCount::TicksPerSecond) / static_cast<double>(QpcFrequency))));
    }

    [[nodiscard]] TimeSpan ToTimeSpan(const double qpcDuration) const noexcept
    {
      return TimeSpan(static_cast<int64_t>(qpcDuration * (static_cast<double>(TickCount::TicksPerSecond) / static_cast<double>(QpcFrequency))));
    }

    //! The refresh period of the mode of the monitor in QueryPerformanceCounter counts (zero if the mode is not known)
    [[nodiscard]] double GetMonitorPeriodQpc() const noexcept
    {
      return Mode.Valid ? (static_cast<double>(QpcFrequency) / Mode.RefreshHz) : 0.0;
    }

    [[nodiscard]] NativeWindowVSyncInfo TryGetWaited() const noexcept
    {
      const int64_t lastQpc = WaitThread ? WaitThread->GetLastQpc() : 0;
      const double periodQpc = GetMonitorPeriodQpc();
      if (lastQpc <= 0 || periodQpc <= 0.0)
      {
        // No vertical blank was waited for yet
        return {};
      }
      return {ToTickCount(lastQpc), ToTimeSpan(periodQpc)};
    }
  };


  Win32VSyncSources::Win32VSyncSources(const std::string& requestedSource)
    : m_state(std::make_unique<State>())
  {
    if (!requestedSource.empty() && requestedSource != LocalConfig::NameAuto && requestedSource != LocalConfig::NameDxgi)
    {
      throw NotSupportedException(
        fmt::format("VSyncSource '{}' is not a vsync source of this window system (auto, {})", requestedSource, LocalConfig::NameDxgi));
    }
    m_state->Requested = requestedSource;
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    m_state->QpcFrequency = frequency.QuadPart;
  }


  Win32VSyncSources::~Win32VSyncSources() = default;


  NativeWindowVSyncInfo Win32VSyncSources::TryGetVSyncInfo(const HWND hWnd)
  {
    if (hWnd == nullptr || m_state->QpcFrequency <= 0)
    {
      return {};
    }
    m_state->UpdateMonitor(hWnd);
    return m_state->TryGetWaited();
  }


  void Win32VSyncSources::OnDisplayChanged() noexcept
  {
    // Forget the monitor: the next call looks it up again with its mode, and selects a source for it
    m_state->ReleaseMonitor();
  }


  void Win32VSyncSources::FillTimingSupport(const HWND hWnd, NativeWindowTimingSupport& rSupport)
  {
    if (hWnd != nullptr)
    {
      m_state->UpdateMonitor(hWnd);
    }
    rSupport.RequestedVSyncSource = m_state->Requested.empty() ? LocalConfig::NameAuto : m_state->Requested;
    // The one source is used wherever the monitor of the window has a DXGI output
    const bool isUsed = m_state->HasDxgiOutput;
    rSupport.VSyncSources.emplace_back(LocalConfig::NameDxgi, isUsed ? NativeWindowVSyncSourceState::Used : NativeWindowVSyncSourceState::NotAvailable,
                                       LocalConfig::DescriptionDxgi);
    // And as what the window system has and what is used of it
    if (isUsed)
    {
      rSupport.Available.emplace_back(LocalConfig::ApiNameDxgi);
      rSupport.Used.emplace_back(LocalConfig::ApiNameDxgi);
      rSupport.VSyncSource = LocalConfig::NameDxgi;
    }
    else
    {
      rSupport.NotAvailable.emplace_back(LocalConfig::ApiNameDxgi);
    }
  }
}
#endif
