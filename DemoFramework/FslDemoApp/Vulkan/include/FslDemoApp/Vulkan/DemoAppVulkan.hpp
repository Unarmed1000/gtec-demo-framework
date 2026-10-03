#ifndef FSLDEMOAPP_VULKAN_DEMOAPPVULKAN_HPP
#define FSLDEMOAPP_VULKAN_DEMOAPPVULKAN_HPP
/****************************************************************************************************************************************************
 * Copyright (c) 2016 Freescale Semiconductor, Inc.
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

#include <FslDemoApp/Base/ADemoApp.hpp>
#include <FslDemoHost/Vulkan/Config/VulkanHostDeviceFeatures.hpp>
#include <FslDemoHost/Vulkan/Config/VulkanLaunchOptions.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDeviceFault.hpp>
#include <FslUtil/Vulkan1_0/SafeType/DeviceCreateInfoCopy.hpp>
#include <FslUtil/Vulkan1_0/SafeType/InstanceCreateInfoCopy.hpp>
#include <FslUtil/Vulkan1_0/VUCalibratedTimestamps.hpp>
#include <FslUtil/Vulkan1_0/VUDevice.hpp>
#include <FslUtil/Vulkan1_0/VUDeviceQueueRecord.hpp>
#include <FslUtil/Vulkan1_0/VUPhysicalDeviceRecord.hpp>
#include <vulkan/vulkan.h>
#include <memory>

namespace Fsl
{
  class IGraphicsServiceHost;
  class IHostInfo;

  class DemoAppVulkan : public ADemoApp
  {
    std::shared_ptr<IHostInfo> m_hostInfo;

  protected:
    std::shared_ptr<IGraphicsServiceHost> m_graphicsServiceHost;
    VulkanLaunchOptions m_launchOptions;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    Vulkan::VUPhysicalDeviceRecord m_physicalDevice;
    VkPhysicalDeviceFeatures m_deviceActiveFeatures{};
    //! The enabled Vulkan 1.1 core features (request them with DemoAppHostConfigVulkan::AddPhysicalDeviceFeatureRequest)
    VkPhysicalDeviceVulkan11Features m_deviceActiveFeatures11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, nullptr};
    //! The enabled Vulkan 1.2 core features (request them with DemoAppHostConfigVulkan::AddPhysicalDeviceFeatureRequest)
    VkPhysicalDeviceVulkan12Features m_deviceActiveFeatures12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, nullptr};
    //! The enabled Vulkan 1.3 core features (request them with DemoAppHostConfigVulkan::AddPhysicalDeviceFeatureRequest)
    VkPhysicalDeviceVulkan13Features m_deviceActiveFeatures13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, nullptr};
    //! True if VK_KHR/EXT_swapchain_maintenance1 was enabled (which means VkSwapchainPresentFenceInfoKHR can be used)
    bool m_swapchainMaintenance1Enabled{false};
    //! The optional device extensions the host enabled on m_device
    Vulkan::VulkanHostDeviceFeatures m_hostDeviceFeatures;
    Vulkan::VUDevice m_device;
    //! Relates the clock of the device to the clock of the framework (check IsSupported before use, it needs VK_KHR_calibrated_timestamps)
    Vulkan::VUCalibratedTimestamps m_calibratedTimestamps;
    std::shared_ptr<Vulkan::DeviceCreateInfoCopy> m_deviceCreateInfo;

    Vulkan::VUDeviceQueueRecord m_deviceQueue;


    explicit DemoAppVulkan(const DemoAppConfig& demoAppConfig);

  public:
    ~DemoAppVulkan() override;

  protected:
    void OnDestroy() override;


    AppDrawResult TrySwapBuffers(const FrameInfo& frameInfo) override
    {
      FSL_PARAM_NOT_USED(frameInfo);
      return AppDrawResult::Completed;
    }

    // Call this during destruction to ensure the device is idle before you destroy resources
    void SafeWaitForDeviceIdle() noexcept;

    //! @brief Call this with the result of a failed Vulkan call. If it is VK_ERROR_DEVICE_LOST the faults the driver reports are written to
    //!        the log (when a device fault extension is enabled, see m_hostDeviceFeatures). They are only logged once.
    void ReportDeviceLost(const VkResult result) noexcept;

  private:
    //! Queries the driver for why the device was lost
    Vulkan::VUDeviceFault m_deviceFault;
    bool m_deviceLostReported{false};

    void SafeShutdown();
  };
}

#endif
