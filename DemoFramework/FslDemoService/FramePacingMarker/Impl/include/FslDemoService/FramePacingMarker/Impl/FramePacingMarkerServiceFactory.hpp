#ifndef FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGMARKERSERVICEFACTORY_HPP
#define FSLDEMOSERVICE_FRAMEPACINGMARKER_IMPL_FRAMEPACINGMARKERSERVICEFACTORY_HPP
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

#include <FslDemoService/FramePacingMarker/Impl/FramePacingMarkerService.hpp>
#include <FslDemoService/FramePacingMarker/Impl/FramePacingMarkerServiceOptionParser.hpp>
#include <FslService/Impl/ServiceSupportedInterfaceDeque.hpp>
#include <FslService/Impl/ServiceType/Local/IThreadLocalSingletonServiceFactory.hpp>
#include <memory>

namespace Fsl
{
  class FramePacingMarkerServiceFactory final : public IThreadLocalSingletonServiceFactory
  {
    ServiceCaps::Flags m_flags{ServiceCaps::Default};
    std::shared_ptr<FramePacingMarkerServiceOptionParser> m_optionParser;

  public:
    FramePacingMarkerServiceFactory()
      : m_optionParser(std::make_shared<FramePacingMarkerServiceOptionParser>())
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
      rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(IFramePacingMarkerService)));
      rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(IFramePacingMarkerServiceControl)));
      rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(IFramePacingFrameSource)));
    }


    std::shared_ptr<IService> Allocate(ServiceProvider& provider) final
    {
      return std::make_shared<FramePacingMarkerService>(provider, m_optionParser);
    }
  };
}

#endif
