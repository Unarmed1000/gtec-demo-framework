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

#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Span/SpanUtil_Array.hpp>
#include <FslBase/Span/SpanUtil_Vector.hpp>
#include <FslDemoApp/Base/Service/Host/IHostInfo.hpp>
#include <FslDemoApp/Vulkan/DemoAppVulkan.hpp>
#include <FslDemoHost/Vulkan/Config/DemoAppHostConfigVulkan.hpp>
#include <FslDemoHost/Vulkan/Config/HostDeviceExtensions.hpp>
#include <FslDemoHost/Vulkan/Config/PhysicalDeviceConfigUtil.hpp>
#include <FslDemoHost/Vulkan/Config/PhysicalDeviceFeatureRequest.hpp>
#include <FslDemoHost/Vulkan/Config/PhysicalDeviceFeatureRequestUtil.hpp>
#include <FslDemoHost/Vulkan/Config/PhysicalDeviceFeatureUtil.hpp>
#include <FslDemoHost/Vulkan/Config/Service/IVulkanHostInfo.hpp>
#include <FslDemoHost/Vulkan/Config/SwapchainMaintenance1Util.hpp>
#include <FslDemoHost/Vulkan/Config/VulkanDeviceSetupUtil.hpp>
#include <FslDemoHost/Vulkan/Config/VulkanValidationUtil.hpp>
#include <FslDemoService/Graphics/Control/GraphicsDeviceCreateInfo.hpp>
#include <FslDemoService/Graphics/Control/IGraphicsServiceHost.hpp>
#include <FslDemoService/NativeGraphics/Vulkan/NativeGraphicsCustomVulkanDeviceCreateInfo.hpp>
#include <FslDemoService/NativeGraphics/Vulkan/NativeGraphicsService.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <FslUtil/Vulkan1_0/Util/DeviceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/MemoryBudgetUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceKHRUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/QueueUtil.hpp>
#include <algorithm>
#include <array>
#include <vector>

namespace Fsl
{
  namespace
  {
    void LogDeviceExtensions(const VkPhysicalDevice device)
    {
      const auto extensionProperties = Vulkan::PhysicalDeviceUtil::EnumerateDeviceExtensionProperties(device);
      FSLLOG3_INFO("Device extensions: {}", extensionProperties.size());
      for (const auto& extension : extensionProperties)
      {
        FSLLOG3_INFO("- Extension: '{}' specVersion: {}", extension.extensionName, extension.specVersion);
      }
    }

    constexpr double ToMiB(const VkDeviceSize bytes) noexcept
    {
      return static_cast<double>(bytes) / (1024.0 * 1024.0);
    }

    //! Log the memory heaps of the physical device, with what this process can use and does use of them when VK_EXT_memory_budget is supported.
    void LogMemoryHeaps(const Vulkan::VUPhysicalDeviceRecord& physicalDevice)
    {
      Vulkan::VUMemoryBudget budget;
      const bool hasBudget = Vulkan::MemoryBudgetUtil::TryGetMemoryBudget(physicalDevice.Device, budget);

      const uint32_t heapCount = physicalDevice.MemoryProperties.memoryHeapCount;
      FSLLOG3_INFO("Vulkan memory heaps: {}", heapCount);
      for (uint32_t i = 0; i < heapCount; ++i)
      {
        const VkMemoryHeap& heap = physicalDevice.MemoryProperties.memoryHeaps[i];
        const char* const pszType = (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0u ? "device local" : "host";
        if (hasBudget && i < budget.HeapCount)
        {
          FSLLOG3_INFO("- heap #{} ({}): size: {:.0f} MiB budget: {:.0f} MiB usage: {:.1f} MiB", i, pszType, ToMiB(heap.size),
                       ToMiB(budget.HeapBudget[i]), ToMiB(budget.HeapUsage[i]));
        }
        else
        {
          FSLLOG3_INFO("- heap #{} ({}): size: {:.0f} MiB", i, pszType, ToMiB(heap.size));
        }
      }
    }
  }

  DemoAppVulkan::DemoAppVulkan(const DemoAppConfig& demoAppConfig)
    : ADemoApp(demoAppConfig)
    , m_hostInfo(demoAppConfig.DemoServiceProvider.Get<IHostInfo>())
    , m_graphicsServiceHost(demoAppConfig.DemoServiceProvider.Get<IGraphicsServiceHost>())
  {
    // FIX: move most of this init code to the Vulkan demo host
    const auto appHostConfigBase = m_hostInfo->TryGetAppHostConfig();
    if (!appHostConfigBase)
    {
      throw std::runtime_error("The AppHostConfig was not set");
    }
    const auto appHostConfig = std::dynamic_pointer_cast<DemoAppHostConfigVulkan>(appHostConfigBase);
    if (!appHostConfig)
    {
      throw std::runtime_error("The AppHostConfig was not of the expected type");
    }

    const auto vulkanHostInfo = demoAppConfig.DemoServiceProvider.Get<IVulkanHostInfo>();
    // Retrieve the launch options
    m_launchOptions = vulkanHostInfo->GetLaunchOptions();

    m_surface = vulkanHostInfo->GetSurfaceKHR();

    std::deque<Vulkan::PhysicalDeviceFeatureRequest> requiredFeatures;
    if (appHostConfig && appHostConfig->HasDeviceRequiredFeatures())
    {
      appHostConfig->ExtractDeviceRequiredFeatures(requiredFeatures);
    }
    if (m_launchOptions.TimelineSemaphore != OptionUserChoice::Off &&
        std::none_of(requiredFeatures.begin(), requiredFeatures.end(), [](const Vulkan::PhysicalDeviceFeatureRequest& entry)
                     { return entry.Feature == Vulkan::PhysicalDeviceFeature::TimelineSemaphore; }))
    {
      // The frame loop of DemoAppVulkanBasic waits for its frames with a timeline semaphore where the device has them
      requiredFeatures.emplace_back(Vulkan::PhysicalDeviceFeature::TimelineSemaphore, Vulkan::FeatureRequirement::Optional);
    }

    m_physicalDevice = vulkanHostInfo->GetPhysicalDevice();

    if (m_launchOptions.LogDeviceExtensions)
    {
      LogDeviceExtensions(m_physicalDevice.Device);
    }

    {
      std::vector<Vulkan::FeatureRequest> hostExtensions = {
        Vulkan::FeatureRequest(VK_KHR_SWAPCHAIN_EXTENSION_NAME, Vulkan::FeatureRequirement::Mandatory)};
#ifdef VK_EXT_memory_budget
      // Used to report how much GPU memory the app uses (see MemoryBudgetUtil)
      hostExtensions.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, Vulkan::FeatureRequirement::Optional);
#endif

      // Enable swapchain present fences if supported
      VkBaseInStructure* pExtraDeviceCreateInfoNext = nullptr;
#ifdef FSL_VULKAN_SWAPCHAIN_MAINTENANCE1_SUPPORTED
      Vulkan::SwapchainMaintenance1Util::PhysicalDeviceSwapchainMaintenance1Features swapchainMaintenance1Features{};
      if (m_launchOptions.SwapchainMaintenance1 == OptionUserChoice::Off)
      {
        FSLLOG3_INFO("Swapchain maintenance1: disabled by user");
      }
      else
      {
        const char* const pszSwapchainMaintenance1 =
          Vulkan::SwapchainMaintenance1Util::TryGetDeviceExtensionName(vulkanHostInfo->GetInstance(), m_physicalDevice.Device);
        FSLLOG3_WARNING_IF(pszSwapchainMaintenance1 == nullptr && m_launchOptions.SwapchainMaintenance1 == OptionUserChoice::On,
                           "Swapchain maintenance1 was requested but is unsupported");
        if (pszSwapchainMaintenance1 != nullptr)
        {
          hostExtensions.emplace_back(pszSwapchainMaintenance1, Vulkan::FeatureRequirement::Mandatory);
          swapchainMaintenance1Features.sType = Vulkan::SwapchainMaintenance1Util::PhysicalDeviceSwapchainMaintenance1FeaturesSType;
          swapchainMaintenance1Features.swapchainMaintenance1 = VK_TRUE;
          pExtraDeviceCreateInfoNext = reinterpret_cast<VkBaseInStructure*>(&swapchainMaintenance1Features);
          m_swapchainMaintenance1Enabled = true;
        }
        FSLLOG3_VERBOSE("Swapchain maintenance1: {}", pszSwapchainMaintenance1 != nullptr ? pszSwapchainMaintenance1 : "unsupported");
      }
#endif

      // The other optional device extensions the host uses. Their feature structs go in front of the chain and are owned by hostDeviceExtensions,
      // which therefore has to live until the device has been created.
      Vulkan::HostDeviceExtensions hostDeviceExtensions(m_physicalDevice.Device, hostExtensions, m_launchOptions.PresentTiming);
      pExtraDeviceCreateInfoNext = hostDeviceExtensions.LinkDeviceCreateInfoChain(pExtraDeviceCreateInfoNext);

      const auto deviceConfig =
        PhysicalDeviceConfigUtil::BuildConfig(m_physicalDevice.Device, appHostConfig, SpanUtil::AsReadOnlySpan(hostExtensions));
      const PhysicalDeviceConfigUtil::DeviceConfigAsCharArrays deviceConfigEx(deviceConfig);
      const ReadOnlySpan<const char*> extensions = SpanUtil::AsReadOnlySpan(deviceConfigEx.Extensions);

      auto vulkanDeviceSetup =
        Vulkan::VulkanDeviceSetupUtil::CreateSetup(vulkanHostInfo->GetInstance(), m_physicalDevice, m_surface, requiredFeatures, extensions,
                                                   appHostConfig->TryGetDeviceCreationCustomizer().get(), pExtraDeviceCreateInfoNext);
      m_deviceActiveFeatures = vulkanDeviceSetup.DeviceFeatures.Features;
      m_deviceActiveFeatures11 = vulkanDeviceSetup.DeviceFeatures.Features11;
      m_deviceActiveFeatures12 = vulkanDeviceSetup.DeviceFeatures.Features12;
      m_deviceActiveFeatures13 = vulkanDeviceSetup.DeviceFeatures.Features13;
      m_device = std::move(vulkanDeviceSetup.Device);
      m_deviceCreateInfo = vulkanDeviceSetup.DeviceCreateInfo;
      m_deviceQueue = vulkanDeviceSetup.DeviceQueueRecord;
      m_hostDeviceFeatures = hostDeviceExtensions.GetFeatures();
      m_deviceFault = Vulkan::VUDeviceFault(m_device.Get(), m_hostDeviceFeatures.DeviceFault);
      if (m_hostDeviceFeatures.CalibratedTimestamps)
      {
        m_calibratedTimestamps = Vulkan::VUCalibratedTimestamps(vulkanHostInfo->GetInstance(), m_physicalDevice.Device, m_device.Get());
      }

      Vulkan::VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_DEVICE, m_device.Get(), "MainDevice");
      Vulkan::VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_QUEUE, m_deviceQueue.Queue, "MainQueue");
    }

    if (Fsl::LogConfig::GetLogLevel() >= LogType::Verbose2)
    {
      LogMemoryHeaps(m_physicalDevice);
    }

    Vulkan::VulkanValidationUtil::CheckWindowAndSurfaceExtent(m_physicalDevice.Device, m_surface, GetScreenExtent());

    // We do this last to ensure that we dont have to call VulkanDeviceShutdown as nothing else can go wrong
    if (m_graphicsServiceHost)
    {
      Vulkan::NativeGraphicsCustomVulkanDeviceCreateInfo vulkanCreateInfo(m_device, m_deviceQueue.Queue, m_deviceQueue.QueueFamilyIndex);

      const bool preloadBasic2D = m_hostInfo->GetConfig().PreloadBasic2D;
      const GraphicsDeviceCreateInfo createInfo(GetRenderConfig().MaxFramesInFlight, preloadBasic2D, &vulkanCreateInfo);
      m_graphicsServiceHost->CreateDevice(createInfo);
    }
  }

  DemoAppVulkan::~DemoAppVulkan()
  {
    SafeShutdown();
  }

  void DemoAppVulkan::OnDestroy()
  {
    SafeShutdown();
    // Finally call the OnDestroy of our inherited object
    ADemoApp::OnDestroy();
  }


  void DemoAppVulkan::SafeWaitForDeviceIdle() noexcept
  {
    try
    {
      m_device.DeviceWaitIdle();
    }
    catch (const std::exception& ex)
    {
      // We log and swallow it since destructor's are not allowed to throw
      FSLLOG3_ERROR("DeviceWaitIdle, threw exception: {}", ex.what());
    }
  }


  void DemoAppVulkan::ReportDeviceLost(const VkResult result) noexcept
  {
    if (result != VK_ERROR_DEVICE_LOST || m_deviceLostReported)
    {
      return;
    }
    // One lost device makes many calls fail, so the faults are only reported for the first of them
    m_deviceLostReported = true;
    if (!m_deviceFault.IsSupported())
    {
      FSLLOG3_ERROR("The Vulkan device was lost (no device fault extension is available, so the driver can not be asked why)");
      return;
    }
    FSLLOG3_ERROR("The Vulkan device was lost, asking the driver why");
    // The number of faults is for a caller that wants it, the faults are in the log
    static_cast<void>(m_deviceFault.LogFaults());
  }

  void DemoAppVulkan::SafeShutdown()
  {
    SafeWaitForDeviceIdle();
    if (m_graphicsServiceHost)
    {
      m_graphicsServiceHost->DestroyDevice();
      m_graphicsServiceHost.reset();
    }
  }
}
