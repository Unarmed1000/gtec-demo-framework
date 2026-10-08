#ifndef FSLDEMOSERVICE_TRACE_SCOPEDTRACEZONE_HPP
#define FSLDEMOSERVICE_TRACE_SCOPEDTRACEZONE_HPP
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


#include <FslDemoService/Trace/ITraceService.hpp>

namespace Fsl
{
  //! A zone of the trace that lasts as long as this object: it begins where the object is made and ends where it goes out of scope.
  //! It does nothing if there is no trace service.
  class ScopedTraceZone final
  {
    ITraceService* m_pService;

  public:
    ScopedTraceZone(const ScopedTraceZone&) = delete;
    ScopedTraceZone& operator=(const ScopedTraceZone&) = delete;
    ScopedTraceZone(ScopedTraceZone&&) = delete;
    ScopedTraceZone& operator=(ScopedTraceZone&&) = delete;

    ScopedTraceZone(ITraceService* const pService, const TraceZone zone) noexcept
      : m_pService(zone.IsValid() ? pService : nullptr)
    {
      if (m_pService != nullptr)
      {
        m_pService->BeginZone(zone);
      }
    }

    ~ScopedTraceZone() noexcept
    {
      if (m_pService != nullptr)
      {
        m_pService->EndZone();
      }
    }
  };
}

#endif
