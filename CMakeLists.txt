#
# Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.10)

project(StreamlineSample)

# Variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/_bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(DONUT_SHADERS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/framework")
if (MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_ITERATOR_DEBUG_LEVEL=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP")
endif()
set(PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/_package")

# Do not install additional libraries
option(NVRHI_INSTALL "Generate install rules for NVRHI" OFF)
option(JSONCPP_WITH_CMAKE_PACKAGE "" OFF)
option(JSONCPP_WITH_PKGCONFIG_SUPPORT "" OFF)
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/_extra_install")

# Only Latewarp & PCL support whened USE_SL=OFF!
# Additionally, if using Vulkan path with USE_SL=OFF, revert the patch to the donut/nvrhi/Vulkan-Headers repo
# Be sure to clean the build and output directories when toggling this flag
option(USE_SL "Use Streamline" ON)

# Overide donut options
option(STREAMLINE_FEATURE_DLSS_SR   "Include DLSS-SR dll"           ON)
option(STREAMLINE_FEATURE_IMGUI     "Include Imgui dll"             ON)
option(STREAMLINE_FEATURE_NVPERF    "Include NSight Perf SDK dll"   ON)
option(STREAMLINE_FEATURE_REFLEX    "Include Reflex dll"            ON)
option(STREAMLINE_FEATURE_NIS       "Include NIS dll"               ON)
option(STREAMLINE_FEATURE_DEEPDVC   "Include DEEPDVC dll"           ON)
if(USE_SL)
    option(STREAMLINE_FEATURE_DLSS_RR   "Include DLSS-RR dll"           ON)
else()
    option(STREAMLINE_FEATURE_DLSS_RR   "Include DLSS-RR dll"           OFF)
endif()

# DLSS-FG will take precedence over Latewarp if both are enabled at the same time
# Be sure to clean the output dir when toggling flags, as sl.dlss_g.dll will override sl.latewarp.dll
option(STREAMLINE_FEATURE_DLSS_FG   "Include DLSS-FG dll"           ON)
option(STREAMLINE_FEATURE_LATEWARP  "Include Latewarp dll"          OFF)
if(STREAMLINE_FEATURE_DLSS_FG AND STREAMLINE_FEATURE_LATEWARP AND USE_SL)
    message(WARNING "STREAMLINE_FEATURE_DLSS_FG is enabled and will override STREAMLINE_FEATURE_LATEWARP")
endif()

# We've got platform arch in some of our paths.  CMAKE_GENERATOR_PLATFORM/CMAKE_VS_PLATFORM_NAME contain a useful 
# value when using Visual Studio generator (cmake -G), but not when using e.g. ninja
if (CMAKE_GENERATOR_PLATFORM)
    set(STREAMLINE_TARGET_ARCH "${CMAKE_GENERATOR_PLATFORM}")
else()
    set(STREAMLINE_TARGET_ARCH "x64")
    message(WARNING "STREAMLINE_TARGET_ARCH defaulting to ${STREAMLINE_TARGET_ARCH}")
endif()


if(STREAMLINE_FEATURE_DLSS_SR)
    add_compile_definitions(STREAMLINE_FEATURE_DLSS_SR)
endif()
if(STREAMLINE_FEATURE_IMGUI)
    add_compile_definitions(STREAMLINE_FEATURE_IMGUI)
endif()
if(STREAMLINE_FEATURE_NVPERF)
    add_compile_definitions(STREAMLINE_FEATURE_NVPERF)
endif()
if(STREAMLINE_FEATURE_REFLEX)
    add_compile_definitions(STREAMLINE_FEATURE_REFLEX)
endif()
if(STREAMLINE_FEATURE_NIS)
    add_compile_definitions(STREAMLINE_FEATURE_NIS)
endif()
if (STREAMLINE_FEATURE_DLSS_FG)
    add_compile_definitions(STREAMLINE_FEATURE_DLSS_FG)
endif()
if(STREAMLINE_FEATURE_DEEPDVC)
    add_compile_definitions(STREAMLINE_FEATURE_DEEPDVC)
endif()
if(STREAMLINE_FEATURE_LATEWARP)
    add_compile_definitions(STREAMLINE_FEATURE_LATEWARP)
endif()
if (STREAMLINE_FEATURE_DLSS_RR)
    add_compile_definitions(STREAMLINE_FEATURE_DLSS_RR)
endif()

set(STREAMLINE_INSTALL_DIR  "${PACKAGE_DIRECTORY}/_bin" CACHE STRING "Streamline Install Dir")

if(USE_SL)
# Override vulkan dll used by glfw
add_compile_definitions(_GLFW_VULKAN_LIBRARY="sl.interposer.dll")
endif()

# Add libraries
add_subdirectory(donut)
if(USE_SL)
    add_compile_definitions(USE_SL)
    add_subdirectory(streamline)
else()
    add_subdirectory(ngx)
endif()

if(USE_SL)
# Remove d3d linking from donut and relink with streamline
get_target_property(TARGET_LIBRARIES donut_app LINK_LIBRARIES)
LIST(REMOVE_ITEM TARGET_LIBRARIES d3d11)
LIST(REMOVE_ITEM TARGET_LIBRARIES d3d12)
LIST(REMOVE_ITEM TARGET_LIBRARIES dxgi)
set_property(TARGET donut_app PROPERTY LINK_LIBRARIES  ${TARGET_LIBRARIES} )
endif()

# Collect Code
if(USE_SL)
    file(GLOB src_header
        "src/StreamlineSample.h"
        "src/SLWrapper.h"
        "src/NVWrapper.h"
        "src/UIRenderer.h"
        "src/RenderTargets.h"
        "src/UIData.h"
    )
    file(GLOB src_source
        "src/main.cpp"
        "src/StreamlineSample.cpp"
        "src/SLWrapper.cpp"
        "src/NVWrapper.cpp"
    )
    # Find all HLSL files in the src/shaders directory
    set(folder "StreamlineSampleFolder")
    file(GLOB SHADERS "src/*.hlsl")
    # Custom shader compilation command
    donut_compile_shaders_all_platforms(
        TARGET StreamlineSample_shaders
        CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders.cfg
        FOLDER ${folder}
        OUTPUT_BASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/StreamlineSample
    )

else()
    file(GLOB src_header
        "src/StreamlineSample.h"
        "src/NVWrapper.h"
        "src/NGXWrapper.h"
        "src/UIRenderer.h"
        "src/RenderTargets.h"
        "src/UIData.h"
    )
    file(GLOB src_source
        "src/main.cpp"
        "src/StreamlineSample.cpp"
        "src/NGXWrapper.cpp"
        "src/NVWrapper.cpp"
    )
endif()
    file(GLOB src_overrides 
        "src/DeviceManagerOverride/DeviceManagerOverride.h"
        "src/DeviceManagerOverride/DeviceManagerOverride_DX11.cpp"
        "src/DeviceManagerOverride/DeviceManagerOverride_DX12.cpp"
        "src/DeviceManagerOverride/DeviceManagerOverride_VK.cpp"
    )


# Create exe and link
add_executable(StreamlineSample WIN32 ${src_header} ${src_source} ${src_overrides})
source_group("Headers"              FILES ${src_header})
source_group("Sources"              FILES ${src_source})
source_group("DeviceManagerOverride"       FILES ${src_overrides})

# Add AGS
option(AMD_AGS "Add AMD AGS support" OFF)
if(AMD_AGS)
    set(AGS_DIR "${CMAKE_SOURCE_DIR}/amd_ags/ags_lib")
    find_library(AGS_LIB amd_ags_x64 HINTS "${AGS_DIR}/lib" NO_CACHE)
    if(AGS_LIB)
        find_path(AGS_INCLUDE_DIR "amd_ags.h" HINTS "${AGS_DIR}/inc" NO_CACHE)
        find_file(AGS_DLL "amd_ags_x64.dll" HINTS "${AGS_DIR}/lib" NO_CACHE)
        add_library(amd_ags STATIC IMPORTED GLOBAL)
        set_target_properties(amd_ags
            PROPERTIES
            IMPORTED_LOCATION "${AGS_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${AGS_INCLUDE_DIR}")
        add_compile_definitions(AGS_ENABLE)
        install(FILES ${AGS_DLL} DESTINATION "${STREAMLINE_INSTALL_DIR}")
    else()
        comment("Warning: Failed to find AMD AGS")
    endif()
    target_link_libraries(StreamlineSample amd_ags)

    if (WIN32)
        add_custom_target(CopyAGSdll ALL DEPENDS ${AGS_DLL})
        add_custom_command(TARGET CopyAGSdll
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${AGS_DLL} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
            COMMENT "Copied DLL: ${AGS_DLL}")
        add_dependencies(StreamlineSample CopyAGSdll)
    endif ()
endif()

target_link_libraries(StreamlineSample donut_render donut_app donut_engine)
if(USE_SL)
    target_link_libraries(StreamlineSample streamline)
else()
    target_include_directories(StreamlineSample PUBLIC
        donut_render donut_app donut_engine
        ${CMAKE_CURRENT_SOURCE_DIR}/ngx
        ${CMAKE_CURRENT_SOURCE_DIR}/ngx/include
        ${CMAKE_CURRENT_SOURCE_DIR}/nvapi/
    )
    if (${STREAMLINE_TARGET_ARCH} STREQUAL "x64")
        target_link_libraries(StreamlineSample
            ${CMAKE_CURRENT_SOURCE_DIR}/nvapi/amd64/nvapi64.lib
            $<$<CONFIG:Debug>:${NGX_LIB_DIR}/x64/nvsdk_ngx_d_dbg_iterator1.lib>
            $<$<NOT:$<CONFIG:Debug>>:${NGX_LIB_DIR}/x64/nvsdk_ngx_d.lib>
        )
    else()
        message(FATAL_ERROR "Unsupported platform for NGX Latewarp: ${STREAMLINE_TARGET_ARCH}")
    endif()
endif()

set_target_properties(StreamlineSample PROPERTIES FOLDER "Streamline Sample")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT StreamlineSample)
if (USE_SL)
add_dependencies(StreamlineSample StreamlineSample_shaders)
endif()
# Install
install(TARGETS StreamlineSample DESTINATION "${PACKAGE_DIRECTORY}/_bin")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/_bin/shaders" DESTINATION "${PACKAGE_DIRECTORY}/_bin")
install(FILES "${CMAKE_SOURCE_DIR}/media/sponza-plus.scene.json" DESTINATION "${PACKAGE_DIRECTORY}/media")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/media/MetalSphere" DESTINATION "${PACKAGE_DIRECTORY}/media")
install(FILES "${CMAKE_SOURCE_DIR}/media/fonts/DroidSans/DroidSans-Mono.ttf" DESTINATION "${PACKAGE_DIRECTORY}/media/fonts/DroidSans")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/media/glTF-Sample-Models/2.0/Sponza" DESTINATION "${PACKAGE_DIRECTORY}/media/glTF-Sample-Models/2.0")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/media/glTF-Sample-Models/2.0/BrainStem" DESTINATION "${PACKAGE_DIRECTORY}/media/glTF-Sample-Models/2.0")
install(FILES "${CMAKE_SOURCE_DIR}/run.bat" DESTINATION "${PACKAGE_DIRECTORY}")
install(FILES "${CMAKE_SOURCE_DIR}/README.md" DESTINATION "${PACKAGE_DIRECTORY}")
install(FILES "${CMAKE_SOURCE_DIR}/NVIDIA RTX SDKs License (12Apr2021).txt" DESTINATION "${PACKAGE_DIRECTORY}")
install(FILES "${CMAKE_SOURCE_DIR}/release.txt" DESTINATION "${PACKAGE_DIRECTORY}")
