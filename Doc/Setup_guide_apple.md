# Setup guide for macOS (Apple) - experimental

This guide uses [Homebrew](https://brew.sh/) for all dependencies and builds the demos with the native Cocoa window system and Vulkan via [MoltenVK](https://github.com/KhronosGroup/MoltenVK).

The Apple platform is still experimental, so expect some of the demo apps to fail (MoltenVK does not support every Vulkan feature, for example geometry shaders).

## Table of contents

* [Prerequisites](#prerequisites)
* [Install the dependencies](#install-the-dependencies)
* [Configure the environment](#configure-the-environment)
* [Verify the Vulkan installation](#verify-the-vulkan-installation)
* [To compile and run an existing Vulkan sample application](#to-compile-and-run-an-existing-vulkan-sample-application)
* [Troubleshooting](#troubleshooting)
* [Appendix A: Using the LunarG Vulkan SDK instead of Homebrew](#appendix-a-using-the-lunarg-vulkan-sdk-instead-of-homebrew)
* [Appendix B: Legacy X11 window system (XQuartz + Mesa)](#appendix-b-legacy-x11-window-system-xquartz--mesa)

## Prerequisites

* macOS 13 or newer (Apple silicon or Intel)
* Xcode command line tools

    ```bash
    xcode-select --install
    ```

* [Homebrew](https://brew.sh/)

## Install the dependencies

Build tools (CMake, Ninja and Python 3.14+)

```bash
brew install cmake ninja python@3.14
```

Vulkan: the loader, headers, MoltenVK (the Vulkan driver that runs on top of Metal), validation layers and tools

```bash
brew install vulkan-headers vulkan-loader molten-vk vulkan-validationlayers vulkan-tools
```

Optional: clang-format and clang-tidy 23 (only needed to format and tidy the code with `FslBuildCheck.py`)

```bash
brew install llvm
```

## Configure the environment

Run this in every terminal you build or run the demos from (or add the `export` to your `~/.zshrc`).
`prepare.sh` automatically selects the `Apple` platform on macOS.

```bash
# Let CMake find the Homebrew packages (/opt/homebrew is not a default CMake search path)
export CMAKE_PREFIX_PATH="$(brew --prefix)${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"

cd gtec-demo-framework
source prepare.sh
```

Make sure that `python3 --version` reports 3.14 or newer.

## Verify the Vulkan installation

```bash
vulkaninfo --summary
```

The output should list your GPU with `MoltenVK` as the driver.

## To compile and run an existing Vulkan sample application

```bash
cd DemoApps/Vulkan/Triangle
FslBuild.py
FslBuildRun.py
```

The native Cocoa window system is the default, so no variant needs to be specified.
The window size (`--Window [x,y,width,height]`) is in points (so it is independent of the display scale factor), while the size reported to the app is in pixels.

## Troubleshooting

* CMake can't find Vulkan: check that `CMAKE_PREFIX_PATH` contains `$(brew --prefix)`, then delete the build directory and build again.
* The demo reports no Vulkan physical devices (or `VK_ERROR_INCOMPATIBLE_DRIVER`): the Vulkan loader didn't find MoltenVK. Run `vulkaninfo --summary` to check the installation, and try `brew reinstall molten-vk vulkan-loader`.
* A demo fails during device or pipeline creation: it most likely uses a feature that MoltenVK doesn't support.

## Appendix A: Using the LunarG Vulkan SDK instead of Homebrew

The [LunarG Vulkan SDK for macOS](https://vulkan.lunarg.com/sdk/home#mac) contains the same Vulkan components (loader, MoltenVK, validation layers and tools) and is useful if you need a specific SDK version.

1. Download and run the installer. By default it installs to `~/VulkanSDK/<version>/`.
   The optional 'System Global Installation' also copies the files to `/usr/local`, so the demos can find MoltenVK without any environment setup.
2. Configure the Vulkan environment in each terminal you build or run from (instead of the Homebrew `CMAKE_PREFIX_PATH` export)

    ```bash
    source ~/VulkanSDK/<version>/setup-env.sh
    ```

    This sets `VULKAN_SDK` (used by CMake to find Vulkan) and points the Vulkan loader at MoltenVK and the validation layers.
3. Continue with `source prepare.sh` as described in [Configure the environment](#configure-the-environment).

Don't mix a Homebrew and a LunarG Vulkan installation in the same terminal, as the loader could pick up the wrong driver or layers.

## Appendix B: Legacy X11 window system (XQuartz + Mesa)

The Apple platform supports two window systems, which are selected with the `WindowSystem` variant:

WindowSystem     | Description                                                      | APIs
-----------------|------------------------------------------------------------------|---------------------------
Cocoa (default)  | Native AppKit window with a CAMetalLayer                         | Vulkan (via MoltenVK), console apps
X11              | Legacy XQuartz (X11) + Mesa path (a 'proof of concept')          | OpenGL ES (via Mesa EGL)

The X11 window system is currently required for the OpenGL ES samples as EGL is only available through Mesa.
It is selected by adding `--Variants [WindowSystem=X11]` to the `FslBuild.py` and `FslBuildRun.py` commands.
Vulkan is not supported with the X11 window system as MoltenVK does not support Xlib surfaces.

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
