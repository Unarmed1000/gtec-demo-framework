#ifndef FSLDEMOSERVICE_SYSTEMSTATS_IMPL_SYSTEMSTATSSERVICEFACTORY_HPP
#define FSLDEMOSERVICE_SYSTEMSTATS_IMPL_SYSTEMSTATSSERVICEFACTORY_HPP
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


#include <FslDemoService/CpuStats/Impl/Adapter/ICpuStatsAdapter.hpp>
#include <FslDemoService/SystemStats/Impl/Adapter/IGpuStatsAdapter.hpp>
#include <FslService/Impl/ServiceType/Local/ThreadLocalSingletonServiceFactoryBase.hpp>
#include <functional>
#include <memory>

namespace Fsl
{
  //! Creates the SystemStatsService, which is registered as ISystemStatsService, as ICpuStatsService (so the apps that use the CPU stats
  //! service get the same object) and as ISystemStatsServiceControl.
  class SystemStatsServiceFactory final : public ThreadLocalSingletonServiceFactoryBase
  {
    std::function<std::unique_ptr<ICpuStatsAdapter>()> m_fnAllocateCpuAdapter;
    std::function<std::unique_ptr<IGpuStatsAdapter>()> m_fnAllocateGpuAdapter;

  public:
    //! @param fnAllocateCpuAdapter creates the adapter for the CPU and RAM usage (required).
    //! @param fnAllocateGpuAdapter creates the adapter for the GPU usage (leave it empty if the platform has none).
    explicit SystemStatsServiceFactory(const std::function<std::unique_ptr<ICpuStatsAdapter>()>& fnAllocateCpuAdapter,
                                       std::function<std::unique_ptr<IGpuStatsAdapter>()> fnAllocateGpuAdapter = {});

    void FillInterfaceType(ServiceSupportedInterfaceDeque& rServiceInterfaceTypeDeque) const final;
    std::shared_ptr<IService> Allocate(ServiceProvider& provider) final;
  };
}

#endif
