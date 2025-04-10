/****************************************************************************************************************************************************
 * Copyright 2021-2022, 2024 NXP
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

#include <FslBase/Math/Pixel/TypeConverter.hpp>
#include <FslBase/Span/SpanUtil_Vector.hpp>
#include <FslDemoApp/Base/Service/DemoAppControl/IDemoAppControl.hpp>
#include <FslSimpleUI/App/DemoPerformanceCapture.hpp>
#include <FslSimpleUI/App/UIDemoAppExtensionCreateInfo.hpp>
#include <FslSimpleUI/Base/UIColors.hpp>
#include <Shared/UI/Benchmark/App/CustomUIDemoAppExtension.hpp>
#include <Shared/UI/Benchmark/App/CustomWindowInfoModuleProxy.hpp>
#include <Shared/UI/Benchmark/App/ITestApp.hpp>
#include <Shared/UI/Benchmark/App/ITestAppFactory.hpp>
#include <Shared/UI/Benchmark/App/TestAppHost.hpp>
#include <Shared/UI/Benchmark/UIModule/ExternalModuleIdConfig.hpp>
#include <Shared/UI/Benchmark/UIModule/ICustomWindowInfoModule.hpp>
#include <Shared/UI/Benchmark/UIModule/WindowIdGeneratorModule.hpp>
#include <Shared/UI/Benchmark/UIModule/WindowIdGeneratorModuleFactory.hpp>
#include <cassert>

namespace Fsl
{
  namespace
  {
    namespace LocalConfig
    {
      // Since we use a lot of overlapping UI in this sample we pre-allocate capacity for a lot as well
      constexpr UIDemoAppRenderCapacityInfo DefaultRenderCapacity(4 * 50000, 6 * 50000);
    }

    std::vector<std::shared_ptr<UI::IExternalModuleFactory>> CreateExternalModules(const bool useCustomModule)
    {
      return useCustomModule ? std::vector<std::shared_ptr<UI::IExternalModuleFactory>>{std::make_shared<WindowIdGeneratorModuleFactory>()}
                             : std::vector<std::shared_ptr<UI::IExternalModuleFactory>>();
    }
  }

  TestAppHost::TestAppHost(const ServiceProvider& serviceProvider, const DemoWindowMetrics& windowMetrics, const AppBenchmarkScene benchmarkScene,
                           const bool useCustomModule)
    : m_serviceProvider(serviceProvider)
    , m_config(false, 0, PxViewport(0, 0, windowMetrics.GetSizePx().Width(), windowMetrics.GetSizePx().Height()), false, {})
    , m_realWindowMetrics(windowMetrics)
    , m_appWindowMetrics(windowMetrics)
    , m_benchmarkScene(benchmarkScene)
    , m_demoAppControl(m_serviceProvider.Get<IDemoAppControl>())
    , m_demoPerformanceCapture(std::make_shared<DemoPerformanceCapture>())
    , m_externalModules(CreateExternalModules(useCustomModule))
    , m_windowInfoProxy(std::make_shared<CustomWindowInfoModuleProxy>())
  {
  }


  TestAppHost::~TestAppHost() = default;


  UI::UIColor TestAppHost::GetRootColor() const
  {
    return m_appRecord.TestApp ? m_appRecord.TestApp->GetRootColor() : UI::UIColors::White();
  }


  bool TestAppHost::TrySetRootColor(const UI::UIColor color)
  {
    if (!m_appRecord.TestApp)
    {
      return false;
    }
    return m_appRecord.TestApp->TrySetRootColor(color);
  }


  void TestAppHost::ClearProfileData()
  {
    m_demoPerformanceCapture->Clear();
    m_demoPerformanceCapture->CalcRoundTripTime();
  }


  const UI::IRenderSystemBase& TestAppHost::GetRenderSystem() const
  {
    assert(m_appRecord.TestApp);
    return m_appRecord.TestApp->GetRenderSystem();
  }

  UI::IRenderSystemBase* TestAppHost::TryGetRenderSystem()
  {
    return m_appRecord.TestApp ? m_appRecord.TestApp->TryGetRenderSystem() : nullptr;
  }


  bool TestAppHost::IsUIIdle() const
  {
    return !m_appRecord.TestApp || m_appRecord.TestApp->IsUIIdle();
  }


  void TestAppHost::StartTestApp(ITestAppFactory& testAppFactory, const UIDemoAppMaterialCreateInfo& materialCreateInfo,
                                 const UIDemoAppMaterialConfig& materialConfig)
  {
    if (m_appRecord.TestApp)
    {
      StopTestApp();
    }

    UIDemoAppRenderCreateInfo renderCreateInfo(UI::UIColorSpace::SRGBNonLinear, LocalConfig::DefaultRenderCapacity, materialCreateInfo,
                                               materialConfig);
    m_appRecord.TestApp = testAppFactory.Create(UIDemoAppExtensionCreateInfo(m_serviceProvider, m_appWindowMetrics, renderCreateInfo,
                                                                             m_demoPerformanceCapture, SpanUtil::AsReadOnlySpan(m_externalModules)),
                                                m_benchmarkScene);
    m_appRecord.TestApp->SetCustomViewport(m_config.AppViewportPx);
    m_appRecord.TestApp->SetClipRectangle(m_config.AppClipEnabled, m_config.AppClipRectanglePx);
    assert(m_appRecord.TestApp);
    m_appRecord.DemoExtension = m_appRecord.TestApp->GetCustomUIDemoAppExtension();
    if (!m_externalModules.empty())
    {
      m_appRecord.CustomWindowModule =
        m_appRecord.DemoExtension->GetExternalModule<WindowIdGeneratorModule>(ExternalModuleIdConfig::WindowIdGeneratorModule);
      m_windowInfoProxy->Set(m_appRecord.CustomWindowModule);
    }
    m_demoAppControl->RequestUpdateTimerReset();
  }


  void TestAppHost::StopTestApp()
  {
    m_windowInfoProxy->Clear();
    m_appRecord = {};
    m_demoAppControl->RequestUpdateTimerReset();
  }


  void TestAppHost::SetAppRectangle(const PxRectangle& appRectPx)
  {
    if (appRectPx == m_config.AppViewportPx.GetBounds())
    {
      return;
    }
    m_config.AppViewportPx = PxViewport::SetBounds(m_config.AppViewportPx, appRectPx);
    if (m_appRecord.TestApp)
    {
      m_appRecord.TestApp->SetCustomViewport(m_config.AppViewportPx);
    }
    ConfigurationChanged(m_realWindowMetrics);
  }


  void TestAppHost::SetAppClipRectangle(const bool enabled, const PxRectangle& clipRectanglePx)
  {
    if (m_config.AppClipEnabled == enabled && clipRectanglePx == m_config.AppClipRectanglePx)
    {
      return;
    }
    m_config.AppClipEnabled = enabled;
    m_config.AppClipRectanglePx = clipRectanglePx;
    if (m_appRecord.TestApp)
    {
      m_appRecord.TestApp->SetClipRectangle(m_config.AppClipEnabled, m_config.AppClipRectanglePx);
    }
  }


  void TestAppHost::AppUpdate(const DemoTime& demoTime)
  {
    if (m_appRecord.TestApp)
    {
      m_appRecord.TestApp->Update(demoTime);
    }
  }


  void TestAppHost::AppDraw(const DemoTime& demoTime)
  {
    FSL_PARAM_NOT_USED(demoTime);
    if (m_appRecord.TestApp)
    {
      m_appRecord.TestApp->Draw();
    }
  }

  void TestAppHost::AppOnDrawSkipped(const FrameInfo& frameInfo)
  {
    FSL_PARAM_NOT_USED(frameInfo);
    if (m_appRecord.TestApp)
    {
      m_appRecord.TestApp->OnDrawSkipped();
    }
  }


  const UIDemoAppMaterialConfig& TestAppHost::GetDefaultMaterialConfig() const
  {
    assert(m_appRecord.DemoExtension);
    return m_appRecord.DemoExtension->GetDefaultMaterialConfig();
  }

  void TestAppHost::SetDefaultMaterialConfig(const UIDemoAppMaterialConfig& config)
  {
    assert(m_appRecord.DemoExtension);
    m_appRecord.DemoExtension->SetDefaultMaterialConfig(config);
  }


  void TestAppHost::SetEmulatedDpi(const bool emulateDpiEnabled, const uint16_t emulatedDpi)
  {
    if (m_config.EmulateDpiEnabled == emulateDpiEnabled && m_config.EmulatedDpi == emulatedDpi)
    {
      return;
    }
    m_config.EmulateDpiEnabled = emulateDpiEnabled;
    m_config.EmulatedDpi = emulatedDpi;
    ConfigurationChanged(m_realWindowMetrics);
  }


  uint16_t TestAppHost::GetTexDpi() const
  {
    assert(m_appRecord.DemoExtension);
    return m_appRecord.DemoExtension->GetDefaultMaterialDpi();
  }


  void TestAppHost::SetUseDrawCache(const bool useDrawCache)
  {
    if (m_appRecord.TestApp)
    {
      m_appRecord.TestApp->SetUseDrawCache(useDrawCache);
    }
  }


  std::shared_ptr<ICustomWindowInfoModule> TestAppHost::GetWindowInfoModule() const
  {
    return m_windowInfoProxy;
  }


  void TestAppHost::OnKeyEvent(const KeyEvent& event)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->OnKeyEvent(event);
    }
  }

  void TestAppHost::OnMouseButtonEvent(const MouseButtonEvent& event)
  {
    if (m_appRecord.DemoExtension)
    {
      MouseButtonEvent fakeEvent(event.GetTimestamp(), event.GetButton(), event.IsPressed(), event.GetPosition() - m_config.AppViewportPx.Location(),
                                 event.IsTouch());
      m_appRecord.DemoExtension->OnMouseButtonEvent(fakeEvent);
      if (fakeEvent.IsHandled())
      {
        event.Handled();
      }
    }
  }

  void TestAppHost::OnMouseMoveEvent(const MouseMoveEvent& event)
  {
    if (m_appRecord.DemoExtension)
    {
      MouseMoveEvent fakeEvent(event.GetTimestamp(), event.GetPosition() - m_config.AppViewportPx.Location(), event.GetMouseButtonFlags(),
                               event.IsTouch());
      m_appRecord.DemoExtension->OnMouseMoveEvent(fakeEvent);
      if (fakeEvent.IsHandled())
      {
        event.Handled();
      }
    }
  }

  void TestAppHost::OnMouseWheelEvent(const MouseWheelEvent& event)
  {
    if (m_appRecord.DemoExtension)
    {
      MouseWheelEvent fakeEvent(event.GetTimestamp(), event.GetDelta(), event.GetPosition() - m_config.AppViewportPx.Location());
      m_appRecord.DemoExtension->OnMouseWheelEvent(fakeEvent);
      if (fakeEvent.IsHandled())
      {
        event.Handled();
      }
    }
  }

  void TestAppHost::OnRawMouseMoveEvent(const RawMouseMoveEvent& event)
  {
    if (m_appRecord.DemoExtension)
    {
      RawMouseMoveEvent fakeEvent(event.GetTimestamp(), event.GetPosition() - m_config.AppViewportPx.Location(), event.GetMouseButtonFlags());
      m_appRecord.DemoExtension->OnRawMouseMoveEvent(fakeEvent);
      if (fakeEvent.IsHandled())
      {
        event.Handled();
      }
    }
  }

  void TestAppHost::OnTimeStateEvent(const TimeStateEvent& event)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->OnTimeStateEvent(event);
    }
  }

  void TestAppHost::ConfigurationChanged(const DemoWindowMetrics& windowMetrics)
  {
    m_realWindowMetrics = windowMetrics;

    const uint32_t densityDpi = !m_config.EmulateDpiEnabled ? m_realWindowMetrics.DensityDpi : m_config.EmulatedDpi;
    const DemoWindowMetrics appWindowMetrics(TypeConverter::To<PxExtent2D>(m_config.AppViewportPx.GetSize()), m_realWindowMetrics.ExactDpi,
                                             densityDpi);


    if (appWindowMetrics != m_appWindowMetrics)
    {
      m_appWindowMetrics = appWindowMetrics;
      if (m_appRecord.DemoExtension)
      {
        m_appRecord.DemoExtension->ConfigurationChanged(appWindowMetrics);
      }
    }
  }


  void TestAppHost::Begin(const DemoAppExtensionCallOrder callOrder)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->Begin(callOrder);
    }
  }

  void TestAppHost::PreUpdate(const DemoAppExtensionCallOrder callOrder, const DemoTime& demoTime)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->PreUpdate(callOrder, demoTime);
    }
  }

  void TestAppHost::FixedUpdate(const DemoAppExtensionCallOrder callOrder, const DemoTime& demoTime)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->FixedUpdate(callOrder, demoTime);
    }
  }

  void TestAppHost::Update(const DemoAppExtensionCallOrder callOrder, const DemoTime& demoTime)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->Update(callOrder, demoTime);
    }
  }

  void TestAppHost::PostUpdate(const DemoAppExtensionCallOrder callOrder, const DemoTime& demoTime)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->PostUpdate(callOrder, demoTime);
    }
  }

  void TestAppHost::Resolve(const DemoAppExtensionCallOrder callOrder, const DemoTime& demoTime)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->Resolve(callOrder, demoTime);
    }
  }

  void TestAppHost::OnDrawSkipped(const DemoAppExtensionCallOrder callOrder, const FrameInfo& frameInfo)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->OnDrawSkipped(callOrder, frameInfo);
    }
  }

  void TestAppHost::End(const DemoAppExtensionCallOrder callOrder)
  {
    if (m_appRecord.DemoExtension)
    {
      m_appRecord.DemoExtension->End(callOrder);
    }
  }

}
