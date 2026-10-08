#ifndef FSLDEMOSERVICE_TRACE_IMPL_TRACESERVICEFACTORY_HPP
#define FSLDEMOSERVICE_TRACE_IMPL_TRACESERVICEFACTORY_HPP
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


#include <FslDemoService/Trace/Impl/ITraceSink.hpp>
#include <FslDemoService/Trace/Impl/TraceService.hpp>
#include <FslDemoService/Trace/Impl/TraceServiceOptionParser.hpp>
#include <FslService/Impl/ServiceSupportedInterfaceDeque.hpp>
#include <FslService/Impl/ServiceType/Local/IThreadLocalSingletonServiceFactory.hpp>
#include <memory>
#include <utility>

namespace Fsl
{
  class TraceServiceFactory final : public IThreadLocalSingletonServiceFactory
  {
    ServiceCaps::Flags m_flags{ServiceCaps::Default};
    std::shared_ptr<TraceServiceOptionParser> m_optionParser;
    TraceSinkCreator m_sinkCreator;

  public:
    //! @param sinkCreator creates what writes the trace file of the platform. Empty on a platform that has nothing that does: the
    //!        service is there and records nothing.
    explicit TraceServiceFactory(TraceSinkCreator sinkCreator = {})
      : m_optionParser(std::make_shared<TraceServiceOptionParser>())
      , m_sinkCreator(std::move(sinkCreator))
    {
    }


    [[nodiscard]] std::shared_ptr<AServiceOptionParser> GetOptionParser() const final
    {
      return m_optionParser;
    }


    [[nodiscard]] ServiceCaps::Flags GetFlags() const final
    {
      return m_flags;
    }


    void FillInterfaceType(ServiceSupportedInterfaceDeque& rServiceInterfaceTypeDeque) const final
    {
      rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(ITraceService)));
      rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(ITraceServiceControl)));
    }


    std::shared_ptr<IService> Allocate(ServiceProvider& provider) final
    {
      return std::make_shared<TraceService>(provider, m_optionParser, m_sinkCreator);
    }
  };
}

#endif
