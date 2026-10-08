#ifndef FSLDEMOSERVICE_TRACE_IMPL_ITRACESINK_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_ITRACESINK_HPP
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


#include <FslBase/IO/Path.hpp>
#include <FslDemoService/Trace/Impl/TraceRecords.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>

namespace Fsl
{
  //! Where the records of the trace go. It is called from one thread at a time: the writer thread of the trace, or the thread that
  //! closes the trace. Every time it is given is a time of the steady clock of the framework in 100 nanosecond ticks.
  class ITraceSink
  {
  public:
    virtual ~ITraceSink() = default;

    //! @brief The thread the zones are on.
    //! @param threadId the id the system has for the thread.
    virtual void WriteThread(const uint64_t threadId, const std::string_view name) = 0;

    //! @brief The name of the process, once it is known.
    virtual void WriteProcessName(const std::string_view name) = 0;

    //! @brief The name of a zone, before the first record that uses it.
    //! @param zone the number of the zone counted from one.
    virtual void WriteZoneName(const uint32_t zone, const std::string_view name) = 0;

    //! @brief What the rows hold, once, before the first row.
    virtual void WriteSchema(const TraceSchema& schema) = 0;

    virtual void WriteEvent(const TraceEventRecord& event) = 0;

    //! @brief The begins and the ends of the zones of the thread, in the order they happened.
    virtual void WriteZones(const std::span<const TraceZoneRecord> zones) = 0;

    //! @brief A frame that can not change anymore.
    virtual void WriteFrame(const TraceFrameRow& row) = 0;

    //! @brief Hand what was written to the file.
    virtual void Flush() = 0;

    //! @brief Nothing is written after this.
    virtual void Close() = 0;
  };


  struct TraceSinkConfig
  {
    IO::Path Path;
    //! True: nothing that names the machine is written by the sink itself (the command line of the process)
    bool Anonymise{true};
  };

  //! Creates the sink of a platform, null if it could not be created (the reason is logged)
  using TraceSinkCreator = std::function<std::unique_ptr<ITraceSink>(const TraceSinkConfig&)>;
}

#endif
