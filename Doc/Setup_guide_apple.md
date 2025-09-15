# Setup guide for Apple devices - very experimental

The current branch is very much a 'proof of concept'.
It will need to take advantage of the new FslBuild Apple platform support and we need to configure the real apple target correctly.

## Lets get started

Install homebrew then install XQuartz, Mesa and CMake

```bash
# Install XQuartz (X11 server)
brew install --cask xquartz

# Install Mesa (OpenGL implementation)
brew install mesa

# Install CMake
brew install cmake

# Install gcc 14
brew install gcc
```

Prepare a helper script to configure your environment:

```bash
#!/bin/bash
# setup-x11-mesa-env.sh
# Configure environment variables for building with X11 and Mesa on macOS

# -------------------------------
# X11 (from XQuartz)
# -------------------------------
export PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:${PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="/opt/X11:${CMAKE_PREFIX_PATH}"
export LIBRARY_PATH="/opt/X11/lib:${LIBRARY_PATH}"
export C_INCLUDE_PATH="/opt/X11/include:${C_INCLUDE_PATH}"
export CPLUS_INCLUDE_PATH="/opt/X11/include:${CPLUS_INCLUDE_PATH}"

# -------------------------------
# Mesa (from Homebrew)
# -------------------------------
MESA_DIR="$(brew --prefix mesa)"
export C_INCLUDE_PATH="${MESA_DIR}/include:${C_INCLUDE_PATH}"
export CPLUS_INCLUDE_PATH="${MESA_DIR}/include:${CPLUS_INCLUDE_PATH}"
export LIBRARY_PATH="${MESA_DIR}/lib:${LIBRARY_PATH}"
export PKG_CONFIG_PATH="${MESA_DIR}/lib/pkgconfig:${PKG_CONFIG_PATH}"
export CMAKE_PREFIX_PATH="${MESA_DIR}:${CMAKE_PREFIX_PATH}"

# -------------------------------
# Use GCC-14
# -------------------------------
export CC=gcc-14
export CXX=g++-14

#--------------------------------
# Start XQuartz if it's not running
#--------------------------------
if ! pgrep -x "XQuartz" > /dev/null; then
    open -a XQuartz
    # Give it a moment to start
    sleep 2
fi


# Export the DISPLAY variable
export DISPLAY=:0
echo "DISPLAY is set to $DISPLAY"

# -------------------------------
# Show confirmation
# -------------------------------
echo "Configured environment for X11 + Mesa (GCC-14):"
echo "  MESA_DIR=${MESA_DIR}"
echo "  PKG_CONFIG_PATH=${PKG_CONFIG_PATH}"
echo "  CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}"
echo "  LIBRARY_PATH=${LIBRARY_PATH}"
echo "  C_INCLUDE_PATH=${C_INCLUDE_PATH}"
echo "  CPLUS_INCLUDE_PATH=${CPLUS_INCLUDE_PATH}"

# -------------------------------
# Change to project directory and source prepare.sh
# -------------------------------
export FSL_PLATFORM_NAME=Apple

cd gtec-demo-framework
source prepare.sh
```

Once you run this you should be ready to try out this experimental branch.
