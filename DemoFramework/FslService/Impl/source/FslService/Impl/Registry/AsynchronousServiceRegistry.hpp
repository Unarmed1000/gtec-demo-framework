#ifndef FSLSERVICE_IMPL_REGISTRY_ASYNCHRONOUSSERVICEREGISTRY_HPP
#define FSLSERVICE_IMPL_REGISTRY_ASYNCHRONOUSSERVICEREGISTRY_HPP
/****************************************************************************************************************************************************
 * Copyright 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *
 *    * Neither the name of the NXP. nor the names of
 *      its contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************************************************************/

#include <FslService/Impl/Registry/RegisteredAsynchronousServiceDeque.hpp>
#include <FslService/Impl/Registry/ServiceTypeEx.hpp>
#include <FslService/Impl/ServiceOptionParserDeque.hpp>
#include <memory>

namespace Fsl
{
  class AsynchronousServiceFactory;
  class InterfaceCollisionChecker;
  class Priority;
  class ProviderIdGenerator;

  class AsynchronousServiceRegistry
  {
    std::shared_ptr<ProviderIdGenerator> m_providerIdGenerator;
    std::shared_ptr<InterfaceCollisionChecker> m_interfaceCollisionChecker;
    const ServiceTypeEx TheServiceType;
    bool m_isLocked;
    RegisteredAsynchronousServiceDeque m_services;
    ServiceOptionParserDeque m_serviceOptionParsers;

  public:
    AsynchronousServiceRegistry(std::shared_ptr<ProviderIdGenerator> providerIdGenerator,
                                std::shared_ptr<InterfaceCollisionChecker> interfaceCollisionChecker, const ServiceTypeEx serviceType);
    ~AsynchronousServiceRegistry();

    void LockAndExtractServices(RegisteredAsynchronousServiceDeque& rServices, ServiceOptionParserDeque& rServiceOptionParsers);

    //! @brief Register a service with the given priority
    void Register(const AsynchronousServiceFactory& factory, const Priority& priority);
  };
}

#endif
