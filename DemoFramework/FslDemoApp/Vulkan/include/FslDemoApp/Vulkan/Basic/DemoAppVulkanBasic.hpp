#ifndef FSLDEMOAPP_VULKAN_BASIC_DEMOAPPVULKANBASIC_HPP
#define FSLDEMOAPP_VULKAN_BASIC_DEMOAPPVULKANBASIC_HPP
/****************************************************************************************************************************************************
 * Copyright 2018 NXP
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

#include <FslBase/Span/ReadOnlySpan.hpp>
#include <FslBase/System/HighResolutionTimer.hpp>
#include <FslDemoApp/Vulkan/Basic/BuildResourcesContext.hpp>
#include <FslDemoApp/Vulkan/Basic/DemoAppVulkanSetup.hpp>
#include <FslDemoApp/Vulkan/Basic/DrawContext.hpp>
#include <FslDemoApp/Vulkan/Basic/FrameBufferCreateContext.hpp>
#include <FslDemoApp/Vulkan/Basic/PresentCallRecord.hpp>
#include <FslDemoApp/Vulkan/Basic/SwapchainInfo.hpp>
#include <FslDemoApp/Vulkan/DemoAppVulkan.hpp>
#include <FslUtil/Vulkan1_0/SurfaceFormatInfo.hpp>
#include <FslUtil/Vulkan1_0/VUImageMemoryView.hpp>
#include <FslUtil/Vulkan1_0/VUSwapchainKHR.hpp>
#include <FslUtil/Vulkan1_0/VUSwapchainPresentTiming.hpp>
#include <RapidVulkan/CommandBuffers.hpp>
#include <RapidVulkan/CommandPool.hpp>
#include <RapidVulkan/Fence.hpp>
#include <RapidVulkan/Framebuffer.hpp>
#include <RapidVulkan/ImageView.hpp>
#include <RapidVulkan/RenderPass.hpp>
#include <RapidVulkan/Semaphore.hpp>
#include <vulkan/vulkan.h>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

namespace Fsl
{
  class DemoAppHostConfigVulkan;
  class DemoAppProfilerOverlay;
  class IFramePacingOverlay;
  class ISystemStatsServiceControl;

  namespace Vulkan
  {
    struct NativeGraphicsSwapchainInfo;
  }

  namespace VulkanBasic
  {
    class DemoAppVulkanBasic : public DemoAppVulkan
    {
    private:
      enum class AppState
      {
        // The app is ready to render
        Ready,
        // We are waiting for the swapchain to be re-creatable.
        // This can occur on windows when the app is minimized and the vkGetPhysicalDeviceSurfaceCapabilitiesKHR call can
        // return a surface with a max extent of zero.
        WaitForSwapchainRecreation
      };

      enum class RecreateSwapchainResult
      {
        Failed,
        NotReady,
        Completed    // Named Complete because we can't use Success as the X11 header might define it :(
      };

      //! We overload the move operator and constructor to ensure we destroy resources in destruction order.
      struct FrameDrawRecord
      {
        RapidVulkan::Semaphore ImageAcquiredSemaphore;
        RapidVulkan::Fence QueueSubmitFence;
        //! Given to vkQueuePresentKHR (only valid if swapchain maintenance1 is enabled)
        RapidVulkan::Fence PresentFence;
        uint32_t AssignedSwapImageIndex{0};
        bool HasSwapBufferImage{false};
        //! True if PresentFence was given to a present operation that has not been waited for yet
        bool PresentFencePending{false};

        FrameDrawRecord() noexcept = default;

        FrameDrawRecord(const FrameDrawRecord&) = delete;
        FrameDrawRecord& operator=(const FrameDrawRecord&) = delete;

        FrameDrawRecord(FrameDrawRecord&& other) noexcept
          : ImageAcquiredSemaphore(std::move(other.ImageAcquiredSemaphore))
          , QueueSubmitFence(std::move(other.QueueSubmitFence))
          , PresentFence(std::move(other.PresentFence))
          , AssignedSwapImageIndex(other.AssignedSwapImageIndex)
          , HasSwapBufferImage(other.HasSwapBufferImage)
          , PresentFencePending(other.PresentFencePending)
        {
          other.AssignedSwapImageIndex = 0;
          other.HasSwapBufferImage = false;
          other.PresentFencePending = false;
        }

        FrameDrawRecord& operator=(FrameDrawRecord&& other) noexcept
        {
          if (this != &other)
          {
            Reset();

            ImageAcquiredSemaphore = std::move(other.ImageAcquiredSemaphore);
            QueueSubmitFence = std::move(other.QueueSubmitFence);
            PresentFence = std::move(other.PresentFence);
            AssignedSwapImageIndex = other.AssignedSwapImageIndex;
            HasSwapBufferImage = other.HasSwapBufferImage;
            PresentFencePending = other.PresentFencePending;

            other.AssignedSwapImageIndex = 0;
            other.HasSwapBufferImage = false;
            other.PresentFencePending = false;
          }
          return *this;
        }
        ~FrameDrawRecord() = default;

        void Reset() noexcept
        {
          // Reset in destruction order
          PresentFencePending = false;
          HasSwapBufferImage = false;
          AssignedSwapImageIndex = 0;
          PresentFence.Reset();
          QueueSubmitFence.Reset();
          ImageAcquiredSemaphore.Reset();
        }
      };

      //! We overload the move operator and constructor to ensure we destroy resources in destruction order.
      struct SwapchainRecord
      {
        RapidVulkan::ImageView SwapchainImageView;
        RapidVulkan::Framebuffer Framebuffer;
        //! Signaled when rendering to this image completes, waited on by vkQueuePresentKHR.
        //! This is per swapchain image (not per frame) since the presentation engine can still be using it until the image is re-acquired.
        RapidVulkan::Semaphore ImageReleasedSemaphore;
        //! This is the frame index it was assigned to
        uint32_t AssignedFrameIndex{0};
        //! This is true if this swapchain record has been assigned to a frame index
        bool HasAssignedFrame{false};

        SwapchainRecord() noexcept = default;

        SwapchainRecord(const SwapchainRecord&) = delete;
        SwapchainRecord& operator=(const SwapchainRecord&) = delete;

        SwapchainRecord(SwapchainRecord&& other) noexcept
          : SwapchainImageView(std::move(other.SwapchainImageView))
          , Framebuffer(std::move(other.Framebuffer))
          , ImageReleasedSemaphore(std::move(other.ImageReleasedSemaphore))
          , AssignedFrameIndex(other.AssignedFrameIndex)
          , HasAssignedFrame(other.HasAssignedFrame)
        {
          other.AssignedFrameIndex = 0;
          other.HasAssignedFrame = false;
        }

        SwapchainRecord& operator=(SwapchainRecord&& other) noexcept
        {
          if (this != &other)
          {
            Reset();

            SwapchainImageView = std::move(other.SwapchainImageView);
            Framebuffer = std::move(other.Framebuffer);
            ImageReleasedSemaphore = std::move(other.ImageReleasedSemaphore);
            AssignedFrameIndex = other.AssignedFrameIndex;
            HasAssignedFrame = other.HasAssignedFrame;

            other.AssignedFrameIndex = 0;
            other.HasAssignedFrame = false;
          }
          return *this;
        }
        ~SwapchainRecord() = default;

        void Reset() noexcept
        {
          // Reset in destruction order
          HasAssignedFrame = false;
          AssignedFrameIndex = 0;
          ImageReleasedSemaphore.Reset();
          Framebuffer.Reset();
          SwapchainImageView.Reset();
        }
      };

      struct Resources
      {
        RapidVulkan::CommandPool MainCommandPool;

      private:
        std::vector<RapidVulkan::Semaphore> m_recycledSemaphores;

      public:
        std::vector<FrameDrawRecord> Frames;

        Resources() noexcept = default;
        Resources(const Resources&) = delete;
        Resources& operator=(const Resources&) = delete;
        Resources(Resources&& other) noexcept = delete;
        Resources& operator=(Resources&& other) noexcept = delete;
        ~Resources() = default;

        void Reset() noexcept
        {
          // Reset in destruction order
          Frames.clear();
          m_recycledSemaphores.clear();
          MainCommandPool.Reset();
        }

        RapidVulkan::Semaphore AcquireSemaphore(VkDevice device)
        {
          if (!m_recycledSemaphores.empty())
          {
            auto tmp = std::move(m_recycledSemaphores.back());
            m_recycledSemaphores.pop_back();
            return tmp;
          }
          return {device, 0};
        }

        void ReleaseSemaphore(RapidVulkan::Semaphore&& value)
        {
          m_recycledSemaphores.push_back(std::move(value));
          // If this fires its likely we have a error
          assert(m_recycledSemaphores.size() < 1024);
        }
      };

      struct FrameRecord
      {
        bool IsValid{false};
        uint32_t FrameIndex{0};

        FrameRecord() noexcept = default;

        explicit FrameRecord(const uint32_t frameIndex) noexcept
          : IsValid(true)
          , FrameIndex(frameIndex)
        {
        }
      };

      struct DependentResources
      {
        bool Valid = false;
        uint32_t FramesInFlightCount{0};
        //! A optional Depth image, only enabled if requested
        //! We only need one RenderAttachment as command buffers on the same queue are executed in order
        Vulkan::VUImageMemoryView DepthImage;
        RapidVulkan::CommandBuffers CmdBuffers;
        std::vector<SwapchainRecord> SwapchainRecords;
        std::shared_ptr<Vulkan::NativeGraphicsSwapchainInfo> NGScreenshotLink;

        DependentResources() = default;
        DependentResources(const DependentResources&) = delete;
        DependentResources& operator=(const DependentResources&) = delete;
        DependentResources(DependentResources&& other) noexcept = delete;
        DependentResources& operator=(DependentResources&& other) noexcept = delete;
        ~DependentResources() = default;

        void Reset() noexcept
        {
          // Reset in destruction order

          NGScreenshotLink.reset();
          SwapchainRecords.clear();
          CmdBuffers.Reset();
          DepthImage.Reset();
          FramesInFlightCount = 0;
          Valid = false;
        }
      };

      std::shared_ptr<DemoAppHostConfigVulkan> m_demoHostConfig;
      Vulkan::SurfaceFormatInfo m_surfaceFormatInfo;

      const DemoAppVulkanSetup AppSetup;
      Resources m_resources;
      Vulkan::VUSwapchainKHR m_swapchain;
      DependentResources m_dependentResources;
      FrameRecord m_frameRecord;

      AppState m_currentAppState = AppState::Ready;
      std::unique_ptr<DemoAppProfilerOverlay> m_demoAppProfilerOverlay;
      //! Null if the frame pacing service is unavailable
      std::shared_ptr<IFramePacingOverlay> m_framePacingOverlay;
      PxExtent2D m_cachedExtentPx;
      //! Measures when the swapchain images were presented (only enabled if requested and supported)
      Vulkan::VUSwapchainPresentTiming m_presentTiming;
      //! The measurements that became available since the previous frame
      std::vector<Vulkan::VUPresentTimingRecord> m_presentTimingRecords;
      //! The id of the last present (the presents are numbered from one)
      uint64_t m_presentCounter{0};
      //! True if the app wants its presents measured (it starts as DemoAppVulkanSetup::PresentTiming)
      bool m_presentTimingRequested{false};
      //! True if m_presentTimingRequested changed since the swapchain was created
      bool m_presentTimingChangePending{false};
      HighResolutionTimer m_presentCallTimer;
      //! The swapchain calls of the frame being drawn and of the last frame that was presented
      PresentCallRecord m_currentPresentCalls;
      PresentCallRecord m_lastPresentCalls;
      //! Null if the system stats service is unavailable
      std::shared_ptr<ISystemStatsServiceControl> m_systemStatsServiceControl;
      //! What is added to the frame pacing log (null if the frames are not logged)
      struct FramePacingLogState;
      std::unique_ptr<FramePacingLogState> m_framePacingLogState;
      //! When the system stats service was last told the GPU memory usage of the app
      TickCount m_lastGpuMemoryStatsTime;

    protected:
      explicit DemoAppVulkanBasic(const DemoAppConfig& demoAppConfig, const DemoAppVulkanSetup& demoAppVulkanSetup = {});

    public:
      ~DemoAppVulkanBasic() override;

    protected:
      void OnConstructed() override;
      void OnDestroy() override;

      AppDrawResult TryPrepareDraw(const FrameInfo& frameInfo) override;

    public:
      void _BeginDraw(const FrameInfo& frameInfo) override;
      void _EndDraw(const FrameInfo& frameInfo) override;

    protected:
      void ConfigurationChanged(const DemoWindowMetrics& windowMetrics) override;
      void Draw(const FrameInfo& frameInfo) final;
      AppDrawResult TrySwapBuffers(const FrameInfo& frameInfo) override;

      void AddSystemUI(VkCommandBuffer hCmdBuffer, const uint32_t frameIndex);

      void BuildResources();
      void FreeResources();

      //! Check if the dependent resources are currently allocated
      [[nodiscard]] bool IsResourcesAllocated() const
      {
        return m_dependentResources.Valid;
      }

      [[nodiscard]] VkCommandPool GetCommandPool() const
      {
        return m_resources.MainCommandPool.Get();
      }

      bool TryRebuildResources();

      //! @brief Called after the swapchain has been created
      //! @return Must return the main RenderPass.
      virtual VkRenderPass OnBuildResources(const BuildResourcesContext& context) = 0;

      //! @brief Called after the swapchain ImageView and FrameBuffers has been deleted
      virtual void OnFreeResources()
      {
      }

      virtual void VulkanDraw(const DemoTime& demoTime, RapidVulkan::CommandBuffers& rCmdBuffers, const DrawContext& drawContext) = 0;

      //! @brief get the swapchain image count
      [[nodiscard]] uint32_t GetSwapchainImageCount() const
      {
        if (!m_swapchain.IsValid() || m_currentAppState != AppState::Ready)
        {
          return 0;
        }
        return m_swapchain.GetImageCount();
      }

      bool TryGetSwapchainInfo(SwapchainInfo& rSwapchainInfo) const;

      //! @brief Call this to create a very basic render pass. This will only be useful for the most basic of applications and
      //         to quickly get something on the screen to get started.
      RapidVulkan::RenderPass CreateBasicRenderPass();

      virtual RapidVulkan::Framebuffer CreateFramebuffer(const FrameBufferCreateContext& frameBufferCreateContext);

      //! @brief Call this to crate a 'basic' depth image view.
      //         The created depth image view will be in the VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL layout.
      static Vulkan::VUImageMemoryView CreateBasicDepthImageView(const Vulkan::VUDevice& device, const VkExtent2D& depthImageExtent,
                                                                 const VkCommandPool commandPool,
                                                                 const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);

      [[nodiscard]] const Vulkan::SurfaceFormatInfo& GetSurfaceFormatInfo() const
      {
        return m_surfaceFormatInfo;
      }

      //! @brief Check if the presents can be measured: the device supports VK_EXT_present_timing and the user did not disable it.
      //!        The surface has a say as well, so use IsPresentTimingEnabled to see if they are being measured.
      [[nodiscard]] bool IsPresentTimingSupported() const noexcept
      {
        return m_hostDeviceFeatures.PresentTiming && m_launchOptions.PresentTiming != OptionUserChoice::Off;
      }

      [[nodiscard]] bool IsPresentTimingRequested() const noexcept
      {
        return m_presentTimingRequested;
      }

      //! @brief Ask for the presents to be measured or not (it starts as DemoAppVulkanSetup::PresentTiming).
      //!        Present timing is a property of the swapchain, so a change recreates the swapchain before the next frame is drawn.
      //! @note  Call it from Update. With '--VkPresentTiming true' the presents are measured no matter what is requested.
      void SetPresentTimingRequested(const bool requested) noexcept;

      //! @brief Check if the presents are being measured. It is requested with DemoAppVulkanSetup::PresentTiming (or '--VkPresentTiming true')
      //!        and needs a device and a surface that support VK_EXT_present_timing, so it can change when the swapchain is recreated.
      [[nodiscard]] bool IsPresentTimingEnabled() const noexcept
      {
        return m_presentTiming.IsEnabled();
      }

      //! @brief Get the id the present of the frame being drawn will get. The presents are numbered from one, whether they are measured or not.
      [[nodiscard]] uint64_t GetNextPresentId() const noexcept
      {
        return m_presentCounter + 1;
      }

      //! @brief Get when the swapchain was called for the last frame that was presented (its PresentId is zero if there is none).
      //!        It is always available, as it needs no extension.
      [[nodiscard]] const PresentCallRecord& GetLastPresentCalls() const noexcept
      {
        return m_lastPresentCalls;
      }

      //! @brief Get the present measurements that became available since the previous frame (empty if IsPresentTimingEnabled is false).
      //!        A measurement arrives a few frames after its present, match it to a frame with its PresentId.
      [[nodiscard]] ReadOnlySpan<Vulkan::VUPresentTimingRecord> GetPresentTimings() const noexcept
      {
        return ReadOnlySpan<Vulkan::VUPresentTimingRecord>(m_presentTimingRecords.data(), m_presentTimingRecords.size());
      }

      //! @brief Get the duration of a refresh cycle of the display as measured by the swapchain (zero if not known or not enabled).
      [[nodiscard]] TimeSpan GetPresentRefreshDuration() const noexcept
      {
        return m_presentTiming.GetRefreshDuration();
      }

    private:
      static std::vector<FrameDrawRecord> CreateFrameSyncObjects(const VkDevice device, const uint32_t maxFramesInFlight,
                                                                 const bool createPresentFence);
      //! Wait for the frames present fence (if pending) and reset it
      AppDrawResult TryWaitForPresentFence(FrameDrawRecord& rFrame);
      //! Submit the command buffer of the frame
      void SubmitFrame(const FrameDrawRecord& frameRecord, const SwapchainRecord& swapchainRecord, const uint32_t currentFrameIndex);
      void BuildSwapchainImageView(SwapchainRecord& rSwapchainRecord, const uint32_t swapBufferIndex);

      RecreateSwapchainResult TryRecreateSwapchain();
      //! Tell the system stats service how much GPU memory the app uses, if it asks for it (VK_EXT_memory_budget)
      void UpdateGpuMemoryStats() noexcept;
      //! The frame pacing log: what the swapchain is, the values of the frame that begins and the present that was just made
      void LogSwapchainCreated(const VkPresentModeKHR presentMode, const VkSwapchainCreateFlagsKHR createFlags);
      void LogFrameBegin();
      void LogPresent(const VkResult result, const bool timingRequested) noexcept;

      AppDrawResult TryDoPrepareDraw(const FrameInfo& frameInfo);
      AppDrawResult TryDoSwapBuffers(const FrameInfo& frameInfo);
      void SetAppState(AppDrawResult result);
    };
  }
}

#endif
