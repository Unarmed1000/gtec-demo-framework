# Setup guide for Apple devices - very experimental

The current branch is very much a 'proof of concept'.
It will need to take advantage of the new FslBuild Apple platform support and we need to configure the real apple target correctly.

## Lets get started

Install homebrew then install XQuartz, Mesa, CMake and Ninja

```bash
# Install XQuartz (X11 server)
brew install --cask xquartz

# Install Mesa (OpenGL implementation)
brew install mesa

# Install CMake
brew install cmake

brew install ninja
```

Prepare a helper script to configure your environment:

```bash
#!/usr/bin/env bash
# setup-x11-mesa-env.sh
# Configure environment variables for building with X11 and Mesa on macOS

# -------------------------------
# Save current "errexit" state and enable -e
# -------------------------------
SAVED_OPT_E=false
if [[ $- == *e* ]]; then
    SAVED_OPT_E=true
fi
set -e

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
# devil (optional)
# -------------------------------
# if brew list devil &>/dev/null; then
#     DEVIL_DIR="$(brew --prefix devil)"
#     export PKG_CONFIG_PATH="${DEVIL_DIR}/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
# fi

# -------------------------------
# Use GCC-14 (if installed)
# -------------------------------
#if command -v gcc-14 >/dev/null 2>&1; then
#    export CC=gcc-14
#    export CXX=g++-14
#else
#    echo "Warning: gcc-14 not found in PATH, using system compiler"
#fi

# -------------------------------
# Start XQuartz if it's not running
# -------------------------------
if ! pgrep -x "XQuartz" >/dev/null 2>&1; then
    echo "Starting XQuartz..."
    open -a XQuartz
    # Give it a moment to start
    sleep 2
fi

# Export the DISPLAY variable
export DISPLAY=:0
echo "DISPLAY is set to $DISPLAY"

# -------------------------------
# Configure the vulkan environment
# -------------------------------

#pushd ~/VulkanSDK/1.4.321.0
#source ./setup-env.sh
#popd

# -------------------------------
# Show confirmation
# -------------------------------
echo "Configured environment for X11 + Mesa (GCC-14):"
[ -n "${MESA_DIR:-}" ] && echo "  MESA_DIR=${MESA_DIR}"
echo "  PKG_CONFIG_PATH=${PKG_CONFIG_PATH}"
echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
echo "  LIBRARY_PATH=${LIBRARY_PATH}"
echo "  C_INCLUDE_PATH=${C_INCLUDE_PATH}"
echo "  CPLUS_INCLUDE_PATH=${CPLUS_INCLUDE_PATH}"
echo "  CC=${CC:-not set}"
echo "  CXX=${CXX:-not set}"

# -------------------------------
# Change to project directory and source prepare.sh
# -------------------------------
export FSL_PLATFORM_NAME=Apple

if [ -d "gtec-demo-framework" ]; then
    cd gtec-demo-framework
    if [ -f "prepare.sh" ]; then
        echo "Sourcing gtec-demo-framework/prepare.sh..."
        source prepare.sh
    else
        echo "Warning: prepare.sh not found in gtec-demo-framework/"
    fi
else
    echo "Warning: gtec-demo-framework directory not found"
fi

# -------------------------------
# Restore original -e state
# -------------------------------
if [ "$SAVED_OPT_E" = false ]; then
    set +e
fi
```

Once you run this you should be ready to try out this experimental branch.
