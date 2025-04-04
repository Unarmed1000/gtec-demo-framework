#ifndef FSLSIMPLEUI_BASE_SYSTEM_MODULES_MODULECALLBACKREGISTRY_HPP
#define FSLSIMPLEUI_BASE_SYSTEM_MODULES_MODULECALLBACKREGISTRY_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2015 Freescale Semiconductor, Inc.
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
 *    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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

#include <deque>
#include <memory>

namespace Fsl::UI
{
  class IModuleCallbackReceiver;
  class TreeNode;

  class ModuleCallbackRegistry
  {
    std::deque<std::weak_ptr<IModuleCallbackReceiver>> m_receivers;

  public:
    ModuleCallbackRegistry();
    ~ModuleCallbackRegistry();

    //! @brief Add a callback receiver, its up to you to ensure you dont add one twice (or you will get two callbacks)
    void AddCallbackReceiver(const std::weak_ptr<IModuleCallbackReceiver>& module);
    void RemoveCallbackReceiver(const std::weak_ptr<IModuleCallbackReceiver>& module);


    //! @brief Called when a TreeNode is addded
    //! @note  This should only be called by the UITree!!
    void ModuleOnTreeNodeAdd(const std::shared_ptr<TreeNode>& node);

    //! @brief Called when a TreeNode is disposed (this call occurs right before we dispose it)
    //! @note  This should only be called by the UITree!!
    void ModuleOnTreeNodeDispose(const std::shared_ptr<TreeNode>& node);
  };
}

#endif
