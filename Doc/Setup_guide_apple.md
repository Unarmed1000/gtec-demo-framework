# Setup guide for macOS (Apple) - experimental

This guide builds the demos with the native Cocoa window system and Vulkan.
On Apple silicon with macOS 26 or newer, Vulkan uses KosmicKrisp, the Vulkan 1.4 conformant driver in the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home#mac).
Intel Macs and older macOS versions can use [MoltenVK](https://github.com/KhronosGroup/MoltenVK) instead (see [Appendix A](#appendix-a-moltenvk-for-intel-macs-and-older-macos-versions)).

The Apple platform is still experimental, so expect some of the demo apps to fail (especially with MoltenVK, which does not support every Vulkan feature, for example geometry shaders).

## Table of contents

* [Prerequisites](#prerequisites)
* [Install the dependencies](#install-the-dependencies)
* [Configure the environment](#configure-the-environment)
* [Verify the Vulkan installation](#verify-the-vulkan-installation)
* [To compile and run an existing Vulkan sample application](#to-compile-and-run-an-existing-vulkan-sample-application)
* [Troubleshooting](#troubleshooting)
* [Appendix A: MoltenVK for Intel Macs and older macOS versions](#appendix-a-moltenvk-for-intel-macs-and-older-macos-versions)
* [Appendix B: Legacy X11 window system (XQuartz + Mesa)](#appendix-b-legacy-x11-window-system-xquartz--mesa)

## Prerequisites

* Apple silicon (M1 or newer) with macOS 26 or newer. For Intel Macs and older macOS versions see [Appendix A](#appendix-a-moltenvk-for-intel-macs-and-older-macos-versions).
* Xcode command line tools

    ```bash
    xcode-select --install
    ```

* [Homebrew](https://brew.sh/) (for the build tools)

## Install the dependencies

Build tools (CMake, Ninja and Python 3.14+)

```bash
brew install cmake ninja python@3.14
```

Vulkan: the [LunarG Vulkan SDK for macOS](https://vulkan.lunarg.com/sdk/home#mac) 1.4.363.0 or newer.
It contains the loader, the headers, the KosmicKrisp and MoltenVK drivers, the validation layers, the Vulkan profiles and the tools.
Download it and run the installer, by default it installs to `~/VulkanSDK/<version>/`.
The installer can also run from the command line:

```bash
vulkansdk-macOS-<version>.app/Contents/MacOS/vulkansdk-macOS-<version> --root ~/VulkanSDK/<version> --accept-licenses --default-answer --confirm-command install
```

Optional: clang-format and clang-tidy 23 (only needed to format and tidy the code with `FslBuildCheck.py`)

```bash
# llvm is currently version 23, once Homebrew moves it to 24 use llvm@23 instead
brew install llvm

# llvm is keg-only (macOS ships its own clang), so add its tools to the path (in every terminal, or in your ~/.zshrc)
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

Make sure that `clang-format --version` and `clang-tidy --version` report version 23.

## Configure the environment

Run this in every terminal you build or run the demos from (or add it to your `~/.zshrc`).
`prepare.sh` automatically selects the `Apple` platform on macOS.

```bash
# Sets VULKAN_SDK (used by CMake to find Vulkan) and points the Vulkan loader at the SDK's drivers and layers
source ~/VulkanSDK/<version>/setup-env.sh

# Only load KosmicKrisp, otherwise the SDK's MoltenVK is enumerated first
export VK_DRIVER_FILES="$VULKAN_SDK/share/vulkan/icd.d/libkosmickrisp_icd.json"

cd gtec-demo-framework
source prepare.sh
```

Make sure that `python3 --version` reports 3.14 or newer.

## Verify the Vulkan installation

```bash
vulkaninfo --summary
```

The output should list your GPU with the KosmicKrisp driver.

## To compile and run an existing Vulkan sample application

```bash
cd DemoApps/Vulkan/Triangle
FslBuild.py
FslBuildRun.py
```

The native Cocoa window system is the default, so no variant needs to be specified.
The window size (`--Window [x,y,width,height]`) is in points (so it is independent of the display scale factor), while the size reported to the app is in pixels.

## Troubleshooting

* CMake can't find Vulkan: check that `VULKAN_SDK` is set (`source ~/VulkanSDK/<version>/setup-env.sh`), then delete the build directory and build again.
* The demo reports no Vulkan physical devices (or `VK_ERROR_INCOMPATIBLE_DRIVER`): check that `VK_DRIVER_FILES` points at an existing `libkosmickrisp_icd.json` and run `vulkaninfo --summary`.
  KosmicKrisp requires Apple silicon and macOS 26 or newer, on other Macs use MoltenVK ([Appendix A](#appendix-a-moltenvk-for-intel-macs-and-older-macos-versions)).
* A demo fails during device or pipeline creation with MoltenVK: it most likely uses a feature that MoltenVK doesn't support.

## Appendix A: MoltenVK for Intel Macs and older macOS versions

[MoltenVK](https://github.com/KhronosGroup/MoltenVK) runs Vulkan on top of Metal on Intel Macs and on macOS 13 or newer.
It is a Vulkan portability implementation and not a fully conformant driver, so some of the demos don't run (for example the ones that use geometry shaders).

### Using the LunarG Vulkan SDK

The SDK also contains MoltenVK. Configure the environment as described in [Configure the environment](#configure-the-environment), but leave out the `VK_DRIVER_FILES` export so the Vulkan loader uses MoltenVK.

### Using Homebrew

Install the Vulkan loader, headers, MoltenVK, validation layers, profiles and tools

```bash
brew install vulkan-headers vulkan-loader molten-vk vulkan-validationlayers vulkan-profiles vulkan-tools
```

Configure the environment in every terminal you build or run the demos from (instead of `setup-env.sh`)

```bash
# Let CMake find the Homebrew packages (/opt/homebrew is not a default CMake search path)
export CMAKE_PREFIX_PATH="$(brew --prefix)${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"

cd gtec-demo-framework
source prepare.sh
```

`vulkaninfo --summary` should list your GPU with `MoltenVK` as the driver.

* CMake can't find Vulkan: check that `CMAKE_PREFIX_PATH` contains `$(brew --prefix)`, then delete the build directory and build again.
* The demo reports no Vulkan physical devices: the Vulkan loader didn't find MoltenVK, try `brew reinstall molten-vk vulkan-loader`.

Don't mix a Homebrew and a LunarG Vulkan installation in the same terminal, as the loader could pick up the wrong driver or layers.

## Appendix B: Legacy X11 window system (XQuartz + Mesa)

The Apple platform supports two window systems, which are selected with the `WindowSystem` variant:

WindowSystem     | Description                                                      | APIs
-----------------|------------------------------------------------------------------|---------------------------
Cocoa (default)  | Native AppKit window with a CAMetalLayer                         | Vulkan (via KosmicKrisp or MoltenVK), console apps
X11              | Legacy XQuartz (X11) + Mesa path (a 'proof of concept')          | OpenGL ES (via Mesa EGL)

The X11 window system is currently required for the OpenGL ES samples as EGL is only available through Mesa.
It is selected by adding `--Variants [WindowSystem=X11]` to the `FslBuild.py` and `FslBuildRun.py` commands.
Vulkan is not supported with the X11 window system, as Vulkan on Apple presents through a CAMetalLayer.

Install XQuartz and Mesa

```bash
# Install XQuartz (X11 server)
brew install --cask xquartz

# Install Mesa (OpenGL implementation)
brew install mesa
```

Prepare a helper script to configure your environment:

```bash
#!/usr/bin/env bash
# setup-x11-mesa-env.sh
# Configure environment variables for building with X11 and Mesa on macOS

# -------------------------------
# X11 (from XQuartz)
# -------------------------------
if [ -d "/opt/X11" ]; then
    export PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export CMAKE_PREFIX_PATH="/opt/X11${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    export LIBRARY_PATH="/opt/X11/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
    export C_INCLUDE_PATH="/opt/X11/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
    export CPLUS_INCLUDE_PATH="/opt/X11/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
else
    echo "Warning: XQuartz not found in /opt/X11"
fi

# -------------------------------
# Mesa (from Homebrew)
# -------------------------------
if command -v brew >/dev/null 2>&1; then
    if brew list mesa &>/dev/null; then
        MESA_DIR="$(brew --prefix mesa)"
        export C_INCLUDE_PATH="${MESA_DIR}/include${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}"
        export CPLUS_INCLUDE_PATH="${MESA_DIR}/include${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}"
        export LIBRARY_PATH="${MESA_DIR}/lib${LIBRARY_PATH:+:$LIBRARY_PATH}"
        export PKG_CONFIG_PATH="${MESA_DIR}/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
        export CMAKE_PREFIX_PATH="${MESA_DIR}${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
    else
        echo "Warning: Mesa not installed via Homebrew"
    fi
else
    echo "Error: Homebrew not found"
fi

# -------------------------------
# Start XQuartz if it's not running
# -------------------------------
if ! pgrep -x "XQuartz" >/dev/null 2>&1; then
    echo "Starting XQuartz..."
    open -a XQuartz
    # Give it a moment to start
    sleep 2
fi
export DISPLAY=:0
```

Then source the script before `prepare.sh` and build with the X11 variant, for example:

```bash
source setup-x11-mesa-env.sh
cd gtec-demo-framework
source prepare.sh
cd DemoApps/GLES2/S01_SimpleTriangle
FslBuild.py --Variants [WindowSystem=X11]
FslBuildRun.py --Variants [WindowSystem=X11]
```
