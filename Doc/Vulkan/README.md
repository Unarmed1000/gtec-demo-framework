# Vulkan

## Requirements

The framework requires Vulkan 1.3. Both the Vulkan loader and the physical device must support it, otherwise the app fails at startup with an error that names the version that was found. The ```VulkanInfo``` sample reports if the loader and each physical device meet this requirement.

- The instance is created with api version 1.3, use ```--VkApiVersion 1.4``` to request a newer version.
- The shaders are compiled to SPIR-V for Vulkan 1.3 (```glslangValidator --target-env vulkan1.3```).

The ```Vulkan``` project template is the recommended way to create a new Vulkan samples.

Vulkan is a low level "no secrets" type of API that might seem intimidating at first glance and its not really recommended for beginners. It requires a lot of attention to detail, knowledge of how to write properly threaded code and issues related to that. It also requires a lot of setup work just to get something on the screen and even more to make sure you handle all the possible error cases that can occur both during setup and while the app is running. Vulkan's 'front heavy' design basically makes it hard/time-consuming to create small demos but fits well with a well thought through rendering engine. However once you get all the setup and error handling done you will find that rendering something becomes *fairly* simple.

This is where the frameworks Vulkan app template helps out as it handles most of the setup for you based on our experience with writing Vulkan samples which should make it a lot faster to get something on the screen to start working on the actual rendering sample or experiment.

However to truly take proper advantage of Vulkan features and gain the desired performance benefits its really recommended to create a 'proper' multi-threaded engine for interacting with it and the engine should be designed with knowledge of how Vulkan works and not be something that is based on the old OpenGL way of thinking.

## The vulkan demo host

Most demo-hosts do a lot of setup and are responsible for doing the frame 'SwapBuffers' call, but due to the flexibility and low level nature of Vulkan we had to make a number of adjustments to the Vulkan demo hosts responsibility.

Currently it takes care of:

- Common command line parameters
- Vulkan instance creation this included enabling extensions.
- Vulkan window creation using the "Vulkan Native Window System".
- Selecting the physical vulkan device to use.

The normal SwapBuffers responsibility is given to the Application instead using ```_TrySwapBuffers```.

## Getting started

As Vulkan is a very low level interface it's more complex to create a template that will fit all Vulkan project types.
Therefore we chosen to create a flexible solution that allows for various app templates to be easily implemented.
Each template is designed for a specific demo type and usage pattern based on what we observed to be common for Vulkan samples.

Project           | Class              | Description
------------------|--------------------|-----------------------------------------------------------------------------------
None              | DemoAppVulkan       | This is the base class for Window based Vulkan apps. A app using this is basically a freestyle app that needs to handle resize, lost resources, stats overlay and screenshot support. **(Not recommended)**
Vulkan            | DemoAppVulkanBasic  | Defaults to double buffered rendering with support for resize strategies, resource loss and stats overlay. Support for screenshots will be added soon. **(Recommended)**
WillemsVulkan     | DemoAppVulkan based | A sample that does not use AssImp. **(Not recommended)**
WillemsMeshVulkan | DemoAppVulkan based | A sample that use AssImp for model loading. **(Not recommended)**

The WillemsVulkan project type exist to make it easier to port a existing sample that use [Sascha Willems framework](https://github.com/SaschaWillems/Vulkan). Its not recommend to use the Willems vulkan templates unless you know what you are doing.

Apps based on DemoAppVulkan take on a lot of responsibility, for example just minimizing the vulkan window on Windows can invalidate the Swapchain requiring you to rebuild it and anything dependent on it. Furthermore the swap chain can not be recreated before the window becomes visible again which is another thing that need to be handled. There are a lot of such small 'gotchas' every Vulkan app needs to deal with which is why its recommended to use the DemoAppVulkanBasic template which has strategies in place for all of these things.

Due to the low level nature of Vulkan, its philosophy of 'no secrets' and because the app has become responsible for doing the 'SwapBuffers' call there is also no 'easy' way to add the '--Stats' overlay. So the app has become responsible for making sure its supported and we currently only have a strategy in place for how to do this for DemoAppVulkanBasic type projects where it must call ```AddSystemUI``` during ```VulkanDraw```. While it technically is possible to render a overlay on top of any Vulkan app its not as cheap performance wise as when the app is cooperating. However we will look into enabling this fallback for any app that doesn't call ```AddSystemUI``` in a future release.

Taking a screenshot is also a lot more involved and complex under Vulkan so we encountered some of the same issues and considerations as we did for rendering the '--Stats' overlay. But we have not had time to implement a strategy for this yet but it is expected to be completed soon.

## DemoAppVulkanBasic

This is the recommended template for new Vulkan projects. It defines a strategy for creating resources, the swapchain (including recreation) and how a frame is rendered. It also includes all the code that provides support for rendering the '--Stats' overlay. Furthermore it supports various resize strategies and has quite a few helper methods that makes creating up a basic Vulkan sample easier.

The sample defaults to a double buffered setup, no depth buffer and one frame 'in flight': the next frame is not started before the GPU is done with the previous one. `--VkFramesInFlight <n>` allows more (limited to `CustomDemoAppConfig::MaxFramesInFlight` of the app, which defaults to two, and to the images of the swapchain).
Having two frames in flight ensures that we dont spend time waiting for the GPU to finish one frame before we start rendering the next one which should utilize the GPU resources better. Beware that most examples on the net wait for the GPU to be done between frames which ensures they dont have to deal with some synchronization issues but at the cost of lower performance. But its not a ideal way to do things for a production application. Which is why we prefer to showcase and deal with this like most real production applications would. The main thing to remember when allowing in-flight frames is that each frame being rendered should only change resources that it owns and things not being used by any other in-flight frame.

The template also defines a init, shutdown, resize, frame and recovery strategy.

If you have OpenGL ES 2+3 experience it can be useful to try to compare the implementation of some of the samples that exist for both API's to see the differences. Many of the samples have been constructed in a way that makes a diff fairly easy to perform.

### Basic template configuration

Various parameters can be tweaked by modifying the ```DemoAppVulkanSetup``` struct supplied to the ```DemoAppVulkanBasic``` constructor.

To do this change:

```C++
CustomDemoApp::CustomDemoApp(const DemoAppConfig& config)
  : DemoAppVulkanBasic(config)
{
}
```

to

```C++
namespace
{
  VulkanBasic::DemoAppVulkanSetup CreateSetup()
  {
    VulkanBasic::DemoAppVulkanSetup setup;
    // Enable depth buffer
    setup.DepthBuffer = VulkanBasic::DepthBufferMode::Enabled;
    return setup;
  }
}

CustomDemoApp::CustomDemoApp(const DemoAppConfig& config)
  : DemoAppVulkanBasic(config, CreateSetup())
{
}
```

It also allows you to tweak the active resize strategy, the VkPresentModeKHR, swapchain image usage flags and more.

### Init sequence

Due to Vulkans low level nature it requires more work to handle its resources. The DemoAppVulkanBasic template basically splits it into two resource types.

- Device dependent
- Swapchain dependent/related.

The device dependent resources are normally created during construction and are not affected by the swap chain being lost or window resizing. Where the swap chain dependent resources are created in the OnBuildResources call which allows them to be freed and recreated on demand. For example if the swap chain becomes invalid.

A high level sequence overview.

<a href="Images/DemoAppVulkanBasic_Flow_Init.svg">
<img src="Images/DemoAppVulkanBasic_Flow_Init.svg">
</a>

While BuildResources can be manually called from the Constructor it's not recommended to do so as it makes error handling more complex and there could also be issues with virtual methods.

### Shutdown sequence

A high level sequence overview.

<a href="Images/DemoAppVulkanBasic_Flow_Shutdown.svg">
<img src="Images/DemoAppVulkanBasic_Flow_Shutdown.svg">
</a>

As we rely on virtual methods and because we technically could have a exception occur during the shutdown sequence all resources are freed during the ```_PreDestruct``` call which calls ```OnDestroy``` in the *CustomDemoApp*.

If the CustomDemoApp needs to shutdown something that could possibly fail we recommend that it overloads the OnDestroy method and implements its shutdown sequence there. For things in the shutdown sequence that are known to not throw the standard destructor can be used as usual.

### Frame sequence

A high level sequence overview.

<a href="Images/DemoAppVulkanBasic_Flow_Draw.svg">
<img src="Images/DemoAppVulkanBasic_Flow_Draw.svg">
</a>

The call sequence can be interrupted at any of the ```_Try``` method calls if they fail, causing a recovery strategy to execute.

### Frame sequence recovery

During the frame sequence any ```_Try``` method is allowed to fail and can request a specific recovery strategy to be used. Currently the frame sequence has these ```_Try``` method.

```C++
AppDrawResult _TryPrepareDraw(const FrameInfo& frameInfo) override;
AppDrawResult _TrySwapBuffers(const FrameInfo& frameInfo) override;
```

They can return

```C++
enum class AppDrawResult
{
  //! The operation failed  (use default handling, which is to restart the app)
  Failed,
  //! Not ready to draw (but keep asking every 'frame')
  NotReady,
  //! Something went wrong but we should be able to recover, restart the draw loop (TryPrepare, Draw, Etc)
  Retry,
  //! Everything completed successfully
  Completed,
};
```

### Resize

When the window is resized it can be necessary to recreate various Vulkan objects and the app might also be dependent on the window size. To support a wide range of scenarios the app template supports various resize strategies.

However you need to be aware that all demo framework apps by default are destroyed then recreate when a resize occurs. This also means that everything is restarted from scratch unless the app implements some kind of load/save state system.

So any resize strategies in DemoAppVulkanBasic has no effect unless this overall DemoFramework strategy is changed.
This can be done in the apps ```CustomDemoApp_Register.cpp``` file by changing:

```C++
namespace Fsl
{
  // Configure the demo environment to run this demo app in a Vulkan host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    DemoAppHostConfigVulkan config;

    DemoAppRegister::Vulkan::Register<CustomDemoApp>(rSetup, "Vulkan.CustomDemoApp", config);
  }
}
```

to

```C++
namespace Fsl
{
  // Configure the demo environment to run this demo app in a Vulkan host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    // 1. Change the default demo-framework app resize strategy to allow the app to keep running on resize
    CustomDemoAppConfig customDemoAppConfig;
    customDemoAppConfig.RestartFlags = CustomDemoAppConfigRestartFlags::Never;

    DemoAppHostConfigVulkan config;

    // 2. Supply the customDemoAppConfig to the register method
    DemoAppRegister::Vulkan::Register<CustomDemoApp>(rSetup, "Vulkan.CustomDemoApp", config, customDemoAppConfig);
  }
}
```

With that done the Vulkan app template resize strategy will be used and the VulkanDemoAppBasic app template supports these strategies:

Strategy name    | Description
-----------------|-----------------------------------------------------------------------------------------------------
Disabled         | The application takes full control and responsibility for supporting the resize.
RebuildResources | The ConfigurationChanged method is called and it is responsible for recreating dependent resources. **(Default)**

The policy can be changed by modifying the ```DemoAppVulkanSetup``` struct supplied to the ```DemoAppVulkanBasic``` constructor.

Change:

```C++
CustomDemoApp::CustomDemoApp(const DemoAppConfig& config)
  : DemoAppVulkanBasic(config)
{
}
```

to

```C++
namespace
{
  VulkanBasic::DemoAppVulkanSetup CreateSetup()
  {
    VulkanBasic::DemoAppVulkanSetup setup;
    setup.ActiveResizeStrategy = VulkanBasic::ResizeStrategy::Disabled;
    return setup;
  }
}

CustomDemoApp::CustomDemoApp(const DemoAppConfig& config)
  : DemoAppVulkanBasic(config, CreateSetup())
{
}
```

#### Resize strategy Disabled

The applications "ConfigurationChanged" method is called and its up to it to recreate all resources that requires it.

#### Resize strategy RebuildResources

The RebuildResources resize policy checks if any resources are allocated and if there is it call DestroyResources followed by BuildResources. While this can be slightly expensive it requires a minimum amount of work to support for most apps and it will be less expensive than destroying and restarting the app. Furthermore as the app isn't restarted there is no need to save any state for most apps as only Vulkan 'dependent' resources are destroyed and recreated.

Beware that the app is responsible for making sure that it supports resizing, so anything that is dependent on the window size will need to be updated. This is what the ```ConfigurationChanged``` callback is for.

A high level sequence overview.

<a href="Images/DemoAppVulkanBasic_Flow_Resize.svg">
<img src="Images/DemoAppVulkanBasic_Flow_Resize.svg">
</a>

Following a successful ```ConfigurationChanged``` operation the normal frame sequence is resumed.

## Build resources

It's recommended to look at some of the existing samples to get a good idea for how they implement their build resources method.

The ```OnBuildResources``` method must create and return the main render pass and the DemoAppVulkanBasic contains a ```CreateBasicRenderPass``` method that can be used to get started. But more advanced apps will want to create their own.

The ```OnBuildResources``` method is supplied a ```BuildResourcesContext``` struct that contain information about the context we are building resources for so its recommended to take a closer look at that struct.

## Using physical device features

Vulkan requires that many physical device features are enabled when creating the Vulkan instance and the instance is created by the Vulkan demo host which provides a mechanism for interacting with it.

For example to enable various texture compression formats we can request them to be enabled. This can be done in the apps ```CustomDemoApp_Register.cpp``` file by changing:

```C++
namespace Fsl
{
  // Configure the demo environment to run this demo app in a Vulkan host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    DemoAppHostConfigVulkan config;

    DemoAppRegister::Vulkan::Register<CustomDemoApp>(rSetup, "Vulkan.CustomDemoApp", config);
  }
}
```

to

```C++
namespace Fsl
{
  // Configure the demo environment to run this demo app in a Vulkan host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    using namespace Vulkan;

    DemoAppHostConfigVulkan config;
    config.AddPhysicalDeviceFeatureRequest(PhysicalDeviceFeature::TextureCompressionASTC_LDR, FeatureRequirement::Optional);
    config.AddPhysicalDeviceFeatureRequest(PhysicalDeviceFeature::TextureCompressionBC, FeatureRequirement::Optional);
    config.AddPhysicalDeviceFeatureRequest(PhysicalDeviceFeature::TextureCompressionETC2, FeatureRequirement::Optional);

    DemoAppRegister::Vulkan::Register<CustomDemoApp>(rSetup, "Vulkan.CustomDemoApp", config);
  }
}
```

Since all of them are set to Optional in this example that means that the application needs to handle that all of them might be unavailable.

The Vulkan 1.1, 1.2 and 1.3 core features are requested the same way, for example:

```C++
config.AddPhysicalDeviceFeatureRequest(PhysicalDeviceFeature::TimelineSemaphore, FeatureRequirement::Mandatory);
config.AddPhysicalDeviceFeatureRequest(PhysicalDeviceFeature::Synchronization2, FeatureRequirement::Optional);
```

The enabled features are available to the app in ```m_deviceActiveFeatures``` (Vulkan 1.0) and ```m_deviceActiveFeatures11```, ```m_deviceActiveFeatures12``` and ```m_deviceActiveFeatures13```.
A ```IVulkanDeviceCreationCustomizer``` must not add the core feature structs (or the feature structs of the extensions that were promoted to core) to the device create info pNext chain, request the features like this instead.

## Extensions and layers

Extensions and layers can be requested in the same way as physical device features. This can be done in the apps ```CustomDemoApp_Register.cpp``` file.

### Instance layers

```C++
namespace Fsl
{
  // Configure the demo environment to run this demo app in a Vulkan host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    using namespace Vulkan;

    DemoAppHostConfigVulkan config;
    // This is just a example (VK_LAYER_LUNARG_api_dump can actually be enabled/disabled from the command line)
    config.AddInstanceLayerRequest("VK_LAYER_LUNARG_api_dump", FeatureRequirement::Mandatory);

    DemoAppRegister::Vulkan::Register<CustomDemoApp>(rSetup, "Vulkan.CustomDemoApp", config);
  }
}
```

### Instance extensions

```C++
namespace Fsl
{
  // Configure the demo environment to run this demo app in a Vulkan host environment
  void ConfigureDemoAppEnvironment(HostDemoAppSetup& rSetup)
  {
    using namespace Vulkan;

    DemoAppHostConfigVulkan config;
    config.AddInstanceExtensionRequest("VK_EXT_swapchain_colorspace", FeatureRequirement::Optional);

    DemoAppRegister::Vulkan::Register<CustomDemoApp>(rSetup, "Vulkan.CustomDemoApp", config);
  }
}
```

A extension that is provided by one of the enabled layers can be requested as well. ```VK_EXT_debug_utils``` does not need to be requested by the app, the demo host takes care of it (see [Debugging](#debugging)).

## Hints

### Misc tips

- [RenderDoc](https://renderdoc.org/) can be extremely useful while debugging a Vulkan app. Take advantage of it.
- All Vulkan samples support ```--LogExtensions``` which logs all available extensions.
- All Vulkan samples support ```--LogLayers``` which logs all available layers.
- All Vulkan samples support ```--VkApiDump``` which enable the VK_LAYER_LUNARG_api_dump layer.
- All Vulkan samples support ```--VkPhysicalDevice``` which allows you to select the physical device.
- DemoAppVulkanBasic samples support ```--VkPresentMode``` which can be used to override the present mode chosen by the app.

### Texture compression

- Its highly recommended to utilize it as much as possible it to save bandwidth, memory and improve performance
- For cross platform support always support multiple compression formats as no format will work everywhere.

### Validation

- Its highly recommended to enable the VK_LAYER_KHRONOS_validation while developing a Vulkan app. If you dont use it your app will most likely contain errors. So save time and just use it!
- Debug builds enable VK_LAYER_KHRONOS_validation by default.
- The ```--VkValidate``` argument can be used to enable or disable it from the command line.
- The ```--VkValidateFeatures``` argument enables the optional checks of the layer. It takes a comma separated list of ```sync``` (synchronization validation), ```gpu``` (GPU assisted validation), ```bestpractices``` and ```printf``` (the debugPrintfEXT shader function). For example ```--VkValidateFeatures sync,bestpractices```. It also enables the layer unless ```--VkValidate false``` is used.
- The messages of the layer are written to the log, see [Debugging](#debugging).

## Debugging

The demo host enables ```VK_EXT_debug_utils``` in debug builds and when the validation layer is enabled. Use ```--VkDebugUtils true``` to enable it in a release build (for example to get named objects in a RenderDoc capture) or ```--VkDebugUtils false``` to disable it.

When it is enabled

- The Vulkan debug messages are written to the log. Errors and warnings are always written, the info messages need ```-vvv``` and the verbose messages need ```-vvvvv``` (the loader is very talkative at those levels). A validation message lists the names of the objects it is about and the command buffer labels that were active.
- The Vulkan objects created by the framework have names (```Swapchain.Image0```, ```Frame0.CmdBuffer```, ```DepthBuffer```, ```QuadBatch.Pipeline.AlphaBlend```, ...) and the commands it records are labeled (```SystemUI```, ```QuadBatch```, ```BasicRender```).

If the working directory contains a ```vk_layer_settings.txt``` that sets ```khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG``` (the ones in this repository do) the validation layer also prints its messages itself, so they are shown twice.

### Naming objects and labeling commands

A app can name its own objects and label the commands it records with ```VUDebugUtils``` and ```VUScopedCmdDebugLabel``` from ```FslUtil.Vulkan1_0```. They do nothing when ```VK_EXT_debug_utils``` is not enabled, so there is no need to check for it. Use ```VUDebugUtils::IsEnabled()``` to skip the work of building a name.

```C++
#include <FslUtil/Vulkan1_0/Debug/VUDebugUtils.hpp>
#include <FslUtil/Vulkan1_0/Debug/VUScopedCmdDebugLabel.hpp>

// Name a object, the type is supplied as the handle types are identical on a 32bit target
Vulkan::VUDebugUtils::SetObjectName(m_device.Get(), VK_OBJECT_TYPE_PIPELINE, m_pipeline.Get(), "Scene.Pipeline");

// Label everything recorded to the command buffer until the end of the scope
{
  const Vulkan::VUScopedCmdDebugLabel scopedLabel(hCmdBuffer, "Scene");
  vkCmdDraw(hCmdBuffer, vertexCount, 1, 0, 0);
}
```

```VUImageMemoryView```, ```VUFramebuffer``` and the ```VulkanImageCreator``` methods take a name that is given to the objects they create. ```Vulkan.Bloom``` and ```Vulkan.PixelArt``` label their render passes.

### Debugging tips

- [RenderDoc](https://renderdoc.org/) shows the object names and uses the labels to group the draw calls of a capture.
- ```--VkApiDump``` logs every Vulkan call, including the names and labels that are set.
- ```-v``` logs the tools that are attached to the physical device (the validation layer, a API dump layer, RenderDoc, ...). A tool can be the reason a app behaves differently.
- ```-vv``` logs the properties of the physical device and its memory heaps. If the device supports ```VK_EXT_memory_budget``` each heap is logged with its budget and how much the app uses of it. A app can query the same values with ```MemoryBudgetUtil``` from ```FslUtil.Vulkan1_0```.
- ```--Stats --StatsFlags "frame|cpu|gpu"``` shows the GPU load and the GPU memory usage of the app while it runs (```ISystemStatsService```). They come from the operating system. A Vulkan app supplies the memory usage from ```VK_EXT_memory_budget``` where the operating system has no number.
- A failed ```vkAcquireNextImageKHR``` or ```vkQueuePresentKHR``` (for example ```VK_ERROR_DEVICE_LOST```) is logged before the app is restarted.
- When a device is lost the driver is asked why, and its fault report (a description, the faulting memory and instruction addresses and vendor information) is written to the log. This needs ```VK_KHR_device_fault``` or ```VK_EXT_device_fault```, which the host enables when the device supports it (```-v``` logs which one). A app that makes its own Vulkan calls can pass a failed ```VkResult``` to ```ReportDeviceLost``` to get the same report.

## Timing

The host enables a number of optional device extensions that tell a app when things happened. All of them are optional: when the Vulkan
headers, the device or the surface do not have a extension the app runs as before and the objects below report that they are not
supported, so check before relying on a value. ```-v``` logs what was enabled (```Calibrated timestamps```, ```Present timing```).

### When a frame reached the display

With ```VK_EXT_present_timing``` (and ```VK_KHR_present_id2```, ```VK_KHR_calibrated_timestamps```) the swapchain reports when a present was handed to the presentation engine and when its first pixel left for the display. It changes how the swapchain is created, so a ```DemoAppVulkanBasic``` app has to ask for it:

```C++
// In the app, when calling the DemoAppVulkanBasic constructor
VulkanBasic::DemoAppVulkanSetup setup;
setup.PresentTiming = true;

// In VulkanDraw
if (IsPresentTimingEnabled())
{
  // The id the present of the frame being drawn will get
  const uint64_t presentId = GetNextPresentId();
  // The measurements that arrived since the previous frame, a few frames after their present
  for (const Vulkan::VUPresentTimingRecord& record : GetPresentTimings())
  {
    // record.PresentId, record.QueueOperationsEnd, record.GetDisplayTime(): HighResolutionTimer timestamps, empty if not available
  }
}
```

- ```--VkPresentTiming true``` measures the presents of any app (use ```-vvvv``` to log them), ```--VkPresentTiming false``` never uses the extension.
- ```IsPresentTimingSupported()``` says if the device has it, ```IsPresentTimingEnabled()``` if the presents are being measured. The surface has a say as well, so it can change when the swapchain is recreated.
- ```SetPresentTimingRequested``` switches it on or off at runtime. This recreates the swapchain before the next frame.
- The times of a record are converted to the clock of the framework (```HighResolutionTimer```), so they can be compared with the times a app takes itself. A stage is empty when the presentation engine had no time for it, which does not mean that the frame was not shown.
- ```GetPresentRefreshDuration()``` is the duration of a refresh according to the swapchain.
- The values are what the driver reports. Check them against something else before a decision depends on them: see the notes in [FramePacing.md](../FramePacing.md#what-the-vulkan-sample-measures-about-its-presents).

The mechanics are in ```VUSwapchainPresentTiming``` (```FslUtil.Vulkan1_0```) for a app that manages its own swapchain.

### When the GPU worked on a frame

A timestamp query is a time on the clock of the device. With ```VK_KHR_calibrated_timestamps``` (or the EXT version) it can be converted to the clock of the framework, so the work of the GPU can be placed on the same timeline as the work of the CPU:

```C++
// m_calibratedTimestamps is a member of DemoAppVulkan, it reports 'not supported' without the extension
Vulkan::VUGpuTimeCalibration m_gpuTimeCalibration(m_calibratedTimestamps, m_gpuTimer.GetTimestampPeriod(), m_gpuTimer.GetTimestampMask());

// Once per frame: reads both clocks again when the last read is older than this. The two clocks do not run at the same rate,
// so the rate of the device clock is measured from the reads and a timestamp is converted from the newest one.
m_gpuTimeCalibration.CalibrateIfOlderThan(TimeSpan::FromMilliseconds(250));

TickCount gpuStartTime;
if (m_gpuTimeCalibration.TryToHostTime(m_gpuTimer.GetBeginTimestamp(), gpuStartTime))
{
  // gpuStartTime can be compared with HighResolutionTimer::GetTimestamp()
}
```

```VUGpuFrameTimer``` returns the timestamps of the last frame it measured as a ```VUDeviceTimestamp```, a type of its own as a device timestamp is not a time in ticks or nanoseconds.

### Where the frame loop waits

```GetLastPresentCalls()``` of ```DemoAppVulkanBasic``` returns when ```vkAcquireNextImageKHR``` and ```vkQueuePresentKHR``` were called and when they returned for the last frame that was presented. It needs no extension. ```Vulkan.FramePacing``` uses all of the above, and ```--Trace``` writes all of it to a trace for every frame of any Vulkan app, see [FramePacing.md](../FramePacing.md#the-frame-log).

## Known issues

- As most of these samples are simple they currently perform many small Vulkan allocations. This is not something that should be done for a real production workload!
- Most samples refill the command-buffer every frame even when its not strictly necessary.
- The UI meshes are currently rebuild every frame, even when the UI is not modified.
- DemoAppVulkanBasic: The depth buffer format selection can not be modified.
- Package FslUtil.Vulkan1_0: is still a work in progress.
- The physical device feature request mechanism does not support 'grouping' a request like 'TextureCompressionASTC_LDR, TextureCompressionBC or TextureCompressionETC2. To do this the app should set all of them to optional and then check if it got one of them.
- Console based Vulkan apps do not have a DemoHost yet. So they are 100% freestyle apps.
- The samples often chose the 'simple' but not optimal solution for doing things as its simpler to understand. A common example is texture loading which most samples load from the main thread and in sequence not taking advantage of the abilities for Vulkan to do this from multiple threads. Even for the case were we only want to use one thread it has multiple synchronization points to ensure that once the 'load' call returns the texture is ready. For a real production application it would be more optimal to 'batch' the loading requests and then wait for all of them to complete at the end instead. A future update will most likely introduce a more optimal loader that enables this scenario.
- Some samples work on uncompressed textures just because its supported on all platforms and its performance is good enough for whats being demonstrated.
- The ```--VkPresentMode``` is currently ignored by Vulkan samples not based on DemoAppVulkanBasic.
- The ```--Stats``` is currently not supported by Vulkan samples not based on DemoAppVulkanBasic.

Feature       | Description
--------------|---------------
Stats overlay | Are supported on demos that use DemoAppVulkanBasic as a base class and if they call the right methods. Its up to the app developer to ensure its available and tested.
Screenshot    | Screenshots are currently not supported for Vulkan apps.

## Detailed sequence

<a href="Images/DemoAppVulkanBasic_Flow_Detailed.svg">
<img src="Images/DemoAppVulkanBasic_Flow_Detailed.svg">
</a>

## Class diagrams

### Class diagram DemoAppVulkanBasic

<a href="Images/DemoAppVulkanBasic_Classes.svg">
<img src="Images/DemoAppVulkanBasic_Classes.svg">
</a>

## Demo conversion status

The current conversion status for converting the VulkanSamples to use the DemoAppVulkanBasic class.

Name                       |Converted|
---------------------------|---------|
ComputeParticles           |         |
DevBatch                   |    Y    |
DFGraphicsBasic2D          |    Y    |
DFNativeBatch2D            |    Y    |
DFSimpleUI100              |    Y    |
DFSimpleUI101              |    Y    |
DisplacementMapping        |         |
DynamicTerrainTessellation |         |
GammaCorrection            |    Y    |
Gears                      |    Y    |
HDR01_BasicToneMapping     |    Y    |
HDR02_FBBasicToneMapping   |    Y    |
HDR03_SkyboxTonemapping    |    Y    |
InputEvents                |    Y    |
LineBuilder                |    Y    |
MeshInstancing             |         |
OpenCL101                  |    Y    |
OpenCLGaussianFilter       |    Y    |
OpenCV101                  |    Y    |
OpenCVMatToNativeBatch     |    Y    |
OpenCVMatToUI              |    Y    |
OpenVX101                  |    Y    |
Scissor101                 |    Y    |
Skybox                     |    Y    |
TessellationPNTriangles    |         |
TextureCompression         |    Y    |
Texturing                  |         |
TexturingArrays            |         |
TexturingCubeMap           |         |
Triangle                   |    Y    |
Vulkan101                  |    Y    |
VulkanComputeMandelbrot    |         |
VulkanInfo                 |    X    |

X = not relevant
