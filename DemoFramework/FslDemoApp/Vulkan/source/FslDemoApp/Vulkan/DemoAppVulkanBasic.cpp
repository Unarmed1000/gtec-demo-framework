/****************************************************************************************************************************************************
 * Copyright 2018, 2022, 2024 NXP
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

#include <FslBase/Bits/BitsUtil.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Span/SpanUtil_Vector.hpp>
#include <FslBase/Time/TimeSpan.hpp>
#include <FslDemoApp/Base/FrameInfo.hpp>
#include <FslDemoApp/Base/Overlay/DemoAppProfilerOverlay.hpp>
#include <FslDemoApp/Base/Service/Host/IHostInfo.hpp>
#include <FslDemoApp/Vulkan/Basic/DemoAppVulkanBasic.hpp>
#include <FslDemoHost/Vulkan/Config/DemoAppHostConfigVulkan.hpp>
#include <FslDemoHost/Vulkan/Config/SwapchainMaintenance1Util.hpp>
#include <FslDemoService/FramePacingMarker/Control/IFramePacingMarkerServiceControl.hpp>
#include <FslDemoService/FramePacingMarker/Control/IFramePacingOverlay.hpp>
#include <FslDemoService/FramePacingMarker/IFramePacingFrameLog.hpp>
#include <FslDemoService/Graphics/Control/GraphicsBeginFrameInfo.hpp>
#include <FslDemoService/Graphics/Control/GraphicsDependentCreateInfo.hpp>
#include <FslDemoService/Graphics/Control/IGraphicsServiceHost.hpp>
#include <FslDemoService/NativeGraphics/Vulkan/BasicNativeBeginCustomVulkanFrameInfo.hpp>
#include <FslDemoService/NativeGraphics/Vulkan/BasicNativeDependentCustomVulkanCreateInfo.hpp>
#include <FslDemoService/NativeGraphics/Vulkan/NativeGraphicsService.hpp>
#include <FslDemoService/NativeGraphics/Vulkan/NativeGraphicsSwapchainInfo.hpp>
#include <FslDemoService/SystemStats/Control/ISystemStatsServiceControl.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUScopedCmdDebugLabel.hpp>
#include <FslUtil/Vulkan1_0/Log/All.hpp>
#include <FslUtil/Vulkan1_0/Log/FmtAll.hpp>
#include <FslUtil/Vulkan1_0/TypeConverter.hpp>
#include <FslUtil/Vulkan1_0/Util/CommandBufferUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/MemoryBudgetUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceKHRUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/PhysicalDeviceUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/SurfaceFormatUtil.hpp>
#include <FslUtil/Vulkan1_0/Util/SwapchainKHRUtil.hpp>
#include <RapidVulkan/CommandBuffer.hpp>
#include <RapidVulkan/Debug/Strings/VkImageUsageFlagBits.hpp>
#include <RapidVulkan/Debug/Strings/VkResult.hpp>
#include <RapidVulkan/Exceptions.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <exception>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>

namespace Fsl::VulkanBasic
{
  namespace
  {
    namespace LocalConfig
    {
      // The desired minimum image count for the swap buffers
      //! The swapchain is asked for one image more than the surface needs and for at least this many. With the fewest images a
      //! surface allows the app can hold none while it waits, and with two images and a FIFO present the rendering of a frame can
      //! not begin before the frame before it is on the display.
      constexpr const uint32_t MinDesiredSwapBufferCount = 3;

      constexpr const auto DefaultTimeout = std::numeric_limits<uint64_t>::max();

      //! How often the system stats service is told the GPU memory usage of the app while it wants it
      constexpr TimeSpan GpuMemoryStatsInterval = TimeSpan::FromSeconds(1);

      //! The device extensions that have to do with when a frame is shown, from the least to the most a swapchain can do about time
      //! (Doc/FramePacingPlatformSupport.md). The frame pacing log says which of them the device has.
      constexpr std::array<const char*, 10> FramePacingExtensions = {"VK_KHR_present_id",
                                                                     "VK_KHR_present_id2",
                                                                     "VK_EXT_swapchain_maintenance1",
                                                                     "VK_KHR_swapchain_maintenance1",
                                                                     "VK_KHR_present_wait",
                                                                     "VK_KHR_present_wait2",
                                                                     "VK_GOOGLE_display_timing",
                                                                     "VK_EXT_calibrated_timestamps",
                                                                     "VK_KHR_calibrated_timestamps",
                                                                     "VK_EXT_present_timing"};
    }


    DemoAppVulkanSetup ProcessDemoAppSetup(DemoAppVulkanSetup demoAppVulkanSetup)
    {
      return demoAppVulkanSetup;
    }

    //! True for the present modes that show one image per refresh in the order they were presented. A present can only be given a
    //! target time with one of these.
    constexpr bool IsFifoPresentMode(const VkPresentModeKHR presentMode) noexcept
    {
      switch (presentMode)
      {
      case VK_PRESENT_MODE_FIFO_KHR:
      case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
#ifdef VK_KHR_present_mode_fifo_latest_ready
      case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR:
#endif
        return true;
      default:
        return false;
      }
    }

    VkImageUsageFlags FilterUnsupportedImageUsageFlags(const VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface,
                                                       const VkImageUsageFlags flags)
    {
      VkSurfaceCapabilitiesKHR surfaceCapabilities;
      RAPIDVULKAN_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities));

      VkImageUsageFlags srcFlags = flags;
      VkImageUsageFlags supportedFlags = 0;
      int bitIndex = BitsUtil::IndexOf(srcFlags);
      while (bitIndex >= 0)
      {
        const VkImageUsageFlags flag = static_cast<uint32_t>(1u) << bitIndex;
        if ((surfaceCapabilities.supportedUsageFlags & flag) != 0u)
        {
          supportedFlags |= flag;
        }
        else
        {
          FSLLOG3_WARNING("Desired swapchain image usage flag 0x{:x} ({}) is unsupported so it was ignored", flag,
                          RapidVulkan::Debug::ToString(static_cast<VkImageUsageFlagBits>(flag)));
        }
        srcFlags &= ~flag;
        bitIndex = BitsUtil::IndexOf(srcFlags);
      }
      return supportedFlags;
    }


    Vulkan::SurfaceFormatInfo FindPreferredSurfaceInfo(VkPhysicalDevice physicalDevice, const VkSurfaceKHR surface,
                                                       const std::deque<Vulkan::SurfaceFormatInfo>& preferredFormats)
    {
      std::vector<Vulkan::SurfaceFormatInfo> finalPreferredFormats(preferredFormats.begin(), preferredFormats.end());

      const auto supportedFormats = Vulkan::PhysicalDeviceKHRUtil::GetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface);
      if (!preferredFormats.empty())
      {
        auto res = Vulkan::SurfaceFormatUtil::TryFindPreferredFormat(SpanUtil::AsReadOnlySpan(supportedFormats),
                                                                     SpanUtil::AsReadOnlySpan(finalPreferredFormats));
        if (res.Format != VK_FORMAT_UNDEFINED)
        {
          return res;
        }
        FSLLOG3_VERBOSE("None of the preferred surface formats found, using default");
      }
      if (supportedFormats.empty())
      {
        throw NotSupportedException("No surface formats found");
      }

      // Try to locate a format
      {    // Add the suggested defaults
        const ReadOnlySpan<Vulkan::SurfaceFormatInfo> suggestedEntries = Vulkan::SurfaceFormatUtil::GetSuggestedDefaultPreferredSurfaceFormats();
        finalPreferredFormats.reserve(finalPreferredFormats.size() + suggestedEntries.size());
        for (const auto& suggested : suggestedEntries)
        {
          finalPreferredFormats.push_back(suggested);
        }
      }
      return Vulkan::SurfaceFormatUtil::TryFindSurfaceFormat(SpanUtil::AsReadOnlySpan(supportedFormats),
                                                             SpanUtil::AsReadOnlySpan(finalPreferredFormats), VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    }

    //! @param rResult the result of the Vulkan call that failed (VK_SUCCESS if nothing failed)
    AppDrawResult WaitForFenceAndResetIt(const VkDevice device, const VkFence fence, VkResult& rResult)
    {
      {    // Wait for the current frames fence and reset it
           // time to synchronize by waiting for the fence before we modify command queues etc.
        const auto waitResult = vkWaitForFences(device, 1, &fence, VK_TRUE, LocalConfig::DefaultTimeout);
        rResult = waitResult;
        if (waitResult != VK_SUCCESS)
        {
          FSLLOG3_WARNING("vkWaitForFences failed with: {}", RapidVulkan::Debug::ToString(waitResult));
          return AppDrawResult::Failed;
        }

        const auto resetResult = vkResetFences(device, 1, &fence);
        rResult = resetResult;
        if (resetResult != VK_SUCCESS)
        {
          FSLLOG3_WARNING("vkResetFences failed with: {}", RapidVulkan::Debug::ToString(resetResult));
          return AppDrawResult::Failed;
        }
      }
      return AppDrawResult::Completed;
    }

    //! The refresh mode a swapchain says it is in, as the log writes it
    constexpr const char* ToLogText(const Vulkan::VUPresentRefreshMode mode) noexcept
    {
      switch (mode)
      {
      case Vulkan::VUPresentRefreshMode::Fixed:
        return "fixed";
      case Vulkan::VUPresentRefreshMode::Variable:
        return "variable";
      case Vulkan::VUPresentRefreshMode::Unknown:
      default:
        return "unknown";
      }
    }

    //! Check if the present operation was enqueued, which means the present fence will be signaled.
    //! Even when the presentation engine rejects the request with some errors the queue operations are still considered to be enqueued.
    constexpr bool IsPresentFenceSignalExpected(const VkResult result) noexcept
    {
      switch (result)
      {
      case VK_SUCCESS:
      case VK_SUBOPTIMAL_KHR:
      case VK_ERROR_OUT_OF_DATE_KHR:
      case VK_ERROR_SURFACE_LOST_KHR:
        return true;
      default:
        return false;
      }
    }
  }

  //! What the Vulkan app base adds to the frame pacing log: when the swapchain was called and what the presentation engine measured
  struct DemoAppVulkanBasic::FramePacingLogState
  {
    struct PresentFrame
    {
      uint64_t PresentId{0};
      uint64_t FrameIndex{0};
    };

    std::shared_ptr<IFramePacingFrameLog> Log;
    FramePacingLogColumn PresentId;
    FramePacingLogColumn ImageIndex;
    FramePacingLogColumn SwapchainGeneration;
    FramePacingLogColumn AcquireCall;
    FramePacingLogColumn AcquireReturn;
    FramePacingLogColumn PresentCall;
    FramePacingLogColumn PresentReturn;
    FramePacingLogColumn PresentResult;
    FramePacingLogColumn AcquireResult;
    FramePacingLogColumn PresentTimingRequested;
    FramePacingLogColumn PresentTargetRelative;
    FramePacingLogColumn RefreshDuration;
    FramePacingLogColumn RefreshInterval;
    FramePacingLogColumn TimeDomainId;
    FramePacingLogColumn ResultReadAtFrame;
    //! One for each present stage: the time on the clock of the framework and the time as the presentation engine reported it
    std::array<FramePacingLogColumn, 4> StageTicks;
    std::array<FramePacingLogColumn, 4> StageRaw;

    //! The frame of the log that is being drawn (valid if HasFrame)
    uint64_t FrameIndex{0};
    bool HasFrame{false};
    //! The frame each of the last presents belongs to
    std::array<PresentFrame, 64> PresentFrames{};
    //! How many swapchains were created
    uint32_t Generation{0};
    uint32_t LoggedTimingPropertiesReadCount{0};
    uint32_t LoggedCalibrationCount{0};

    //! @return the state, null if the frames are not logged
    static std::unique_ptr<FramePacingLogState> TryCreate(std::shared_ptr<IFramePacingFrameLog> log,
                                                          const Vulkan::VUPhysicalDeviceRecord& physicalDevice,
                                                          const Vulkan::VulkanHostDeviceFeatures& hostDeviceFeatures,
                                                          const VulkanLaunchOptions& launchOptions, const bool swapchainMaintenance1Enabled)
    {
      if (!log || !log->IsLogEnabled())
      {
        return nullptr;
      }
      auto state = std::make_unique<FramePacingLogState>();
      IFramePacingFrameLog& rLog = *log;
      state->PresentId = rLog.RegisterColumn("presentId", FramePacingLogUnit::Id, "The number of the present of the frame, counted from one");
      state->ImageIndex = rLog.RegisterColumn("imageIndex", FramePacingLogUnit::Id, "The index of the swapchain image that was presented");
      state->SwapchainGeneration =
        rLog.RegisterColumn("swapchainGeneration", FramePacingLogUnit::Count, "How many swapchains were created up to this frame");
      state->AcquireCall = rLog.RegisterColumn("acquireCallTicks", FramePacingLogUnit::Ticks, "When vkAcquireNextImageKHR was called");
      state->AcquireReturn = rLog.RegisterColumn("acquireReturnTicks", FramePacingLogUnit::Ticks, "When vkAcquireNextImageKHR returned");
      state->PresentCall = rLog.RegisterColumn("presentCallTicks", FramePacingLogUnit::Ticks, "When vkQueuePresentKHR was called");
      state->PresentReturn = rLog.RegisterColumn("presentReturnTicks", FramePacingLogUnit::Ticks, "When vkQueuePresentKHR returned");
      state->PresentResult = rLog.RegisterColumn("presentResult", FramePacingLogUnit::Code, "The VkResult of vkQueuePresentKHR");
      state->AcquireResult =
        rLog.RegisterColumn("acquireResult", FramePacingLogUnit::Code, "The VkResult of vkAcquireNextImageKHR (1000001003 is VK_SUBOPTIMAL_KHR)");
      state->PresentTimingRequested =
        rLog.RegisterColumn("presentTimingRequested", FramePacingLogUnit::Flag,
                            "1 if the present was asked to be timed, 0 if not: present timing is off, or too many results were outstanding");
      state->PresentTargetRelative =
        rLog.RegisterColumn("presentTargetRelativeNs", FramePacingLogUnit::Nanoseconds,
                            "The target time the present was given: its image is not shown before this long after the image of the present "
                            "before it was shown (empty: the present was not scheduled)");
      state->RefreshDuration = rLog.RegisterColumn("refreshDurationNs", FramePacingLogUnit::Nanoseconds,
                                                   "VkSwapchainTimingPropertiesEXT::refreshDuration as the swapchain last reported it");
      state->RefreshInterval = rLog.RegisterColumn("refreshIntervalNs", FramePacingLogUnit::Nanoseconds,
                                                   "VkSwapchainTimingPropertiesEXT::refreshInterval as the swapchain last reported it");

      // VK_EXT_present_timing: the stages of a present, known a few frames after the present
      constexpr std::array<const char*, 4> StageNames = {"queueOperationsEnd", "requestDequeued", "firstPixelOut", "firstPixelVisible"};
      constexpr std::array<const char*, 4> StageDescriptions = {
        "the queue operations of the present ended (the image was handed to the presentation engine)",
        "the presentation engine took the present from its queue", "the first pixel of the image left for the display",
        "the first pixel of the image became visible on the display"};
      for (std::size_t i = 0; i < StageNames.size(); ++i)
      {
        state->StageTicks[i] = rLog.RegisterColumn(fmt::format("{}Ticks", StageNames[i]), FramePacingLogUnit::Ticks,
                                                   fmt::format("When {}, on the clock of the framework", StageDescriptions[i]));
        state->StageRaw[i] =
          rLog.RegisterColumn(fmt::format("{}RawNs", StageNames[i]), FramePacingLogUnit::Nanoseconds,
                              fmt::format("When {}, as the presentation engine reported it on the clock of its time domain", StageDescriptions[i]));
      }
      state->TimeDomainId =
        rLog.RegisterColumn("presentTimeDomainId", FramePacingLogUnit::Id, "The id of the time domain the stages were reported in");
      state->ResultReadAtFrame = rLog.RegisterColumn("resultReadAtFrame", FramePacingLogUnit::Id,
                                                     "The frame in which the stages of this frame were read: how late they arrived");

      // The facts of the device the frames are drawn with
      const VkPhysicalDeviceProperties& properties = physicalDevice.Properties;
      rLog.SetLogFact("vulkan.deviceName", static_cast<const char*>(properties.deviceName));
      rLog.SetLogFact("vulkan.vendorId", fmt::format("{:#x}", properties.vendorID));
      rLog.SetLogFact("vulkan.deviceId", fmt::format("{:#x}", properties.deviceID));
      rLog.SetLogFact("vulkan.driverVersion", fmt::format("{}", properties.driverVersion));
      rLog.SetLogFact("vulkan.apiVersion", fmt::format("{}.{}.{}", VK_API_VERSION_MAJOR(properties.apiVersion),
                                                       VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion)));
      rLog.SetLogFact("vulkan.calibratedTimestamps", hostDeviceFeatures.CalibratedTimestamps ? "1" : "0");
      rLog.SetLogFact("vulkan.presentTimingDevice", hostDeviceFeatures.PresentTiming ? "1" : "0");
      rLog.SetLogFact("vulkan.presentAtRelativeTimeDevice", hostDeviceFeatures.PresentAtRelativeTime ? "1" : "0");
      rLog.SetLogFact("vulkan.presentTimingOption", fmt::format("{}", static_cast<int32_t>(launchOptions.PresentTiming)));
      {    // What the device has for frame pacing, used or not, so a log says by itself what the platform offers
        const auto deviceExtensions = Vulkan::PhysicalDeviceUtil::EnumerateDeviceExtensionProperties(physicalDevice.Device);
        std::string available;
        for (const char* const pszExtensionName : LocalConfig::FramePacingExtensions)
        {
          const bool isAvailable =
            std::any_of(deviceExtensions.begin(), deviceExtensions.end(), [pszExtensionName](const VkExtensionProperties& entry)
                        { return std::strcmp(static_cast<const char*>(entry.extensionName), pszExtensionName) == 0; });
          rLog.SetLogFact(fmt::format("vulkan.has.{}", pszExtensionName), isAvailable ? "1" : "0");
          if (isAvailable)
          {
            fmt::format_to(std::back_inserter(available), "{}{}", available.empty() ? "" : ", ", pszExtensionName);
          }
        }
        // And what the framework uses of it for this device
        const bool usesPresentTiming = hostDeviceFeatures.PresentTiming;
        rLog.SetLogFact("vulkan.uses.VK_EXT_present_timing", usesPresentTiming ? "1" : "0");
        rLog.SetLogFact("vulkan.uses.VK_KHR_present_id2", usesPresentTiming ? "1" : "0");
        // The KHR or the EXT version of the extension, whichever the device has
        rLog.SetLogFact("vulkan.uses.calibrated_timestamps", hostDeviceFeatures.CalibratedTimestamps ? "1" : "0");
        rLog.SetLogFact("vulkan.uses.swapchain_maintenance1", swapchainMaintenance1Enabled ? "1" : "0");
        rLog.SetLogFact("vulkan.uses.presentAtRelativeTime", hostDeviceFeatures.PresentAtRelativeTime ? "1" : "0");
        FSLLOG3_INFO(
          "FramePacing: Vulkan device has [{}], uses present timing: {}, a relative target time: {}, calibrated timestamps: {}, "
          "present fences: {}",
          available, usesPresentTiming, hostDeviceFeatures.PresentAtRelativeTime, hostDeviceFeatures.CalibratedTimestamps,
          swapchainMaintenance1Enabled);
      }
      state->Log = std::move(log);
      return state;
    }
  };


  DemoAppVulkanBasic::DemoAppVulkanBasic(const DemoAppConfig& demoAppConfig, const DemoAppVulkanSetup& demoAppVulkanSetup)
    : DemoAppVulkan(demoAppConfig)
    , AppSetup(ProcessDemoAppSetup(demoAppVulkanSetup))
    , m_cachedExtentPx(demoAppConfig.WindowMetrics.ExtentPx)
  {
    const auto hostInfo = demoAppConfig.DemoServiceProvider.Get<IHostInfo>();
    const auto hostConfig = hostInfo->GetConfig();
    if (hostConfig.StatOverlay)
    {
      m_demoAppProfilerOverlay = std::make_unique<DemoAppProfilerOverlay>(demoAppConfig.DemoServiceProvider, hostConfig.LogStatsFlags);
    }
    // The frame pacing service is only registered on platforms that support the marker
    if (const auto framePacingServiceControl = demoAppConfig.DemoServiceProvider.TryGet<IFramePacingMarkerServiceControl>())
    {
      m_framePacingOverlay = framePacingServiceControl->CreateOverlay(demoAppConfig.DemoServiceProvider);
    }
    m_systemStatsServiceControl = demoAppConfig.DemoServiceProvider.TryGet<ISystemStatsServiceControl>();
    m_framePacingLogState = FramePacingLogState::TryCreate(demoAppConfig.DemoServiceProvider.TryGet<IFramePacingFrameLog>(), m_physicalDevice,
                                                           m_hostDeviceFeatures, m_launchOptions, m_swapchainMaintenance1Enabled);
    const auto demoHostConfig = hostInfo->TryGetAppHostConfig();
    if (!demoHostConfig)
    {
      throw NotSupportedException("Could not access the demo host config");
    }
    m_demoHostConfig = std::dynamic_pointer_cast<DemoAppHostConfigVulkan>(demoHostConfig);
    if (!m_demoHostConfig)
    {
      throw NotSupportedException("DemoHostConfig not of the expected type");
    }

    m_surfaceFormatInfo = FindPreferredSurfaceInfo(m_physicalDevice.Device, m_surface, m_demoHostConfig->GetPreferredSurfaceFormats());
    m_presentTimingRequested = AppSetup.PresentTiming;

    m_resources.MainCommandPool.Reset(m_device.Get(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, m_deviceQueue.QueueFamilyIndex);
    Vulkan::VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_COMMAND_POOL, m_resources.MainCommandPool.Get(), "MainCommandPool");
    m_resources.Frames = CreateFrameSyncObjects(m_device.Get(), GetRenderConfig().MaxFramesInFlight, m_swapchainMaintenance1Enabled);
    m_resources.FrameSubmitValues.assign(m_resources.Frames.size(), 0u);
    m_useFrameTimeline = m_deviceActiveFeatures12.timelineSemaphore != VK_FALSE && m_launchOptions.TimelineSemaphore != OptionUserChoice::Off;
    if (m_useFrameTimeline)
    {
      VkSemaphoreTypeCreateInfo typeCreateInfo{};
      typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
      typeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      typeCreateInfo.initialValue = 0;
      VkSemaphoreCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      createInfo.pNext = &typeCreateInfo;
      m_resources.FrameTimeline.Reset(m_device.Get(), createInfo);
      Vulkan::VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_SEMAPHORE, m_resources.FrameTimeline.Get(), "FrameTimeline");
    }
    FSLLOG3_VERBOSE("Frame synchronization: {}", m_useFrameTimeline ? "a timeline semaphore" : "a fence per frame slot");
  }


  DemoAppVulkanBasic::~DemoAppVulkanBasic()
  {
    if (m_dependentResources.Valid)
    {
      const auto currentLifeCycleState = GetObjectLifeCycleState();
      try
      {
        FSLLOG3_WARNING_IF(currentLifeCycleState != ObjectLifeCycle::Constructing,
                           "Resources still allocated during destruction, trying to free them");
        FreeResources();

        // Release the swapchain (we dont do this in FreeResources because BuildResources might reuse it).
        m_presentTiming.Reset();
        m_swapchain.Reset();
      }
      catch (const std::exception& ex)
      {
        FSLLOG3_ERROR("Exception during FreeResources: {}", ex.what());
        FSLLOG3_ERROR("FreeResources failed to complete. Setting exit code to failure");
        try
        {
          // IMPROVEMENT: Be able to RequestExit and change exit code in one operation
          // We request a exit (this might silently return if a exit is already scheduled so the exit code will not be set)
          GetDemoAppControl()->RequestExit(EXIT_FAILURE);
          // Which is why we also call ChangeExitCode (beware this only changes the exit code if a non-default exit code has not been set)
          GetDemoAppControl()->ChangeExitCode(EXIT_FAILURE);
        }
        catch (const std::exception& ex2)
        {
          FSLLOG3_ERROR("internal error, failed to request exit: {0}", ex2.what());
        }
      }
    }
  }


  void DemoAppVulkanBasic::OnConstructed()
  {
    DemoAppVulkan::OnConstructed();
    if (!m_dependentResources.Valid)
    {
      BuildResources();
    }
  }


  void DemoAppVulkanBasic::OnDestroy()
  {
    // Calling FreeResources here instead of in the destructor ensures that 'virtual' methods are called as expected
    FreeResources();

    // Release the swapchain (we dont do this in FreeResources because BuildResources might reuse it).
    m_presentTiming.Reset();
    m_swapchain.Reset();

    // Finally call the OnDestroy of our inherited object
    DemoAppVulkan::OnDestroy();
  }

  AppDrawResult DemoAppVulkanBasic::TryPrepareDraw(const FrameInfo& frameInfo)
  {
    const auto result = TryDoPrepareDraw(frameInfo);
    SetAppState(result);

    const FrameDrawRecord& frameResources = m_resources.Frames[frameInfo.FrameIndex];
    if (result == AppDrawResult::Completed && m_dependentResources.NGScreenshotLink && m_swapchain.IsValid() && frameResources.HasSwapBufferImage)
    {
      *m_dependentResources.NGScreenshotLink = Vulkan::NativeGraphicsSwapchainInfo(m_swapchain[frameResources.AssignedSwapImageIndex],
                                                                                   m_swapchain.GetImageFormat(), m_swapchain.GetImageUsageFlags());
    }
    else
    {
      *m_dependentResources.NGScreenshotLink = {};
    }
    return result;
  }

  void DemoAppVulkanBasic::_BeginDraw(const FrameInfo& frameInfo)
  {
    DemoAppVulkan::_BeginDraw(frameInfo);

    // _Endframe was not called properly
    assert(!m_frameRecord.IsValid);

    // Collect the present measurements that arrived, so the app can use them while it draws this frame
    m_presentTimingRecords.clear();
    m_presentTiming.Poll(m_presentTimingRecords);
    UpdateGpuMemoryStats();
    LogFrameBegin();
    if (m_graphicsServiceHost)
    {
      Vulkan::BasicNativeBeginCustomVulkanFrameInfo vulkanBeginInfo(m_dependentResources.CmdBuffers[frameInfo.FrameIndex]);
      const GraphicsBeginFrameInfo beginInfo(frameInfo.FrameIndex, &vulkanBeginInfo);
      m_graphicsServiceHost->BeginFrame(beginInfo);
    }
    m_frameRecord = FrameRecord(frameInfo.FrameIndex);
  }

  void DemoAppVulkanBasic::_EndDraw(const FrameInfo& frameInfo)
  {
    if (m_graphicsServiceHost)
    {
      m_graphicsServiceHost->EndFrame();
    }
    m_frameRecord = {};
    DemoAppVulkan::_EndDraw(frameInfo);
  }


  void DemoAppVulkanBasic::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    DemoAppVulkan::ConfigurationChanged(windowMetrics);

    if (windowMetrics.ExtentPx != m_cachedExtentPx)
    {
      m_cachedExtentPx = windowMetrics.ExtentPx;

      // Quick, dirty and slow resize support by destroying any screen size dependent Vulkan resources.
      if (AppSetup.ActiveResizeStrategy == ResizeStrategy::RebuildResources)
      {
        if (IsResourcesAllocated())
        {
          TryRebuildResources();
        }
      }
    }
  }


  void DemoAppVulkanBasic::Draw(const FrameInfo& frameInfo)
  {
    const auto currentFrameIndex = frameInfo.FrameIndex;
    const FrameDrawRecord& frameRecord = m_resources.Frames[currentFrameIndex];
    // Allow the app to draw
    const SwapchainRecord& swapchainRecord = m_dependentResources.SwapchainRecords[frameRecord.AssignedSwapImageIndex];
    const VkFramebuffer framebuffer = swapchainRecord.Framebuffer.Get();

    // Possible improvement:
    // We could replace the VulkanDraw with a normal Draw call but that would just mean that all implementations would have
    // to get the commandBuffers, currentSwapBufferIndex and active framebuffer as the first thing they do.
    // We would need a helper method like this
    //    VulkanDrawContext& GetVulkanDrawContext() const
    // The context would be non copyable and contain the above mentioned things

    const DrawContext drawContext(m_swapchain.GetImageExtent(), framebuffer, currentFrameIndex);
    try
    {
      VulkanDraw(frameInfo.Time, m_dependentResources.CmdBuffers, drawContext);
      SubmitFrame(frameRecord, swapchainRecord, currentFrameIndex);
    }
    catch (const RapidVulkan::VulkanErrorException& ex)
    {
      // This is the only place that sees the result of a failed draw or submit, so its the place to ask why a device was lost
      ReportDeviceLost(ex.GetResult());
      throw;
    }
  }


  void DemoAppVulkanBasic::SubmitFrame(const FrameDrawRecord& frameRecord, const SwapchainRecord& swapchainRecord, const uint32_t currentFrameIndex)
  {
    assert(frameRecord.ImageAcquiredSemaphore.IsValid());
    const VkSemaphore waitSemaphore = frameRecord.ImageAcquiredSemaphore.Get();
    const VkSemaphore signalSemaphore = swapchainRecord.ImageReleasedSemaphore.Get();
    // The frame is waited for by its fence, or with a timeline semaphore by the value this submit gives it
    const VkFence queueSubmitFence = m_useFrameTimeline ? VK_NULL_HANDLE : frameRecord.QueueSubmitFence.Get();
    const std::array<VkSemaphore, 2> signalSemaphores = {signalSemaphore, m_resources.FrameTimeline.Get()};
    // The value of a binary semaphore is not looked at
    const std::array<uint64_t, 2> signalValues = {0u, m_frameTimelineValue + 1u};
    VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
    timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineSubmitInfo.signalSemaphoreValueCount = static_cast<uint32_t>(signalValues.size());
    timelineSubmitInfo.pSignalSemaphoreValues = signalValues.data();

    // Submit the draw operations
    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &waitSemaphore;
    submitInfo.pWaitDstStageMask = &waitDstStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = m_dependentResources.CmdBuffers.GetPointer(currentFrameIndex);
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    if (m_useFrameTimeline)
    {
      submitInfo.pNext = &timelineSubmitInfo;
      submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalSemaphores.size());
      submitInfo.pSignalSemaphores = signalSemaphores.data();
    }

    m_deviceQueue.Submit(1, &submitInfo, queueSubmitFence);
    if (m_useFrameTimeline)
    {
      ++m_frameTimelineValue;
      m_resources.FrameSubmitValues[currentFrameIndex] = m_frameTimelineValue;
    }
  }


  AppDrawResult DemoAppVulkanBasic::TrySwapBuffers(const FrameInfo& frameInfo)
  {
    const auto result = TryDoSwapBuffers(frameInfo);
    SetAppState(result);
    return result;
  }


  void DemoAppVulkanBasic::AddSystemUI(VkCommandBuffer hCmdBuffer, [[maybe_unused]] const uint32_t frameIndex)
  {
    // We assume that we get called with the 'active' command buffer
    assert(m_dependentResources.Valid);
    // assert(cmdBufferIndex == m_resources.CurrentFrame);
    assert(hCmdBuffer == m_dependentResources.CmdBuffers[frameIndex]);

    if (!m_demoAppProfilerOverlay && !m_framePacingOverlay)
    {
      return;
    }

    const Vulkan::VUScopedCmdDebugLabel scopedLabel(hCmdBuffer, "SystemUI");
    if (m_demoAppProfilerOverlay)
    {
      m_demoAppProfilerOverlay->Draw(GetWindowMetrics());
    }
    // The frame pacing marker must be the very last thing drawn
    if (m_framePacingOverlay)
    {
      m_framePacingOverlay->Draw(GetWindowMetrics());
    }
  }


  void DemoAppVulkanBasic::BuildResources()
  {
    if (m_dependentResources.Valid)
    {
      throw UsageErrorException("Resources are still allocated, call FreeResources first");
    }
    FSLLOG3_VERBOSE("DemoAppVulkanBasic::BuildResources()");

    try
    {
      // We do this right away to ensure we can call 'FreeResources' if something goes wrong
      m_dependentResources.Valid = true;

      FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): Creating swapchain");
      // m_launchOptions.ScreenshotsEnabled

      auto desiredSwapchainImageUsageFlags = AppSetup.DesiredSwapchainImageUsageFlags;
      if (m_launchOptions.ScreenshotsEnabled != OptionUserChoice::Off)
      {
        // Add this to allow for screenshot support and we rely on the filtering below to remove it if its unsupported
        desiredSwapchainImageUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      }

      const auto fallbackExtent = TypeConverter::UncheckedTo<VkExtent2D>(GetScreenExtent());
      VkPresentModeKHR presentMode = !m_launchOptions.OverridePresentMode ? AppSetup.DesiredSwapchainPresentMode : m_launchOptions.PresentMode;
#ifdef VK_KHR_present_mode_fifo_latest_ready
      if (presentMode == VK_PRESENT_MODE_FIFO_LATEST_READY_KHR && !m_hostDeviceFeatures.PresentModeFifoLatestReady)
      {
        // The surface can list the mode without the device having the extension enabled, which is needed to use it
        FSLLOG3_WARNING("PresentMode: VK_PRESENT_MODE_FIFO_LATEST_READY_KHR is not supported by the device, using VK_PRESENT_MODE_FIFO_KHR");
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
      }
#endif
      const auto supportedImageUsageFlags = FilterUnsupportedImageUsageFlags(m_physicalDevice.Device, m_surface, desiredSwapchainImageUsageFlags);
      const VkImageUsageFlags desiredImageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | supportedImageUsageFlags;

      // Measuring when the frames are presented is something the app (or the user) asks for and both the device and the surface must support it
      const bool usePresentTiming = m_hostDeviceFeatures.PresentTiming && m_launchOptions.PresentTiming != OptionUserChoice::Off &&
                                    (m_presentTimingRequested || m_launchOptions.PresentTiming == OptionUserChoice::On);
      m_presentTimingChangePending = false;
      m_swapchainSuboptimalPending = false;
      m_swapchainSuboptimalCount = 0;
      const uint32_t desiredMinImageCount = GetDesiredMinSwapBufferCount();
      const VkSwapchainCreateFlagsKHR swapchainCreateFlags =
        usePresentTiming ? Vulkan::VUSwapchainPresentTiming::GetSwapchainCreateFlags(m_physicalDevice.Device, m_surface) : 0u;
      // The old swapchain is retired when the new one is created
      m_presentTiming.Reset();
      m_presentTimingRecords.clear();

      m_swapchain = Vulkan::SwapchainKHRUtil::CreateSwapchain(m_physicalDevice.Device, m_device.Get(), swapchainCreateFlags, m_surface,
                                                              desiredMinImageCount, 1, desiredImageUsageFlags, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
                                                              VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, presentMode, VK_TRUE, m_swapchain.Get(),
                                                              fallbackExtent, m_surfaceFormatInfo, m_swapchainMaintenance1Enabled);
      // The helper can replace the present mode that was asked for by one the surface supports, so the swapchain is asked
      presentMode = m_swapchain.GetPresentMode();
      if (swapchainCreateFlags != 0u)
      {
        if (m_presentTiming.Reset(m_physicalDevice.Device, m_device.Get(), m_surface, m_swapchain.Get(), m_calibratedTimestamps) &&
            m_hostDeviceFeatures.PresentAtRelativeTime && IsFifoPresentMode(presentMode))
        {
          // The presents of this swapchain can be given a target time, if its surface supports that as well. A target time is only
          // valid with a present mode of the FIFO family (VUID-VkPresentTimingsInfoEXT-pSwapchains-12235).
          m_presentTiming.TryEnablePresentAtRelativeTime();
        }
      }
      FSLLOG3_VERBOSE_IF(usePresentTiming, "Present timing: {}", m_presentTiming.IsEnabled() ? "enabled" : "not supported by the surface");

      const uint32_t swapchainImageCount = m_swapchain.GetImageCount();
      if (swapchainImageCount == 0)
      {
        throw std::runtime_error("We need at least one image in the swapchain");
      }

      // One frame in flight unless the user asks for more (--VkFramesInFlight), which is limited to what the app is configured for and to
      // the images of the swapchain
      m_dependentResources.FramesInFlightCount =
        std::clamp(m_launchOptions.FramesInFlight, 1u, std::max(std::min(GetRenderConfig().MaxFramesInFlight, swapchainImageCount), 1u));
      FSLLOG3_VERBOSE_IF(m_launchOptions.FramesInFlight != 0u, "DemoAppVulkanBasic::BuildResources(): Frames in flight: {}",
                         m_dependentResources.FramesInFlightCount);
      FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): Swapchain image count: {}", swapchainImageCount);
      // Ensure that the render loop frame counter never goes above this value
      GetDemoAppControl()->SetRenderLoopFrameCounter(m_dependentResources.FramesInFlightCount);
      LogSwapchainCreated(presentMode, swapchainCreateFlags, desiredMinImageCount);

      if (AppSetup.DepthBuffer == DepthBufferMode::Enabled)
      {
        FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): Creating depth image view");
        auto extent = m_swapchain.GetImageExtent();
        extent = VkExtent2D{std::max(extent.width, AppSetup.DepthBufferMinimumExtent.Width.Value),
                            std::max(extent.height, AppSetup.DepthBufferMinimumExtent.Height.Value)};
        m_dependentResources.DepthImage = CreateBasicDepthImageView(m_device, extent, m_resources.MainCommandPool.Get());

        FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): DepthBuffer PixelFormat: {}", m_dependentResources.DepthImage.Image().GetFormat());
      }

      FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): Creating command buffers");
      m_dependentResources.CmdBuffers.Reset(m_device.Get(), m_resources.MainCommandPool.Get(), VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                            m_dependentResources.FramesInFlightCount);

      FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): OnBuildResources");
      // Allow the app to create its resources
      const VkImageView depthImageView = m_dependentResources.DepthImage.GetImageView();
      const VkFormat depthImageFormat = m_dependentResources.DepthImage.GetFormat();
      const VkExtent2D depthImageExtent = m_dependentResources.DepthImage.GetExtent2D();

      const BuildResourcesContext buildResourcesContext(m_swapchain.GetImageExtent(), m_swapchain.GetImageFormat(), swapchainImageCount,
                                                        m_dependentResources.FramesInFlightCount, depthImageView, depthImageFormat, depthImageExtent,
                                                        m_resources.MainCommandPool.Get());
      const VkRenderPass mainRenderPass = OnBuildResources(buildResourcesContext);

      FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): Populate swapchain records");

      m_dependentResources.SwapchainRecords.clear();
      m_dependentResources.SwapchainRecords.resize(swapchainImageCount);

      for (uint32_t i = 0; i < swapchainImageCount; ++i)
      {
        BuildSwapchainImageView(m_dependentResources.SwapchainRecords[i], i);
        const VkImageView swapchainImageView = m_dependentResources.SwapchainRecords[i].SwapchainImageView.Get();

        const FrameBufferCreateContext frameBufferCreateContext(swapchainImageView, m_swapchain.GetImageExtent(), mainRenderPass, depthImageView);
        m_dependentResources.SwapchainRecords[i].Framebuffer = CreateFramebuffer(frameBufferCreateContext);
        m_dependentResources.SwapchainRecords[i].ImageReleasedSemaphore.Reset(m_device.Get(), 0);
      }

      if (Vulkan::VUDebugUtils::IsEnabled())
      {
        // Name the objects so they can be recognized in validation messages and tools like RenderDoc
        const VkDevice device = m_device.Get();
        Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_SWAPCHAIN_KHR, m_swapchain.Get(), "Swapchain");
        for (uint32_t i = 0; i < m_dependentResources.FramesInFlightCount; ++i)
        {
          Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_COMMAND_BUFFER, m_dependentResources.CmdBuffers[i],
                                              fmt::format("Frame{}.CmdBuffer", i));
        }
        for (uint32_t i = 0; i < swapchainImageCount; ++i)
        {
          const SwapchainRecord& record = m_dependentResources.SwapchainRecords[i];
          Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_IMAGE, m_swapchain[i], fmt::format("Swapchain.Image{}", i));
          Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, record.SwapchainImageView.Get(),
                                              fmt::format("Swapchain.Image{}.View", i));
          Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_FRAMEBUFFER, record.Framebuffer.Get(),
                                              fmt::format("Swapchain.Framebuffer{}", i));
          Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, record.ImageReleasedSemaphore.Get(),
                                              fmt::format("Swapchain.ImageReleased{}", i));
        }
      }

      // Create a struct containing all relevant information to be able to capture a screenshot on demand
      m_dependentResources.NGScreenshotLink = std::make_shared<Vulkan::NativeGraphicsSwapchainInfo>();

      if (m_graphicsServiceHost)
      {
        FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): IGraphicsServiceHost::CreateDependentResources");

        // This also links the swapchain info with the native graphics service this information is used for enabling the screenshot capabilities.
        const VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        Vulkan::BasicNativeDependentCustomVulkanCreateInfo vulkanCreateInfo(pipelineCache, mainRenderPass, AppSetup.SubpassSystemUI,
                                                                            m_dependentResources.NGScreenshotLink);

        const GraphicsDependentCreateInfo createInfo(GetScreenExtent(), &vulkanCreateInfo);
        m_graphicsServiceHost->CreateDependentResources(createInfo);
      }
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_ERROR("BuildResources failed with: {}", ex.what());
      FreeResources();
      // A swapchain that was given to vkCreateSwapchainKHR as oldSwapchain is retired, also when the creation failed, and a retired
      // swapchain can not be given again. So it goes here, and the next attempt creates one from nothing.
      m_swapchain.Reset();
      throw;
    }
    FSLLOG3_VERBOSE2("DemoAppVulkanBasic::BuildResources(): Completed");
  }

  void DemoAppVulkanBasic::FreeResources()
  {
    if (!m_dependentResources.Valid)
    {
      return;
    }
    FSLLOG3_VERBOSE("DemoAppVulkanBasic::FreeResources()");

    SafeWaitForDeviceIdle();

    if (m_graphicsServiceHost)
    {
      m_graphicsServiceHost->DestroyDependentResources();
    }

    // Device idle does not cover the presentation engine, so when possible we wait for it to release the semaphores before destroying them
    for (auto& rFrame : m_resources.Frames)
    {
      TryWaitForPresentFence(rFrame);
    }

    for (auto& rRecord : m_dependentResources.SwapchainRecords)
    {
      rRecord.ImageReleasedSemaphore.Reset();
      rRecord.Framebuffer.Reset();
      rRecord.SwapchainImageView.Reset();
    }
    m_dependentResources.SwapchainRecords.clear();

    // Allow the app to free its custom resources
    OnFreeResources();

    m_dependentResources.CmdBuffers.Reset();
    m_dependentResources.DepthImage.Reset();

    SafeWaitForDeviceIdle();
    m_dependentResources.FramesInFlightCount = 0;
    m_dependentResources.Valid = false;
    FSLLOG3_VERBOSE2("DemoAppVulkanBasic::FreeResources(): Completed");
  }


  VkResult DemoAppVulkanBasic::WaitForFrameSlot(const uint32_t frameIndex)
  {
    if (!m_useFrameTimeline)
    {
      return vkWaitForFences(m_device.Get(), 1, m_resources.Frames[frameIndex].QueueSubmitFence.GetPointer(), VK_TRUE, LocalConfig::DefaultTimeout);
    }
    const uint64_t submitValue = m_resources.FrameSubmitValues[frameIndex];
    if (submitValue == 0u)
    {
      // Nothing was submitted from this frame slot yet
      return VK_SUCCESS;
    }
    const VkSemaphore timeline = m_resources.FrameTimeline.Get();
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &timeline;
    waitInfo.pValues = &submitValue;
    return vkWaitSemaphores(m_device.Get(), &waitInfo, LocalConfig::DefaultTimeout);
  }


  uint32_t DemoAppVulkanBasic::GetDesiredMinSwapBufferCount() const
  {
    if (m_launchOptions.SwapchainImages != 0u)
    {
      return m_launchOptions.SwapchainImages;
    }
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice.Device, m_surface, &surfaceCapabilities) != VK_SUCCESS)
    {
      return LocalConfig::MinDesiredSwapBufferCount;
    }
    return std::max(surfaceCapabilities.minImageCount + 1u, LocalConfig::MinDesiredSwapBufferCount);
  }


  bool DemoAppVulkanBasic::IsSwapchainRecreationWorthIt() const
  {
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice.Device, m_surface, &surfaceCapabilities) != VK_SUCCESS)
    {
      return false;
    }
    // A surface that leaves its extent to the swapchain (the special value) can not be out of step with it
    const VkExtent2D swapchainExtent = m_swapchain.GetImageExtent();
    const bool hasExtent = surfaceCapabilities.currentExtent.width != 0xFFFFFFFF || surfaceCapabilities.currentExtent.height != 0xFFFFFFFF;
    const bool extentDiffers = hasExtent && (surfaceCapabilities.currentExtent.width != swapchainExtent.width ||
                                             surfaceCapabilities.currentExtent.height != swapchainExtent.height);
    // The pre-transform a new swapchain would get: identity where the surface supports it (see SwapchainKHRUtil::CreateSwapchain).
    // A rotated surface under a identity swapchain says suboptimal for every frame, and a new swapchain would be the same again.
    const VkSurfaceTransformFlagBitsKHR newPreTransform = (surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0u
                                                            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                                                            : surfaceCapabilities.currentTransform;
    return extentDiffers || newPreTransform != m_swapchain.GetPreTransform();
  }


  void DemoAppVulkanBasic::LogSwapchainCreated(const VkPresentModeKHR presentMode, const VkSwapchainCreateFlagsKHR createFlags,
                                               const uint32_t desiredMinImageCount)
  {
    if (!m_framePacingLogState)
    {
      return;
    }
    FramePacingLogState& rState = *m_framePacingLogState;
    ++rState.Generation;
    rState.LoggedTimingPropertiesReadCount = 0;
    rState.LoggedCalibrationCount = 0;

    const VkExtent2D extent = m_swapchain.GetImageExtent();
    const bool hasPresentFence = !m_resources.Frames.empty() && m_resources.Frames.front().PresentFence.IsValid();
    rState.Log->AddLogEvent("swapchainCreated",
                            fmt::format("generation={};widthPx={};heightPx={};format={};presentMode={};desiredMinImageCount={};imageCount={};"
                                        "createFlags={:#x};imageUsage={:#x};presentFence={};framesInFlight={};frameSync={}",
                                        rState.Generation, extent.width, extent.height, static_cast<int32_t>(m_swapchain.GetImageFormat()),
                                        static_cast<int32_t>(presentMode), desiredMinImageCount, m_swapchain.GetImageCount(), createFlags,
                                        m_swapchain.GetImageUsageFlags(), hasPresentFence ? 1 : 0, m_dependentResources.FramesInFlightCount,
                                        m_useFrameTimeline ? "timeline" : "fence"));

    const Vulkan::VUPresentTimingState timingState = m_presentTiming.GetState();
    rState.Log->AddLogEvent("presentTiming", fmt::format("generation={};enabled={};requested={};stages={:#x};timeDomain={};timeDomainId={};"
                                                         "presentAtAbsoluteTime={};presentAtRelativeTime={};canSchedule={}",
                                                         rState.Generation, m_presentTiming.IsEnabled() ? 1 : 0, m_presentTimingRequested ? 1 : 0,
                                                         timingState.StageQueries, timingState.TimeDomain, timingState.TimeDomainId,
                                                         timingState.PresentAtAbsoluteTime ? 1 : 0, timingState.PresentAtRelativeTime ? 1 : 0,
                                                         m_presentTiming.CanPresentAtRelativeTime() ? 1 : 0));
  }


  void DemoAppVulkanBasic::LogFrameBegin()
  {
    if (!m_framePacingLogState)
    {
      return;
    }
    FramePacingLogState& rState = *m_framePacingLogState;
    IFramePacingFrameLog& rLog = *rState.Log;

    // The frame the service started for this draw. The image was acquired before it was started, so that is written now.
    rState.FrameIndex = rLog.GetLogFrameIndex();
    rState.HasFrame = true;
    rLog.SetLogValue(rState.AcquireCall, m_currentPresentCalls.AcquireCallTime);
    rLog.SetLogValue(rState.AcquireReturn, m_currentPresentCalls.AcquireReturnTime);
    rLog.SetLogInt64(rState.AcquireResult, m_currentPresentCalls.AcquireResult);
    rLog.SetLogUInt64(rState.SwapchainGeneration, rState.Generation);

    const Vulkan::VUPresentTimingState timingState = m_presentTiming.GetState();
    if (timingState.TimingPropertiesReadCount > 0u)
    {
      // As the swapchain reported them, the two are written apart: by the specification they tell a fixed from a variable refresh mode
      rLog.SetLogUInt64(rState.RefreshDuration, timingState.RefreshDurationNanoseconds);
      rLog.SetLogUInt64(rState.RefreshInterval, timingState.RefreshIntervalNanoseconds);
    }
    if (timingState.TimingPropertiesReadCount != rState.LoggedTimingPropertiesReadCount)
    {
      rState.LoggedTimingPropertiesReadCount = timingState.TimingPropertiesReadCount;
      rLog.AddLogEvent("refreshProperties",
                       fmt::format("generation={};refreshDurationNs={};refreshIntervalNs={};counter={};readCount={};refreshMode={}",
                                   rState.Generation, timingState.RefreshDurationNanoseconds, timingState.RefreshIntervalNanoseconds,
                                   timingState.TimingPropertiesCounter, timingState.TimingPropertiesReadCount,
                                   ToLogText(timingState.GetRefreshMode())));
    }
    if (timingState.CalibrationCount != rState.LoggedCalibrationCount)
    {
      rState.LoggedCalibrationCount = timingState.CalibrationCount;
      for (std::size_t stageIndex = 0; stageIndex < timingState.HasStageOffset.size(); ++stageIndex)
      {
        if (timingState.HasStageOffset[stageIndex])
        {
          rLog.AddLogEvent("presentClockCalibration",
                           fmt::format("generation={};stage={:#x};offsetTicks={};maxDeviationNs={};calibrationCount={}", rState.Generation,
                                       1u << stageIndex, timingState.StageOffsetTicks[stageIndex],
                                       timingState.StageMaxDeviationNanoseconds[stageIndex], timingState.CalibrationCount));
        }
      }
    }

    // The measurements that arrived belong to earlier frames
    for (const Vulkan::VUPresentTimingRecord& record : m_presentTimingRecords)
    {
      const FramePacingLogState::PresentFrame& presentFrame = rState.PresentFrames[record.PresentId % rState.PresentFrames.size()];
      if (presentFrame.PresentId != record.PresentId)
      {
        continue;
      }
      const std::array<std::optional<TickCount>, 4> stageTimes = {record.QueueOperationsEnd, record.RequestDequeued, record.FirstPixelOut,
                                                                  record.FirstPixelVisible};
      for (std::size_t stageIndex = 0; stageIndex < stageTimes.size(); ++stageIndex)
      {
        if (stageTimes[stageIndex].has_value())
        {
          rLog.SetLogValueAt(presentFrame.FrameIndex, rState.StageTicks[stageIndex], stageTimes[stageIndex].value());
        }
        if (record.RawStageTimes[stageIndex] != 0u)
        {
          rLog.SetLogUInt64At(presentFrame.FrameIndex, rState.StageRaw[stageIndex], record.RawStageTimes[stageIndex]);
        }
      }
      rLog.SetLogUInt64At(presentFrame.FrameIndex, rState.TimeDomainId, record.TimeDomainId);
      rLog.SetLogUInt64At(presentFrame.FrameIndex, rState.ResultReadAtFrame, rState.FrameIndex);
    }
  }


  void DemoAppVulkanBasic::SetPresentRelativeTargetTime(const TimeSpan time) noexcept
  {
    m_presentTiming.SetNextRelativeTargetTime(static_cast<uint64_t>(std::max(time.Ticks(), int64_t{0})) * TickCount::NanoSecondsPerTick);
  }


  void DemoAppVulkanBasic::LogPresent(const VkResult result, const bool timingRequested, const uint64_t relativeTargetTimeNanoseconds) noexcept
  {
    if (!m_framePacingLogState || !m_framePacingLogState->HasFrame)
    {
      return;
    }
    FramePacingLogState& rState = *m_framePacingLogState;
    IFramePacingFrameLog& rLog = *rState.Log;
    const uint64_t frameIndex = rState.FrameIndex;
    rLog.SetLogUInt64At(frameIndex, rState.PresentId, m_currentPresentCalls.PresentId);
    rLog.SetLogUInt64At(frameIndex, rState.ImageIndex, m_currentPresentCalls.ImageIndex);
    rLog.SetLogValueAt(frameIndex, rState.PresentCall, m_currentPresentCalls.PresentCallTime);
    rLog.SetLogValueAt(frameIndex, rState.PresentReturn, m_currentPresentCalls.PresentReturnTime);
    rLog.SetLogInt64At(frameIndex, rState.PresentResult, static_cast<int64_t>(result));
    rLog.SetLogInt64At(frameIndex, rState.PresentTimingRequested, timingRequested ? 1 : 0);
    if (relativeTargetTimeNanoseconds != 0u)
    {
      rLog.SetLogUInt64At(frameIndex, rState.PresentTargetRelative, relativeTargetTimeNanoseconds);
    }
    // So a measurement that arrives later finds the frame of its present
    rState.PresentFrames[m_currentPresentCalls.PresentId % rState.PresentFrames.size()] = {m_currentPresentCalls.PresentId, frameIndex};
    rState.HasFrame = false;
  }


  void DemoAppVulkanBasic::UpdateGpuMemoryStats() noexcept
  {
    // The service only wants it while somebody asks for the GPU memory usage and the operating system has no number of its own
    if (!m_systemStatsServiceControl || !m_systemStatsServiceControl->IsApplicationGpuMemoryUsageWanted())
    {
      return;
    }
    const TickCount currentTime = m_presentCallTimer.GetTimestamp();
    if (m_lastGpuMemoryStatsTime.Ticks() != 0 && (currentTime - m_lastGpuMemoryStatsTime) < LocalConfig::GpuMemoryStatsInterval)
    {
      return;
    }
    m_lastGpuMemoryStatsTime = currentTime;
    try
    {
      Vulkan::VUMemoryBudget budget;
      if (Vulkan::MemoryBudgetUtil::TryGetMemoryBudget(m_physicalDevice.Device, budget))
      {
        // The heaps of the device are the memory of the GPU, what the app uses of the other heaps is system memory the GPU uses for it
        const VkDeviceSize deviceLocalUsage = budget.GetDeviceLocalUsage();
        VkDeviceSize totalUsage = 0;
        for (uint32_t i = 0; i < budget.HeapCount; ++i)
        {
          totalUsage += budget.HeapUsage[i];
        }
        m_systemStatsServiceControl->SetApplicationGpuMemoryUsage(deviceLocalUsage, totalUsage - std::min(deviceLocalUsage, totalUsage));
      }
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_VERBOSE3("The GPU memory usage could not be read: {}", ex.what());
    }
  }


  void DemoAppVulkanBasic::SetPresentTimingRequested(const bool requested) noexcept
  {
    if (requested != m_presentTimingRequested)
    {
      m_presentTimingRequested = requested;
      // The swapchain is recreated by TryDoPrepareDraw, as it can not be done while a frame is being drawn.
      // The request only decides something if the presents can be measured and the user did not force them on.
      m_presentTimingChangePending =
        m_dependentResources.Valid && IsPresentTimingSupported() && m_launchOptions.PresentTiming != OptionUserChoice::On;
    }
  }


  bool DemoAppVulkanBasic::TryRebuildResources()
  {
    if (m_currentAppState != AppState::Ready)
    {
      return false;
    }
    FreeResources();
    BuildResources();
    return true;
  }


  bool DemoAppVulkanBasic::TryGetSwapchainInfo(SwapchainInfo& rSwapchainInfo) const
  {
    if (!m_swapchain.IsValid() || m_currentAppState != AppState::Ready || !m_frameRecord.IsValid)
    {
      FSLLOG3_WARNING_IF(!m_frameRecord.IsValid, "Not called while rendering a frame, so this will always fail");
      rSwapchainInfo = {};
      return false;
    }

    const auto frameAssignedSwapImageIndex = m_resources.Frames[m_frameRecord.FrameIndex].AssignedSwapImageIndex;

    rSwapchainInfo = SwapchainInfo(m_swapchain.GetImageExtent(), m_swapchain[frameAssignedSwapImageIndex], m_swapchain.GetImageFormat(),
                                   m_swapchain.GetImageUsageFlags());
    return true;
  }


  RapidVulkan::RenderPass DemoAppVulkanBasic::CreateBasicRenderPass()
  {
    if (!m_swapchain.IsValid())
    {
      throw UsageErrorException("Swapchain is not valid");
    }
    FSLLOG3_VERBOSE2("DemoAppVulkanBasic::CreateBasicRenderPass()");

    VkAttachmentDescription colorAttachmentDescription{};
    colorAttachmentDescription.flags = 0;
    colorAttachmentDescription.format = m_swapchain.GetImageFormat();
    colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference{};
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDependency subpassDependency{};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDescription subpassDescription{};
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments = nullptr;
    subpassDescription.pDepthStencilAttachment = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;

    if (!m_dependentResources.DepthImage.IsValid())
    {
      // Basic renderPass no depth buffer
      return {m_device.Get(), 0, 1, &colorAttachmentDescription, 1, &subpassDescription, 1, &subpassDependency};
    }

    // Ensure that the correct masks are set. The depth image is one for all frames, so with more than one frame in flight the frame
    // before can still be writing it when this one begins: the store of a depth attachment is done in the late fragment tests, and
    // the dependency has to name that stage and that write, or the layout transition of this frame is not ordered after it.
    subpassDependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    subpassDependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    subpassDependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription depthAttachmentDescription{};
    depthAttachmentDescription.format = m_dependentResources.DepthImage.Image().GetFormat();
    depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference{};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // patch -> VkSubpassDescription
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachmentDescription, depthAttachmentDescription};
    return {m_swapchain.GetDevice(), 0, static_cast<uint32_t>(attachments.size()), attachments.data(), 1, &subpassDescription, 1, &subpassDependency};
  }


  std::vector<DemoAppVulkanBasic::FrameDrawRecord> DemoAppVulkanBasic::CreateFrameSyncObjects(const VkDevice device, const uint32_t maxFramesInFlight,
                                                                                              const bool createPresentFence)
  {
    FSLLOG3_VERBOSE2("DemoAppVulkanBasic::CreateFrameSyncObjects()");

    std::vector<DemoAppVulkanBasic::FrameDrawRecord> framesDrawRecords(maxFramesInFlight);

    for (auto& rFrame : framesDrawRecords)
    {
      assert(!rFrame.HasSwapBufferImage);
      assert(rFrame.AssignedSwapImageIndex == 0);
      assert(!rFrame.ImageAcquiredSemaphore.IsValid());
      // rFrame.ImageAcquiredSemaphore.Reset(device, 0);  // We set this on demand, so we start with a empty one
      rFrame.QueueSubmitFence.Reset(device, VK_FENCE_CREATE_SIGNALED_BIT);
      if (createPresentFence)
      {
        // Must be unsignaled when given to vkQueuePresentKHR
        rFrame.PresentFence.Reset(device, 0);
      }
    }

    if (Vulkan::VUDebugUtils::IsEnabled())
    {
      for (std::size_t i = 0; i < framesDrawRecords.size(); ++i)
      {
        const FrameDrawRecord& frame = framesDrawRecords[i];
        Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_FENCE, frame.QueueSubmitFence.Get(), fmt::format("Frame{}.QueueSubmitFence", i));
        Vulkan::VUDebugUtils::SetObjectName(device, VK_OBJECT_TYPE_FENCE, frame.PresentFence.Get(), fmt::format("Frame{}.PresentFence", i));
      }
    }
    return framesDrawRecords;
  }


  // m_swapchain.GetImageExtent()
  void DemoAppVulkanBasic::BuildSwapchainImageView(SwapchainRecord& rSwapchainRecord, const uint32_t swapBufferIndex)
  {
    const VkComponentMapping
      componentMapping{};    // = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
    const VkImageSubresourceRange imageSubresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    rSwapchainRecord.SwapchainImageView.Reset(m_device.Get(), 0, m_swapchain[swapBufferIndex], VK_IMAGE_VIEW_TYPE_2D, m_swapchain.GetImageFormat(),
                                              componentMapping, imageSubresourceRange);
  }


  RapidVulkan::Framebuffer DemoAppVulkanBasic::CreateFramebuffer(const FrameBufferCreateContext& frameBufferCreateContext)
  {
    std::array<VkImageView, 2> imageViews = {frameBufferCreateContext.SwapchainImageView, frameBufferCreateContext.DepthBufferImageView};
    const uint32_t numViews = frameBufferCreateContext.DepthBufferImageView == VK_NULL_HANDLE ? 1 : 2;

    return {m_device.Get(),
            0,
            frameBufferCreateContext.RenderPass,
            numViews,
            imageViews.data(),
            frameBufferCreateContext.SwapChainImageExtent.width,
            frameBufferCreateContext.SwapChainImageExtent.height,
            1};
  }


  Vulkan::VUImageMemoryView DemoAppVulkanBasic::CreateBasicDepthImageView(const Vulkan::VUDevice& device, const VkExtent2D& depthImageExtent,
                                                                          const VkCommandPool commandPool, const VkSampleCountFlagBits sampleCount)
  {
    FSLLOG3_VERBOSE2("DemoAppVulkanBasic::CreateBasicDepthImageView()");

    const auto depthFormat = device.GetPhysicalDevice().FindDepthFormat(false);

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = depthFormat;
    imageCreateInfo.extent = {depthImageExtent.width, depthImageExtent.height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = sampleCount;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    Vulkan::VUImageMemoryView depthImageMemoryView(device, imageCreateInfo, subresourceRange, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "DepthBuffer");

    // Transition the depth buffer to a optimal layout
    {
      RapidVulkan::CommandBuffer commandBuffer(device.Get(), commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      commandBuffer.Begin(beginInfo);
      {
        Vulkan::CommandBufferUtil::SetImageLayout(commandBuffer.Get(), depthImageMemoryView.Image().Get(), VK_IMAGE_ASPECT_COLOR_BIT,
                                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange);
      }
      commandBuffer.End();
    }

    return depthImageMemoryView;
  }

  DemoAppVulkanBasic::RecreateSwapchainResult DemoAppVulkanBasic::TryRecreateSwapchain()
  {
    // Before we try to recreate things we need to ensure nothing is in use.
    if (vkDeviceWaitIdle(m_device.Get()) != VK_SUCCESS)
    {
      FSLLOG3_WARNING("Wait for device idle failed");
      return RecreateSwapchainResult::Failed;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice.Device, m_surface, &surfaceCapabilities) != VK_SUCCESS)
    {
      FSLLOG3_WARNING("Failed to get surface capabilities");
      return RecreateSwapchainResult::Failed;
    }

    if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
    {
      // Don't even try to recreate things at the moment
      return RecreateSwapchainResult::NotReady;
    }

    FSLLOG3_VERBOSE("DemoAppVulkanBasic::TryRecreateSwapchain() Recreating vulkan swapchain");

    try
    {
      FreeResources();
      BuildResources();

      if (m_graphicsServiceHost)
      {
        m_graphicsServiceHost->OnRenderSystemEvent(BasicRenderSystemEvent::SwapchainRecreated);
      }
      return RecreateSwapchainResult::Completed;
    }
    catch (const std::exception& ex)
    {
      FSLLOG3_WARNING("Failed to recreate swapchain due to: {}", ex.what());
      return RecreateSwapchainResult::Failed;
    }
  }

  // Per frame:
  // - ImageAcquiredSemaphore - Given to vkAcquireNextImageKHR, used by vkQueueSubmit (set when the presentation engine is finished using the
  // image).
  // - QueueSubmitFence       - Given to vkQueueSubmit, and its signaled once all submitted command buffers have completed execution
  // - PresentFence           - Given to vkQueuePresentKHR (only with swapchain maintenance1), its signaled once the presentation engine
  //                            no longer needs the wait semaphores. Waited for before its reused and before the semaphores are destroyed.
  //
  // Per swapchain image:
  // - ImageReleasedSemaphore - Given to vkQueueSubmit, waited on by vkQueuePresentKHR (set when the command buffer has finished executing).
  //                            The presentation engine may still use it until the image is re-acquired, so it can't be tied to the frame fence.


  AppDrawResult DemoAppVulkanBasic::TryDoPrepareDraw(const FrameInfo& frameInfo)
  {
    FSL_PARAM_NOT_USED(frameInfo);

    if (m_presentTimingChangePending && m_currentAppState == AppState::Ready)
    {
      // Measuring the presents is a property of the swapchain, so the app turning it on or off means a new swapchain
      FSLLOG3_VERBOSE("Present timing: {} by the app, recreating the swapchain", m_presentTimingRequested ? "requested" : "switched off");
      m_presentTimingChangePending = false;
      SetAppState(AppDrawResult::Retry);
    }

    if (m_swapchainSuboptimalPending && m_currentAppState == AppState::Ready)
    {
      // A acquire or a present said that the swapchain no longer matches the surface exactly. The image was still presented, that
      // result is a success. A new swapchain is only made when it would differ from this one: a surface can say suboptimal for
      // every frame for a reason a new swapchain does not cure, and that must not make one per frame.
      m_swapchainSuboptimalPending = false;
      if (IsSwapchainRecreationWorthIt())
      {
        FSLLOG3_VERBOSE("The swapchain is suboptimal ({} results) and the surface changed, recreating the swapchain", m_swapchainSuboptimalCount);
        SetAppState(AppDrawResult::Retry);
      }
    }

    if (m_currentAppState == AppState::WaitForSwapchainRecreation)
    {
      switch (TryRecreateSwapchain())
      {
      case RecreateSwapchainResult::Completed:
        break;
      case RecreateSwapchainResult::NotReady:
        return AppDrawResult::NotReady;
      case RecreateSwapchainResult::Failed:
        FSLLOG3_VERBOSE("DemoAppVulkanBasic::TryDoPrepareDraw() Failed to recreate swapchain");
        return AppDrawResult::Failed;
      default:
        throw NotSupportedException("Unsupported result");
      }
    }
    else if (m_currentAppState != AppState::Ready)
    {
      throw NotSupportedException("Unsupported app state");
    }

    const auto currentFrameIndex = frameInfo.FrameIndex;

    // The order of a frame: the app holds its start if it paces its frames, then the frame slot is waited for, then a image is
    // acquired. So no swapchain image is held while the frame waits, which matters with the few images a swapchain has.
    OnVulkanFrameStart();
    {    // Wait for the frame slot to be ready, so we know the frame resources can be reused
      const VkResult waitVkResult = WaitForFrameSlot(currentFrameIndex);
      if (waitVkResult != VK_SUCCESS)
      {
        FSLLOG3_WARNING("Waiting for the frame slot failed with: {}", RapidVulkan::Debug::ToString(waitVkResult));
        ReportDeviceLost(waitVkResult);
        return AppDrawResult::Failed;
      }
      // Ensure the present fence can be reused
      const AppDrawResult waitResult = TryWaitForPresentFence(m_resources.Frames[currentFrameIndex]);
      if (waitResult != AppDrawResult::Completed)
      {
        return waitResult;
      }
    }

    uint32_t acquiredSwapImageIndex{0};
    VkResult result = VK_SUCCESS;
    RapidVulkan::Semaphore imageAcquiredSemaphore = m_resources.AcquireSemaphore(m_device.Get());
    {
      m_currentPresentCalls.AcquireCallTime = m_presentCallTimer.GetTimestamp();
      result = vkAcquireNextImageKHR(m_device.Get(), m_swapchain.Get(), LocalConfig::DefaultTimeout, imageAcquiredSemaphore.Get(), VK_NULL_HANDLE,
                                     &acquiredSwapImageIndex);
      m_currentPresentCalls.AcquireReturnTime = m_presentCallTimer.GetTimestamp();
      m_currentPresentCalls.AcquireResult = static_cast<int32_t>(result);
    }
    if (result == VK_SUBOPTIMAL_KHR)
    {
      m_swapchainSuboptimalPending = true;
      ++m_swapchainSuboptimalCount;
    }

    switch (result)
    {
    case VK_SUBOPTIMAL_KHR:
    case VK_SUCCESS:
      {
        if (!m_useFrameTimeline)
        {
          // The fence of the frame slot was waited for before the acquire. It is reset here, now that the frame will be submitted:
          // a acquire that fails has to leave it signaled, or the next wait for it would never end.
          const VkResult resetResult = vkResetFences(m_device.Get(), 1, m_resources.Frames[currentFrameIndex].QueueSubmitFence.GetPointer());
          if (resetResult != VK_SUCCESS)
          {
            FSLLOG3_WARNING("vkResetFences failed with: {}", RapidVulkan::Debug::ToString(resetResult));
            ReportDeviceLost(resetResult);
            return AppDrawResult::Failed;
          }
        }

        {    // Patch the current frame record with the selected image
          // Check if the swapchain image is currently assigned to a frame, and if it is we wait for it
          SwapchainRecord& rSwapchainRecord = m_dependentResources.SwapchainRecords[acquiredSwapImageIndex];

          // Check if the assigned frame index was assigned to another frame and if it was we wait for that frame to finish before reclaiming the
          // swapchain record.
          if (AppSetup.WaitForLastUseOfSwapchainImage && rSwapchainRecord.HasAssignedFrame &&
              rSwapchainRecord.AssignedFrameIndex != currentFrameIndex)
          {
            // FSLLOG3_INFO("Remapping frameIndex {} to image previously used in frameIndex{}", currentFrameIndex,
            // rSwapchainRecord.AssignedFrameIndex);
            const FrameDrawRecord& rOldFrame = m_resources.Frames[rSwapchainRecord.AssignedFrameIndex];

            // We only wait for the other frames fence (and it will be up to the frame to reset it once we get to it)
            const VkResult waitResult = WaitForFrameSlot(rSwapchainRecord.AssignedFrameIndex);
            if (waitResult != VK_SUCCESS)
            {
              FSLLOG3_WARNING("Waiting for the frame slot that used the image failed with: {}", RapidVulkan::Debug::ToString(waitResult));
              ReportDeviceLost(waitResult);
              return AppDrawResult::Failed;
            }
            rSwapchainRecord.HasAssignedFrame = false;
          }

          // Since we made sure that no frame is associated with the acquiredSwapImageIndex we can simply just assign it to this frame.
          // We also mark the swapchain record with the current frameIndex so we can do a quick lookup of the previously assigned frameIndex the
          // next time we need the same swapImageIndex

          FrameDrawRecord& rFrame = m_resources.Frames[currentFrameIndex];
          rFrame.HasSwapBufferImage = true;
          rFrame.AssignedSwapImageIndex = acquiredSwapImageIndex;
          rSwapchainRecord.HasAssignedFrame = true;
          rSwapchainRecord.AssignedFrameIndex = currentFrameIndex;

          // Release the old image acquired semaphore if available
          if (rFrame.ImageAcquiredSemaphore.IsValid())
          {
            m_resources.ReleaseSemaphore(std::move(rFrame.ImageAcquiredSemaphore));
          }
          // Store the new one
          rFrame.ImageAcquiredSemaphore = std::move(imageAcquiredSemaphore);
        }

        return AppDrawResult::Completed;
      }
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_SURFACE_LOST_KHR:
      return AppDrawResult::Retry;
    default:
      // This restarts the app and demo host, so make sure the reason can be found in the log
      FSLLOG3_ERROR("vkAcquireNextImageKHR failed with: {}", RapidVulkan::Debug::ToString(result));
      ReportDeviceLost(result);
      return AppDrawResult::Failed;
    }
  }


  AppDrawResult DemoAppVulkanBasic::TryDoSwapBuffers(const FrameInfo& frameInfo)
  {
    FSL_PARAM_NOT_USED(frameInfo);

    FrameDrawRecord& rFrame = m_resources.Frames[frameInfo.FrameIndex];
    const VkSemaphore signalSemaphore = m_dependentResources.SwapchainRecords[rFrame.AssignedSwapImageIndex].ImageReleasedSemaphore.Get();

    const void* pPresentInfoNext = nullptr;
#ifdef FSL_VULKAN_SWAPCHAIN_MAINTENANCE1_SUPPORTED
    Vulkan::SwapchainMaintenance1Util::SwapchainPresentFenceInfo presentFenceInfo{};
    if (rFrame.PresentFence.IsValid())
    {
      // Normally a no-op since TryDoPrepareDraw already did this
      const AppDrawResult waitResult = TryWaitForPresentFence(rFrame);
      if (waitResult != AppDrawResult::Completed)
      {
        return waitResult;
      }
      presentFenceInfo.sType = Vulkan::SwapchainMaintenance1Util::SwapchainPresentFenceInfoSType;
      presentFenceInfo.swapchainCount = 1;
      presentFenceInfo.pFences = rFrame.PresentFence.GetPointer();
      pPresentInfoNext = &presentFenceInfo;
    }
#endif

    const bool hasPresentFence = pPresentInfoNext != nullptr;

    // Number the present and, if enabled, ask for it to be measured
    ++m_presentCounter;
    Vulkan::VUPresentTimingPresentInfo presentTimingInfo;
    pPresentInfoNext = m_presentTiming.PreparePresent(presentTimingInfo, m_presentCounter, pPresentInfoNext);

    m_currentPresentCalls.PresentId = m_presentCounter;
    m_currentPresentCalls.ImageIndex = rFrame.AssignedSwapImageIndex;
    m_currentPresentCalls.PresentCallTime = m_presentCallTimer.GetTimestamp();
    const auto result =
      m_swapchain.TryQueuePresent(m_deviceQueue.Queue, 1, &signalSemaphore, &rFrame.AssignedSwapImageIndex, nullptr, pPresentInfoNext);
    m_currentPresentCalls.PresentReturnTime = m_presentCallTimer.GetTimestamp();
    m_lastPresentCalls = m_currentPresentCalls;
    LogPresent(result, presentTimingInfo.IsTimingRequested, presentTimingInfo.RelativeTargetTimeNanoseconds);
    rFrame.PresentFencePending = hasPresentFence && IsPresentFenceSignalExpected(result);
    m_presentTiming.OnPresent(presentTimingInfo, result);

    switch (result)
    {
    case VK_SUBOPTIMAL_KHR:
      // The image was presented. If a new swapchain would do better is looked at before the next acquire.
      m_swapchainSuboptimalPending = true;
      ++m_swapchainSuboptimalCount;
      [[fallthrough]];
    case VK_SUCCESS:
      return AppDrawResult::Completed;
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_ERROR_SURFACE_LOST_KHR:
      // Simply try to recreate the swapchain
      return AppDrawResult::Retry;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    case VK_ERROR_DEVICE_LOST:
    default:
      // This case should restart the app and demo host, so make sure the reason can be found in the log
      FSLLOG3_ERROR("vkQueuePresentKHR failed with: {}", RapidVulkan::Debug::ToString(result));
      ReportDeviceLost(result);
      return AppDrawResult::Failed;
    }
  }


  AppDrawResult DemoAppVulkanBasic::TryWaitForPresentFence(FrameDrawRecord& rFrame)
  {
    if (!rFrame.PresentFencePending)
    {
      return AppDrawResult::Completed;
    }
    rFrame.PresentFencePending = false;
    VkResult waitVkResult = VK_SUCCESS;
    const AppDrawResult waitResult = WaitForFenceAndResetIt(m_device.Get(), rFrame.PresentFence.Get(), waitVkResult);
    ReportDeviceLost(waitVkResult);
    return waitResult;
  }


  void DemoAppVulkanBasic::SetAppState(AppDrawResult result)
  {
    switch (result)
    {
    case AppDrawResult::Completed:
      FSLLOG3_VERBOSE_IF(m_currentAppState != AppState::Ready, "DemoAppVulkanBasic::SetAppState(): Ready");
      m_currentAppState = AppState::Ready;
      break;
    case AppDrawResult::Failed:
    case AppDrawResult::NotReady:
    case AppDrawResult::Retry:
    default:
      if (m_currentAppState != AppState::WaitForSwapchainRecreation)
      {
        FSLLOG3_VERBOSE("DemoAppVulkanBasic::SetAppState(): WaitForSwapchainRecreation");
        if (m_graphicsServiceHost)
        {
          if (m_device.IsValid())
          {
            FSLLOG3_VERBOSE4("DemoAppVulkanBasic::SetAppState(): Swapchain lost, waiting for device to be idle");
            vkDeviceWaitIdle(m_device.Get());
          }
          // Give the native graphics service a chance to do some processing when this occurs
          m_graphicsServiceHost->OnRenderSystemEvent(BasicRenderSystemEvent::SwapchainLost);
        }
      }
      m_currentAppState = AppState::WaitForSwapchainRecreation;
      break;
    }
  }
}
