#if defined(FSL_WINDOWSYSTEM_COCOA)
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

#include <FslBase/Exceptions.hpp>
#include <FslBase/Log/Log3Fmt.hpp>
#include <FslBase/Math/Rectangle.hpp>
#include <FslBase/Time/MillisecondTickCount32.hpp>
#include <FslBase/Time/TimeSpanUtil.hpp>
#include <FslNativeWindow/Base/INativeWindowEventQueue.hpp>
#include <FslNativeWindow/Base/NativeWindowEventHelper.hpp>
#include <FslNativeWindow/Base/NativeWindowSetup.hpp>
#include <FslNativeWindow/Base/NativeWindowSystemSetup.hpp>
#include <FslNativeWindow/Platform/Adapter/Cocoa/PlatformNativeWindowAdapterCocoa.hpp>
#include <FslNativeWindow/Platform/Adapter/Cocoa/PlatformNativeWindowSystemAdapterCocoa.hpp>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cmath>

#if !__has_feature(objc_arc)
#error This file must be compiled with ARC (-fobjc-arc)
#endif

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// Objective-C helper classes
// ----------------------------------------------------------------------------------------------------------------------------------------------------

//! Intercepts 'Quit' (Cmd+Q / the application menu) so the demo main loop can shut down in a orderly fashion instead of calling exit()
@interface FslCocoaApplicationDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic) BOOL quitRequested;
@end

@implementation FslCocoaApplicationDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
  (void)sender;
  self.quitRequested = YES;
  return NSTerminateCancel;
}
@end


@interface FslCocoaWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic) Fsl::PlatformNativeWindowAdapterCocoa* owner;
@end

@implementation FslCocoaWindowDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender
{
  (void)sender;
  if (self.owner != nullptr)
  {
    self.owner->OnCloseRequested();
  }
  // The window is destroyed by the adapter once the demo has shut down
  return NO;
}

- (void)windowDidResize:(NSNotification*)notification
{
  (void)notification;
  if (self.owner != nullptr)
  {
    self.owner->OnResized();
  }
}

- (void)windowDidChangeScreen:(NSNotification*)notification
{
  (void)notification;
  if (self.owner != nullptr)
  {
    self.owner->OnScreenConfigChanged();
  }
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification
{
  (void)notification;
  if (self.owner != nullptr)
  {
    self.owner->OnScreenConfigChanged();
  }
}
@end


//! The window content view, it hosts the CAMetalLayer and forwards input
@interface FslCocoaView : NSView
@property(nonatomic) Fsl::PlatformNativeWindowAdapterCocoa* owner;
@end

namespace
{
  // Virtual key codes from HIToolbox/Events.h (kVK_*). They are defined here so we dont have to pull in the Carbon headers.
  namespace MacKeyCode
  {
    constexpr unsigned short A = 0x00;
    constexpr unsigned short S = 0x01;
    constexpr unsigned short D = 0x02;
    constexpr unsigned short F = 0x03;
    constexpr unsigned short H = 0x04;
    constexpr unsigned short G = 0x05;
    constexpr unsigned short Z = 0x06;
    constexpr unsigned short X = 0x07;
    constexpr unsigned short C = 0x08;
    constexpr unsigned short V = 0x09;
    constexpr unsigned short B = 0x0B;
    constexpr unsigned short Q = 0x0C;
    constexpr unsigned short W = 0x0D;
    constexpr unsigned short E = 0x0E;
    constexpr unsigned short R = 0x0F;
    constexpr unsigned short Y = 0x10;
    constexpr unsigned short T = 0x11;
    constexpr unsigned short Code1 = 0x12;
    constexpr unsigned short Code2 = 0x13;
    constexpr unsigned short Code3 = 0x14;
    constexpr unsigned short Code4 = 0x15;
    constexpr unsigned short Code6 = 0x16;
    constexpr unsigned short Code5 = 0x17;
    constexpr unsigned short Code9 = 0x19;
    constexpr unsigned short Code7 = 0x1A;
    constexpr unsigned short Code8 = 0x1C;
    constexpr unsigned short Code0 = 0x1D;
    constexpr unsigned short O = 0x1F;
    constexpr unsigned short U = 0x20;
    constexpr unsigned short I = 0x22;
    constexpr unsigned short P = 0x23;
    constexpr unsigned short Return = 0x24;
    constexpr unsigned short L = 0x25;
    constexpr unsigned short J = 0x26;
    constexpr unsigned short K = 0x28;
    constexpr unsigned short N = 0x2D;
    constexpr unsigned short M = 0x2E;
    constexpr unsigned short Tab = 0x30;
    constexpr unsigned short Space = 0x31;
    constexpr unsigned short Delete = 0x33;
    constexpr unsigned short Escape = 0x35;
    constexpr unsigned short Shift = 0x38;
    constexpr unsigned short RightShift = 0x3C;
    constexpr unsigned short KeypadPlus = 0x45;
    constexpr unsigned short KeypadEnter = 0x4C;
    constexpr unsigned short KeypadMinus = 0x4E;
    constexpr unsigned short F5 = 0x60;
    constexpr unsigned short F6 = 0x61;
    constexpr unsigned short F7 = 0x62;
    constexpr unsigned short F3 = 0x63;
    constexpr unsigned short F8 = 0x64;
    constexpr unsigned short F9 = 0x65;
    constexpr unsigned short F11 = 0x67;
    constexpr unsigned short F10 = 0x6D;
    constexpr unsigned short F12 = 0x6F;
    constexpr unsigned short Help = 0x72;
    constexpr unsigned short Home = 0x73;
    constexpr unsigned short PageUp = 0x74;
    constexpr unsigned short ForwardDelete = 0x75;
    constexpr unsigned short F4 = 0x76;
    constexpr unsigned short End = 0x77;
    constexpr unsigned short F2 = 0x78;
    constexpr unsigned short PageDown = 0x79;
    constexpr unsigned short F1 = 0x7A;
    constexpr unsigned short LeftArrow = 0x7B;
    constexpr unsigned short RightArrow = 0x7C;
    constexpr unsigned short DownArrow = 0x7D;
    constexpr unsigned short UpArrow = 0x7E;
  }

  Fsl::VirtualKey::Enum ToVirtualKey(const unsigned short keyCode) noexcept
  {
    using namespace Fsl;
    switch (keyCode)
    {
    case MacKeyCode::A:
      return VirtualKey::A;
    case MacKeyCode::B:
      return VirtualKey::B;
    case MacKeyCode::C:
      return VirtualKey::C;
    case MacKeyCode::D:
      return VirtualKey::D;
    case MacKeyCode::E:
      return VirtualKey::E;
    case MacKeyCode::F:
      return VirtualKey::F;
    case MacKeyCode::G:
      return VirtualKey::G;
    case MacKeyCode::H:
      return VirtualKey::H;
    case MacKeyCode::I:
      return VirtualKey::I;
    case MacKeyCode::J:
      return VirtualKey::J;
    case MacKeyCode::K:
      return VirtualKey::K;
    case MacKeyCode::L:
      return VirtualKey::L;
    case MacKeyCode::M:
      return VirtualKey::M;
    case MacKeyCode::N:
      return VirtualKey::N;
    case MacKeyCode::O:
      return VirtualKey::O;
    case MacKeyCode::P:
      return VirtualKey::P;
    case MacKeyCode::Q:
      return VirtualKey::Q;
    case MacKeyCode::R:
      return VirtualKey::R;
    case MacKeyCode::S:
      return VirtualKey::S;
    case MacKeyCode::T:
      return VirtualKey::T;
    case MacKeyCode::U:
      return VirtualKey::U;
    case MacKeyCode::V:
      return VirtualKey::V;
    case MacKeyCode::W:
      return VirtualKey::W;
    case MacKeyCode::X:
      return VirtualKey::X;
    case MacKeyCode::Y:
      return VirtualKey::Y;
    case MacKeyCode::Z:
      return VirtualKey::Z;
    case MacKeyCode::Code0:
      return VirtualKey::Code0;
    case MacKeyCode::Code1:
      return VirtualKey::Code1;
    case MacKeyCode::Code2:
      return VirtualKey::Code2;
    case MacKeyCode::Code3:
      return VirtualKey::Code3;
    case MacKeyCode::Code4:
      return VirtualKey::Code4;
    case MacKeyCode::Code5:
      return VirtualKey::Code5;
    case MacKeyCode::Code6:
      return VirtualKey::Code6;
    case MacKeyCode::Code7:
      return VirtualKey::Code7;
    case MacKeyCode::Code8:
      return VirtualKey::Code8;
    case MacKeyCode::Code9:
      return VirtualKey::Code9;
    case MacKeyCode::Escape:
      return VirtualKey::Escape;
    case MacKeyCode::Tab:
      return VirtualKey::Tab;
    case MacKeyCode::Return:
    case MacKeyCode::KeypadEnter:
      return VirtualKey::Return;
    case MacKeyCode::Space:
      return VirtualKey::Space;
    case MacKeyCode::Delete:
      return VirtualKey::Backspace;
    case MacKeyCode::ForwardDelete:
      return VirtualKey::Delete;
    case MacKeyCode::Help:
      // The 'Help' key is located where 'Insert' is on PC keyboards
      return VirtualKey::Insert;
    case MacKeyCode::Home:
      return VirtualKey::Home;
    case MacKeyCode::End:
      return VirtualKey::End;
    case MacKeyCode::PageUp:
      return VirtualKey::PageUp;
    case MacKeyCode::PageDown:
      return VirtualKey::PageDown;
    case MacKeyCode::UpArrow:
      return VirtualKey::UpArrow;
    case MacKeyCode::DownArrow:
      return VirtualKey::DownArrow;
    case MacKeyCode::LeftArrow:
      return VirtualKey::LeftArrow;
    case MacKeyCode::RightArrow:
      return VirtualKey::RightArrow;
    case MacKeyCode::F1:
      return VirtualKey::F1;
    case MacKeyCode::F2:
      return VirtualKey::F2;
    case MacKeyCode::F3:
      return VirtualKey::F3;
    case MacKeyCode::F4:
      return VirtualKey::F4;
    case MacKeyCode::F5:
      return VirtualKey::F5;
    case MacKeyCode::F6:
      return VirtualKey::F6;
    case MacKeyCode::F7:
      return VirtualKey::F7;
    case MacKeyCode::F8:
      return VirtualKey::F8;
    case MacKeyCode::F9:
      return VirtualKey::F9;
    case MacKeyCode::F10:
      return VirtualKey::F10;
    case MacKeyCode::F11:
      return VirtualKey::F11;
    case MacKeyCode::F12:
      return VirtualKey::F12;
    case MacKeyCode::KeypadPlus:
      return VirtualKey::Add;
    case MacKeyCode::KeypadMinus:
      return VirtualKey::Subtract;
    case MacKeyCode::Shift:
      return VirtualKey::LeftShift;
    case MacKeyCode::RightShift:
      return VirtualKey::RightShift;
    default:
      return VirtualKey::Undefined;
    }
  }

  //! NSEvent timestamps are seconds since system startup, only the low 32 bits of the millisecond value are relevant.
  uint32_t ToTimestampMs(NSEvent* event) noexcept
  {
    return static_cast<uint32_t>(static_cast<uint64_t>(event.timestamp * 1000.0) & 0xFFFFFFFFu);
  }
}

@implementation FslCocoaView
- (BOOL)isFlipped
{
  // Use a top-left origin like the rest of the framework
  return YES;
}

- (BOOL)acceptsFirstResponder
{
  return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event
{
  (void)event;
  return YES;
}

- (BOOL)wantsUpdateLayer
{
  return YES;
}

- (void)viewDidChangeBackingProperties
{
  [super viewDidChangeBackingProperties];
  if (self.owner != nullptr)
  {
    self.owner->OnScreenConfigChanged();
  }
}

- (Fsl::PxPoint2)toPxPosition:(NSEvent*)event
{
  const NSPoint pos = [self convertPoint:event.locationInWindow fromView:nil];
  const CGFloat scale = self.window != nil ? self.window.backingScaleFactor : 1.0;
  return Fsl::PxPoint2::Create(static_cast<int32_t>(std::lround(pos.x * scale)), static_cast<int32_t>(std::lround(pos.y * scale)));
}

- (void)postMouseButton:(NSEvent*)event button:(Fsl::VirtualMouseButton)button isPressed:(bool)isPressed
{
  if (self.owner != nullptr)
  {
    self.owner->OnMouseButton(ToTimestampMs(event), button, isPressed, [self toPxPosition:event]);
  }
}

- (void)postMouseMove:(NSEvent*)event
{
  if (self.owner != nullptr)
  {
    self.owner->OnMouseMove(ToTimestampMs(event), [self toPxPosition:event]);
  }
}

- (void)mouseDown:(NSEvent*)event
{
  [self postMouseButton:event button:Fsl::VirtualMouseButton::Left isPressed:true];
}

- (void)mouseUp:(NSEvent*)event
{
  [self postMouseButton:event button:Fsl::VirtualMouseButton::Left isPressed:false];
}

- (void)rightMouseDown:(NSEvent*)event
{
  [self postMouseButton:event button:Fsl::VirtualMouseButton::Right isPressed:true];
}

- (void)rightMouseUp:(NSEvent*)event
{
  [self postMouseButton:event button:Fsl::VirtualMouseButton::Right isPressed:false];
}

- (void)otherMouseDown:(NSEvent*)event
{
  if (event.buttonNumber == 2)
  {
    [self postMouseButton:event button:Fsl::VirtualMouseButton::Middle isPressed:true];
  }
}

- (void)otherMouseUp:(NSEvent*)event
{
  if (event.buttonNumber == 2)
  {
    [self postMouseButton:event button:Fsl::VirtualMouseButton::Middle isPressed:false];
  }
}

- (void)mouseMoved:(NSEvent*)event
{
  [self postMouseMove:event];
}

- (void)mouseDragged:(NSEvent*)event
{
  [self postMouseMove:event];
}

- (void)rightMouseDragged:(NSEvent*)event
{
  [self postMouseMove:event];
}

- (void)otherMouseDragged:(NSEvent*)event
{
  [self postMouseMove:event];
}

- (void)scrollWheel:(NSEvent*)event
{
  // Scale to the Win32 convention of 120 per wheel notch. Trackpads report precise (pixel based) deltas.
  const double scale = event.hasPreciseScrollingDeltas ? 12.0 : 120.0;
  const auto delta = static_cast<int32_t>(std::lround(event.scrollingDeltaY * scale));
  if (delta != 0 && self.owner != nullptr)
  {
    self.owner->OnMouseWheel(ToTimestampMs(event), delta, [self toPxPosition:event]);
  }
}

- (void)keyDown:(NSEvent*)event
{
  // We intentionally don't call super as that would cause the system to 'beep' for unhandled keys
  if (self.owner != nullptr)
  {
    self.owner->OnKey(ToVirtualKey(event.keyCode), true);
  }
}

- (void)keyUp:(NSEvent*)event
{
  if (self.owner != nullptr)
  {
    self.owner->OnKey(ToVirtualKey(event.keyCode), false);
  }
}

- (void)flagsChanged:(NSEvent*)event
{
  // Modifier keys only generate flagsChanged events
  const auto key = ToVirtualKey(event.keyCode);
  if (self.owner != nullptr && (key == Fsl::VirtualKey::LeftShift || key == Fsl::VirtualKey::RightShift))
  {
    self.owner->OnKey(key, (event.modifierFlags & NSEventModifierFlagShift) != 0);
  }
}
@end


namespace Fsl
{
  namespace
  {
    constexpr double DefaultDpi = 96.0;
    constexpr double BaseDensityDpi = 160.0;
    constexpr int32_t DefaultWindowWidth = 1280;
    constexpr int32_t DefaultWindowHeight = 720;

    FslCocoaApplicationDelegate* g_applicationDelegate = nil;

    std::shared_ptr<IPlatformNativeWindowAdapter>
      AllocateWindow(const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& windowParams,
                     const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    {
      return std::make_shared<PlatformNativeWindowAdapterCocoa>(nativeWindowSetup, windowParams, pPlatformCustomWindowAllocationParams);
    }


    void CreateMainMenu()
    {
      NSMenu* menuBar = [[NSMenu alloc] init];
      NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
      [menuBar addItem:appMenuItem];

      NSMenu* appMenu = [[NSMenu alloc] init];
      NSString* quitTitle = [@"Quit " stringByAppendingString:NSProcessInfo.processInfo.processName];
      [appMenu addItemWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
      appMenuItem.submenu = appMenu;
      NSApp.mainMenu = menuBar;
    }


    void ActivateApplication()
    {
      if (@available(macOS 14.0, *))
      {
        [NSApp activate];
      }
      else
      {
        [NSApp activateIgnoringOtherApps:YES];
      }
    }


    NSScreen* SelectScreen(const int32_t displayId)
    {
      NSArray<NSScreen*>* screens = NSScreen.screens;
      if (displayId >= 0 && static_cast<NSUInteger>(displayId) < screens.count)
      {
        return screens[static_cast<NSUInteger>(displayId)];
      }
      FSLLOG3_WARNING("DisplayId {} not found, using the main display", displayId);
      return NSScreen.mainScreen;
    }


    //! The window rectangle is in points with a top-left origin relative to the visible area of the screen
    NSRect CalcContentRect(const NativeWindowConfig& config, NSScreen* screen)
    {
      const NSRect screenFrame = screen != nil ? screen.frame : NSMakeRect(0, 0, DefaultWindowWidth, DefaultWindowHeight);
      if (config.GetWindowMode() != WindowMode::Window)
      {
        return screenFrame;
      }

      const NSRect visibleFrame = screen != nil ? screen.visibleFrame : screenFrame;
      const Rectangle windowRectangle = config.GetWindowRectangle();
      const CGFloat width = windowRectangle.Width() > 0 ? windowRectangle.Width() : DefaultWindowWidth;
      const CGFloat height = windowRectangle.Height() > 0 ? windowRectangle.Height() : DefaultWindowHeight;
      // Cocoa uses a bottom-left origin
      const CGFloat x = visibleFrame.origin.x + windowRectangle.X();
      const CGFloat y = visibleFrame.origin.y + visibleFrame.size.height - windowRectangle.Y() - height;
      return NSMakeRect(x, y, width, height);
    }
  }    // namespace


  struct PlatformNativeWindowAdapterCocoa::Impl
  {
    NSWindow* Window{nil};
    FslCocoaView* View{nil};
    FslCocoaWindowDelegate* Delegate{nil};
    CAMetalLayer* Layer{nil};
  };


  PlatformNativeWindowSystemAdapterCocoa::PlatformNativeWindowSystemAdapterCocoa(const NativeWindowSystemSetup& setup,
                                                                                 const PlatformNativeWindowAllocationFunction& allocateWindowFunction,
                                                                                 const PlatformNativeWindowSystemParams& /*systemParams*/)
    : PlatformNativeWindowSystemAdapter(setup, nullptr)
    , m_allocationFunction(allocateWindowFunction ? allocateWindowFunction : AllocateWindow)
  {
    if ([NSThread isMainThread] == NO)
    {
      throw NotSupportedException("The Cocoa window system must be created on the main thread");
    }

    FSLLOG3_VERBOSE3("PlatformNativeWindowSystemAdapterCocoa| Initializing NSApplication");
    @autoreleasepool
    {
      [NSApplication sharedApplication];
      if (g_applicationDelegate == nil)
      {
        g_applicationDelegate = [[FslCocoaApplicationDelegate alloc] init];
        NSApp.delegate = g_applicationDelegate;
        // A unbundled executable launched from a terminal is a 'background' app unless we promote it
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        CreateMainMenu();
        [NSApp finishLaunching];
      }
      g_applicationDelegate.quitRequested = NO;
    }

    auto eventQueue = setup.GetEventQueue().lock();
    if (eventQueue)
    {
      eventQueue->PostEvent(NativeWindowEventHelper::EncodeGamepadConfiguration(0));
    }
    FSLLOG3_VERBOSE3("PlatformNativeWindowSystemAdapterCocoa| Initialized");
  }


  PlatformNativeWindowSystemAdapterCocoa::~PlatformNativeWindowSystemAdapterCocoa() = default;


  std::shared_ptr<IPlatformNativeWindowAdapter> PlatformNativeWindowSystemAdapterCocoa::CreateNativeWindow(
    const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
  {
    FSLLOG3_VERBOSE3("PlatformNativeWindowSystemAdapterCocoa| CreateNativeWindow");
    if (m_window.lock())
    {
      throw NotSupportedException("We only support one active window");
    }

    auto window = m_allocationFunction(nativeWindowSetup, PlatformNativeWindowParams(m_platformDisplay), pPlatformCustomWindowAllocationParams);
    auto ptr = std::dynamic_pointer_cast<PlatformNativeWindowAdapterCocoa>(window);
    if (!ptr)
    {
      throw NotSupportedException("Allocation function did not allocate a PlatformNativeWindowAdapterCocoa as required");
    }
    m_window = ptr;
    return window;
  }


  bool PlatformNativeWindowSystemAdapterCocoa::ProcessMessages(const NativeWindowProcessMessagesArgs& /*args*/)
  {
    @autoreleasepool
    {
      while (true)
      {
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES];
        if (event == nil)
        {
          break;
        }
        [NSApp sendEvent:event];
      }
    }

    const auto window = m_window.lock();
    const bool quit = (g_applicationDelegate != nil && g_applicationDelegate.quitRequested) || (window && window->IsCloseRequested());
    return !quit;
  }


  PlatformNativeWindowAdapterCocoa::PlatformNativeWindowAdapterCocoa(
    const NativeWindowSetup& nativeWindowSetup, const PlatformNativeWindowParams& platformWindowParams,
    const PlatformNativeWindowAllocationParams* const pPlatformCustomWindowAllocationParams)
    : PlatformNativeWindowAdapter(nativeWindowSetup, platformWindowParams, pPlatformCustomWindowAllocationParams,
                                  NativeWindowCapabilityFlags::GetDpi | NativeWindowCapabilityFlags::GetDensityDpi |
                                    NativeWindowCapabilityFlags::GetDisplayInfo)
    , m_impl(std::make_unique<Impl>())
    , m_cachedDpi(static_cast<float>(DefaultDpi), static_cast<float>(DefaultDpi))
  {
    FSLLOG3_VERBOSE3("PlatformNativeWindowAdapterCocoa| Constructing");
    @autoreleasepool
    {
      const NativeWindowConfig nativeWindowConfig = nativeWindowSetup.GetConfig();
      NSScreen* screen = SelectScreen(nativeWindowConfig.GetDisplayId());
      const NSRect contentRect = CalcContentRect(nativeWindowConfig, screen);

      const NSWindowStyleMask styleMask =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
      NSWindow* window = [[NSWindow alloc] initWithContentRect:contentRect styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
      if (window == nil)
      {
        throw GraphicsException("Failed to create NSWindow");
      }
      window.releasedWhenClosed = NO;
      window.acceptsMouseMovedEvents = YES;
      window.collectionBehavior = window.collectionBehavior | NSWindowCollectionBehaviorFullScreenPrimary;
      window.title = [NSString stringWithUTF8String:nativeWindowSetup.GetApplicationName().c_str()];

      FslCocoaView* view = [[FslCocoaView alloc] initWithFrame:NSMakeRect(0, 0, contentRect.size.width, contentRect.size.height)];
      view.owner = this;

      // Make the view layer-hosting with a CAMetalLayer so it can be used with VK_EXT_metal_surface
      CAMetalLayer* layer = [CAMetalLayer layer];
      layer.contentsScale = window.backingScaleFactor;
      view.layer = layer;
      view.wantsLayer = YES;
      window.contentView = view;

      FslCocoaWindowDelegate* windowDelegate = [[FslCocoaWindowDelegate alloc] init];
      windowDelegate.owner = this;
      window.delegate = windowDelegate;

      m_impl->Window = window;
      m_impl->View = view;
      m_impl->Delegate = windowDelegate;
      m_impl->Layer = layer;
      m_platformWindow = (__bridge void*)layer;

      [window makeKeyAndOrderFront:nil];
      [window makeFirstResponder:view];
      ActivateApplication();

      if (nativeWindowConfig.GetWindowMode() != WindowMode::Window)
      {
        [window toggleFullScreen:nil];
      }

      UpdateScreenInfo(false);
      UpdateWindowSize(false);
    }

    {    // Post the activation message to let the framework know we are ready
      auto eventQueue = TryGetEventQueue();
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowActivationEvent(true));
      }
    }
    FSLLOG3_VERBOSE3("PlatformNativeWindowAdapterCocoa| Constructed");
  }


  PlatformNativeWindowAdapterCocoa::~PlatformNativeWindowAdapterCocoa()
  {
    @autoreleasepool
    {
      m_platformWindow = nullptr;
      if (m_impl->Window != nil)
      {
        // Ensure that no callbacks reach this object while it is being destroyed
        m_impl->View.owner = nullptr;
        m_impl->Delegate.owner = nullptr;
        m_impl->Window.delegate = nil;
        [m_impl->Window orderOut:nil];
        [m_impl->Window close];
      }
      m_impl.reset();
    }
  }


  void PlatformNativeWindowAdapterCocoa::OnCloseRequested()
  {
    m_closeRequested = true;
  }


  void PlatformNativeWindowAdapterCocoa::OnResized()
  {
    UpdateWindowSize(true);
  }


  void PlatformNativeWindowAdapterCocoa::OnScreenConfigChanged()
  {
    UpdateScreenInfo(true);
    UpdateWindowSize(true);
  }


  void PlatformNativeWindowAdapterCocoa::OnKey(const VirtualKey::Enum key, const bool isPressed)
  {
    if (key == VirtualKey::Undefined)
    {
      return;
    }
    auto eventQueue = TryGetEventQueue();
    if (eventQueue)
    {
      eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputKeyEvent(key, isPressed));
    }
  }


  void PlatformNativeWindowAdapterCocoa::OnMouseButton(const uint32_t timestampMs, const VirtualMouseButton button, const bool isPressed,
                                                       const PxPoint2 positionPx)
  {
    auto eventQueue = TryGetEventQueue();
    if (eventQueue)
    {
      const auto timestamp = MillisecondTickCount32::FromMilliseconds(static_cast<int32_t>(timestampMs));
      eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputMouseButtonEvent(timestamp, button, isPressed, positionPx));
    }
  }


  void PlatformNativeWindowAdapterCocoa::OnMouseMove(const uint32_t timestampMs, const PxPoint2 positionPx)
  {
    auto eventQueue = TryGetEventQueue();
    if (eventQueue)
    {
      const auto timestamp = MillisecondTickCount32::FromMilliseconds(static_cast<int32_t>(timestampMs));
      eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputMouseMoveEvent(timestamp, positionPx));
    }
  }


  void PlatformNativeWindowAdapterCocoa::OnMouseWheel(const uint32_t timestampMs, const int32_t delta, const PxPoint2 positionPx)
  {
    auto eventQueue = TryGetEventQueue();
    if (eventQueue)
    {
      const auto timestamp = MillisecondTickCount32::FromMilliseconds(static_cast<int32_t>(timestampMs));
      eventQueue->PostEvent(NativeWindowEventHelper::EncodeInputMouseWheelEvent(timestamp, delta, positionPx));
    }
  }


  bool PlatformNativeWindowAdapterCocoa::TryGetNativeSize(PxPoint2& rSize) const
  {
    rSize = m_cachedWindowSize;
    return true;
  }


  bool PlatformNativeWindowAdapterCocoa::TryGetNativeDpi(Vector2& rDPI) const
  {
    rDPI = m_cachedDpi;
    return true;
  }


  bool PlatformNativeWindowAdapterCocoa::TryGetNativeDensityDpi(uint32_t& rDensityDpi) const
  {
    rDensityDpi = m_cachedDensityDpi;
    return true;
  }


  NativeWindowDisplayInfo PlatformNativeWindowAdapterCocoa::TryGetNativeDisplayInfo() const
  {
    return m_cachedDisplayInfo;
  }


  void PlatformNativeWindowAdapterCocoa::UpdateScreenInfo(const bool postEvents)
  {
    if (!m_impl || m_impl->Window == nil)
    {
      return;
    }

    @autoreleasepool
    {
      NSScreen* screen = m_impl->Window.screen != nil ? m_impl->Window.screen : NSScreen.mainScreen;
      const CGFloat scale = m_impl->Window.backingScaleFactor;
      m_impl->Layer.contentsScale = scale;

      // Calculate the physical DPI (fall back to a scaled default if the display doesn't report its size)
      auto newDpi = Vector2(static_cast<float>(DefaultDpi * scale), static_cast<float>(DefaultDpi * scale));
      NativeWindowDisplayInfo newDisplayInfo;
      if (screen != nil)
      {
        NSNumber* screenNumber = screen.deviceDescription[@"NSScreenNumber"];
        const auto displayId = static_cast<CGDirectDisplayID>(screenNumber.unsignedIntValue);
        const CGSize sizeMM = CGDisplayScreenSize(displayId);
        const NSSize sizePoints = screen.frame.size;
        if (sizeMM.width > 0.0 && sizeMM.height > 0.0)
        {
          constexpr double MillimetersPerInch = 25.4;
          newDpi = Vector2(static_cast<float>((sizePoints.width * scale) / (sizeMM.width / MillimetersPerInch)),
                           static_cast<float>((sizePoints.height * scale) / (sizeMM.height / MillimetersPerInch)));
        }

        if (@available(macOS 12.0, *))
        {
          const NSInteger maxFps = screen.maximumFramesPerSecond;
          if (maxFps > 0)
          {
            newDisplayInfo = NativeWindowDisplayInfo(TimeSpanUtil::FromFrequencyRational(static_cast<uint64_t>(maxFps), 1u));
          }
        }
      }
      const auto newDensityDpi = static_cast<uint32_t>(std::lround(BaseDensityDpi * scale));

      if (newDpi == m_cachedDpi && newDensityDpi == m_cachedDensityDpi && newDisplayInfo == m_cachedDisplayInfo)
      {
        return;
      }
      m_cachedDpi = newDpi;
      m_cachedDensityDpi = newDensityDpi;
      m_cachedDisplayInfo = newDisplayInfo;
      FSLLOG3_VERBOSE2("PlatformNativeWindowAdapterCocoa| Screen scale: {} dpi: {}x{} densityDpi: {} refresh rate: {}Hz", scale, m_cachedDpi.X,
                       m_cachedDpi.Y, m_cachedDensityDpi, m_cachedDisplayInfo.RefreshRateHz());
    }

    if (postEvents)
    {
      auto eventQueue = TryGetEventQueue();
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowConfigChanged());
      }
    }
  }


  void PlatformNativeWindowAdapterCocoa::UpdateWindowSize(const bool postEvents)
  {
    if (!m_impl || m_impl->View == nil)
    {
      return;
    }

    PxPoint2 newSize;
    @autoreleasepool
    {
      // The size is reported in pixels (the backing store size)
      const NSRect backingRect = [m_impl->View convertRectToBacking:m_impl->View.bounds];
      newSize =
        PxPoint2::Create(static_cast<int32_t>(std::lround(backingRect.size.width)), static_cast<int32_t>(std::lround(backingRect.size.height)));
    }
    if (newSize == m_cachedWindowSize)
    {
      return;
    }
    m_cachedWindowSize = newSize;
    FSLLOG3_VERBOSE2("PlatformNativeWindowAdapterCocoa| Window size {}x{}", m_cachedWindowSize.X.Value, m_cachedWindowSize.Y.Value);

    if (postEvents)
    {
      auto eventQueue = TryGetEventQueue();
      if (eventQueue)
      {
        eventQueue->PostEvent(NativeWindowEventHelper::EncodeWindowResizedEvent());
      }
    }
  }
}    // namespace Fsl
#endif
