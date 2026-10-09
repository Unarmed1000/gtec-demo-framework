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

#include <FslBase/Log/IO/FmtPath.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoApp/Base/Service/AppInfo/IAppInfoService.hpp>
#include <FslDemoService/Trace/Impl/TraceFrameTable.hpp>
#include <FslDemoService/Trace/Impl/TraceLog.hpp>
#include <FslDemoService/Trace/Impl/TracePathUtil.hpp>
#include <FslDemoService/Trace/Impl/TraceService.hpp>
#include <FslDemoService/Trace/Impl/TraceServiceOptionParser.hpp>
#include <FslDemoService/Trace/Impl/TraceThreadUtil.hpp>
#include <fmt/format.h>
#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      //! The version of what the trace holds: the names of its tracks, events and arguments (see Doc/Trace.md)
      constexpr uint32_t FormatVersion = 1;
      constexpr std::string_view MainThreadName("Main");
    }

    //! @return the value of a variable of the environment, empty if there is none.
    std::string ReadEnvironmentVariable(const char* const pszVariable)
    {
#ifdef _WIN32
      // The variant of the runtime that copies the value, which is the one it does not warn about
      std::array<char, 1024> buffer{};
      std::size_t length = 0;
      if (getenv_s(&length, buffer.data(), buffer.size(), pszVariable) != 0 || length == 0u)
      {
        return {};
      }
      return {buffer.data()};
#else
      const char* const pszValue = std::getenv(pszVariable);
      return pszValue != nullptr ? std::string(pszValue) : std::string();
#endif
    }

    void AddEnvironmentDirectory(TraceLog& rLog, const char* const pszVariable, const std::string_view replacement)
    {
      const std::string value = ReadEnvironmentVariable(pszVariable);
      if (!value.empty())
      {
        rLog.AddAnonymousDirectory(value, replacement);
      }
    }
  }


  TraceService::TraceService(const ServiceProvider& serviceProvider, const std::shared_ptr<TraceServiceOptionParser>& optionParser,
                             const TraceSinkCreator& sinkCreator)
    : ThreadLocalService(serviceProvider)
    , m_anonymise(optionParser->IsAnonymised())
  {
    const IO::Path& tracePath = optionParser->GetTracePath();
    if (tracePath.IsEmpty())
    {
      return;
    }
    if (!sinkCreator)
    {
      FSLLOG3_WARNING("Trace: nothing on this platform writes a trace, '--Trace' is ignored");
      return;
    }
    try
    {
      // The file is created by the sink, so it is not there yet
      const IO::Path fullPath = TracePathUtil::ToFullPath(tracePath);
      auto sink = sinkCreator(TraceSinkConfig{fullPath, m_anonymise});
      if (!sink)
      {
        // The creator said why
        return;
      }
      m_log = std::make_unique<TraceLog>(std::move(sink), m_anonymise);
      // The thread id is read here, once: every zone is recorded on this thread
      m_log->SetThread(TraceThreadUtil::GetCurrentThreadId(), LocalConfig::MainThreadName);
      if (m_anonymise)
      {
        m_log->AddAnonymousDirectory(IO::Path::GetDirectoryName(fullPath).ToUTF8String(), "<output>");
        AddEnvironmentDirectory(*m_log, "USERPROFILE", "<home>");
        AddEnvironmentDirectory(*m_log, "HOME", "<home>");
        AddEnvironmentDirectory(*m_log, "FSL_GRAPHICS_SDK", "<sdk>");
      }
      WriteFacts();
      FSLLOG3_INFO("Trace: the trace is written to '{}'{}", tracePath, m_anonymise ? " (anonymised)" : "");
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("Trace: the trace is not written: {}", ex.what());
      m_log.reset();
    }
  }


  TraceService::~TraceService()
  {
    if (m_log)
    {
      // The frames that are still open are written and the writer is stopped
      m_log->Close();
    }
  }


  void TraceService::Link(const ServiceProvider& serviceProvider)
  {
    ThreadLocalService::Link(serviceProvider);
    if (m_log)
    {
      m_appInfo = serviceProvider.TryGet<IAppInfoService>();
    }
  }


  bool TraceService::IsEnabled() const noexcept
  {
    return m_log != nullptr;
  }


  bool TraceService::IsAnonymised() const noexcept
  {
    return m_anonymise;
  }


  TraceFrameIndex TraceService::GetFrameIndex() const noexcept
  {
    return TraceFrameIndex(m_log ? m_log->GetFrameIndex() : 0u);
  }


  TraceZone TraceService::RegisterZone(const std::string_view name)
  {
    return m_log ? m_log->RegisterZone(name) : TraceZone();
  }


  void TraceService::BeginZone(const TraceZone zone) noexcept
  {
    if (m_log)
    {
      m_log->BeginZone(zone);
    }
  }


  void TraceService::EndZone() noexcept
  {
    if (m_log)
    {
      m_log->EndZone();
    }
  }


  void TraceService::BeginZoneAt(const TraceZone zone, const TickCount time) noexcept
  {
    if (m_log)
    {
      m_log->BeginZoneAt(zone, time);
    }
  }


  void TraceService::EndZoneAt(const TickCount time) noexcept
  {
    if (m_log)
    {
      m_log->EndZoneAt(time);
    }
  }


  TraceTrack TraceService::RegisterTrack(const std::string_view name, const TraceTrackKind kind)
  {
    return m_log ? m_log->RegisterTrack(name, kind) : TraceTrack();
  }


  TraceValue TraceService::RegisterValue(const std::string_view name, const TraceUnit unit, const std::string_view description)
  {
    return m_log ? m_log->RegisterValue(name, unit, description) : TraceValue();
  }


  TraceValue TraceService::FindValue(const std::string_view name) const noexcept
  {
    return m_log ? m_log->FindValue(name) : TraceValue();
  }


  bool TraceService::DeclareSpan(const std::string_view title, const TraceTrack track, const TraceValue begin, const TraceValue end,
                                 const TraceLink link)
  {
    return m_log ? m_log->DeclareSpan(title, track, begin, end, link) : false;
  }


  bool TraceService::DeclareMark(const std::string_view title, const TraceTrack track, const TraceValue time, const TraceLink link)
  {
    return m_log ? m_log->DeclareMark(title, track, time, link) : false;
  }


  bool TraceService::DeclareCounter(const std::string_view title, const TraceValue value)
  {
    return m_log ? m_log->DeclareCounter(title, value) : false;
  }


  void TraceService::SetInt64(const TraceValue value, const int64_t number) noexcept
  {
    if (m_log)
    {
      m_log->SetInt64(value, number);
    }
  }


  void TraceService::SetUInt64(const TraceValue value, const uint64_t number) noexcept
  {
    if (m_log)
    {
      // A unsigned value is written as its bits
      m_log->SetInt64(value, static_cast<int64_t>(number));
    }
  }


  void TraceService::SetInt64At(const TraceFrameIndex frameIndex, const TraceValue value, const int64_t number) noexcept
  {
    if (m_log)
    {
      m_log->SetInt64At(frameIndex.Value, value, number);
    }
  }


  void TraceService::SetUInt64At(const TraceFrameIndex frameIndex, const TraceValue value, const uint64_t number) noexcept
  {
    if (m_log)
    {
      m_log->SetInt64At(frameIndex.Value, value, static_cast<int64_t>(number));
    }
  }


  void TraceService::SetValue(const TraceValue value, const TickCount time) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(m_log->GetFrameIndex(), value, time);
    }
  }


  void TraceService::SetValue(const TraceValue value, const NanosecondTickCount time) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(m_log->GetFrameIndex(), value, time);
    }
  }


  void TraceService::SetValue(const TraceValue value, const TimeSpan duration) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(m_log->GetFrameIndex(), value, duration);
    }
  }


  void TraceService::SetValue(const TraceValue value, const NanosecondTimeSpan duration) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(m_log->GetFrameIndex(), value, duration);
    }
  }


  void TraceService::SetValue(const TraceValue value, const bool flag) noexcept
  {
    if (m_log)
    {
      m_log->SetFlagAt(m_log->GetFrameIndex(), value, flag);
    }
  }


  void TraceService::SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const TickCount time) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(frameIndex.Value, value, time);
    }
  }


  void TraceService::SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const NanosecondTickCount time) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(frameIndex.Value, value, time);
    }
  }


  void TraceService::SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const TimeSpan duration) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(frameIndex.Value, value, duration);
    }
  }


  void TraceService::SetValueAt(const TraceFrameIndex frameIndex, const TraceValue value, const NanosecondTimeSpan duration) noexcept
  {
    if (m_log)
    {
      m_log->SetValueAt(frameIndex.Value, value, duration);
    }
  }


  void TraceService::AddEvent(const std::string_view name, const std::string_view details)
  {
    if (m_log)
    {
      m_log->AddEvent(name, details);
    }
  }


  void TraceService::SetFact(const std::string_view key, const std::string_view value)
  {
    if (m_log)
    {
      m_log->SetFact(key, value);
    }
  }


  void TraceService::AddAnonymousText(const std::string_view text, const std::string_view replacement)
  {
    if (m_log)
    {
      m_log->AddAnonymousText(text, replacement);
    }
  }


  void TraceService::AddAnonymousFact(const std::string_view key, const std::string_view replacement)
  {
    if (m_log)
    {
      m_log->AddAnonymousFact(key, replacement);
    }
  }


  void TraceService::BeginFrame(const TraceFrameIndex frameIndex, const TraceRunId runId)
  {
    if (!m_log)
    {
      return;
    }
    if (!m_processNameWritten)
    {
      // Not done before the first frame, as the name of the app is not known when the service is created
      m_processNameWritten = true;
      if (m_appInfo)
      {
        m_log->SetProcessName(std::string_view(m_appInfo->GetAppName()));
      }
    }
    m_log->BeginFrame(frameIndex.Value, runId.Value);
  }


  void TraceService::SetFrameBounds(const TraceValue begin, const TraceValue end)
  {
    if (m_log)
    {
      m_log->SetFrameBounds(begin, end);
    }
  }


  void TraceService::WriteFacts()
  {
    // What a reader of the trace has to know about the trace itself. The clock every time is on, and the same moment as a wall clock
    // time so a trace can be related to other recordings.
    const HighResolutionTimer timer;
    m_log->SetFact("trace.formatVersion", fmt::format("{}", LocalConfig::FormatVersion));
    m_log->SetFact("trace.clock", "HighResolutionTimer, 100 nanosecond ticks");
    m_log->SetFact("trace.clockNativeFrequency", fmt::format("{}", timer.GetNativeTickFrequency()));
    {
      const auto utcNow = std::chrono::system_clock::now();
      const TickCount clockNow = timer.GetTimestamp();
      m_log->SetFact("trace.utcNanoseconds",
                     fmt::format("{}", std::chrono::duration_cast<std::chrono::nanoseconds>(utcNow.time_since_epoch()).count()));
      m_log->SetFact("trace.utcClockTicks", fmt::format("{}", clockNow.Ticks()));
    }
    m_log->SetFact("trace.openFrames", fmt::format("{}", TraceFrameTable::OpenRowCount));
    m_log->SetFact("trace.anonymised", m_anonymise ? "1" : "0");
  }
}
