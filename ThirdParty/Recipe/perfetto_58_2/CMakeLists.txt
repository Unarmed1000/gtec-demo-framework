#################################################################################################################
## Custom CMake build script for the Perfetto C++ tracing SDK.
## The SDK is released as two amalgamated files (perfetto.h and perfetto.cc) without a build script.
#################################################################################################################

cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

project(
  perfetto
  VERSION 58.2.0
  LANGUAGES CXX
)

# Disable project in-source builds
if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
   message(SEND_ERROR "In-source builds are not permitted. Use a separate folder for building.")
endif()

find_package(Threads REQUIRED)

add_library(perfetto STATIC source/perfetto.cc include/perfetto.h)
add_library(perfetto::perfetto ALIAS perfetto)

# Ensure that debug libs are postfixed with a d
set_property(TARGET perfetto PROPERTY DEBUG_POSTFIX d)
set_property(TARGET perfetto PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(perfetto
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(perfetto PUBLIC cxx_std_17)
target_link_libraries(perfetto PUBLIC Threads::Threads)

if (WIN32)
  # The SDK documents these for Windows: it includes the windows headers and uses winsock
  target_compile_definitions(perfetto PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(perfetto PUBLIC ws2_32)
endif()

if (MSVC)
  # perfetto.cc has more sections than a object file holds by default
  target_compile_options(perfetto PRIVATE /bigobj /permissive- /MP)
endif()

if (ANDROID)
  target_link_libraries(perfetto PUBLIC log)
endif()

#############################
# Installation instructions #
#############################

include(GNUInstallDirs)

install(TARGETS perfetto
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
