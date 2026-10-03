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


#include <FslDemoService/SystemStats/Impl/SystemStatsService.hpp>
#include <FslDemoService/SystemStats/Impl/SystemStatsServiceFactory.hpp>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace Fsl
{
  SystemStatsServiceFactory::SystemStatsServiceFactory(const std::function<std::unique_ptr<ICpuStatsAdapter>()>& fnAllocateCpuAdapter,
                                                       std::function<std::unique_ptr<IGpuStatsAdapter>()> fnAllocateGpuAdapter)
    : ThreadLocalSingletonServiceFactoryBase(std::type_index(typeid(ISystemStatsService)))
    , m_fnAllocateCpuAdapter(fnAllocateCpuAdapter)
    , m_fnAllocateGpuAdapter(std::move(fnAllocateGpuAdapter))
  {
    if (!fnAllocateCpuAdapter)
    {
      throw std::invalid_argument("allocate cpu adapter function must be valid");
    }
  }


  void SystemStatsServiceFactory::FillInterfaceType(ServiceSupportedInterfaceDeque& rServiceInterfaceTypeDeque) const
  {
    rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(ISystemStatsService)));
    rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(ICpuStatsService)));
    rServiceInterfaceTypeDeque.push_back(std::type_index(typeid(ISystemStatsServiceControl)));
  }


  std::shared_ptr<IService> SystemStatsServiceFactory::Allocate(ServiceProvider& provider)
  {
    return std::make_shared<SystemStatsService>(provider, m_fnAllocateCpuAdapter, m_fnAllocateGpuAdapter);
  }
}
