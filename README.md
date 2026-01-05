# SL Version 2.10.0

# StreamlineSample

This project combines Streamline (https://github.com/NVIDIAGameWorks/Streamline) and Donut (https://github.com/NVIDIAGameWorks/donut) to create a sample app demonstrating a Streamline integration.

There is also an experimental option to enable NGX-only integration instead of using Streamline.

## Prerequisites

- CMake 3.20+
- Windows SDK 10.0.22000+
- Vulkan SDK 1.2.198.1+

## To get this project setup:

1. Ensure you have CMake 3.20+ and the vulkan sdk (https://vulkan.lunarg.com) on your system.
2. Ensure that a VK-compatible dxc.exe is available in your system `PATH`.  The best way to do this is to install a recent (1.2.198.1 or newer) Vulkan SDK from https://www.vulkan.org/ and ensure that its `bin` directory is in the build machine's system `PATH`.
3. Clone this repository, then run in the commandline: `git submodule update --init --recursive`
4. Pick which integration path you want to use
    * Streamline integration: 
        1. Ensure the CMakeLists.txt's option "Use Streamline" is ON (`-DUSE_SL=1` - the default).
        2. Copy the Streamline SDK into the sample's `streamline/` folder
        - If you have built the Streamline SDK from source, you must run the SDK's package.bat script in order to prepare all the Streamline SDK files to be used within the sample app. After running the SDK's package script, the SDK files will be placed in `_sdk`  (unless you specified the -dir commandline option). Copy the entire contents of the `_sdk` folder to the sample app's `streamline` folder.
    * NGX integration (only FrameWarp (Latewarp) and Reflex implementations are currently supported):
        1. Set the CMakeLists.txt's option "Use Streamline" to OFF (`-DUSE_SL=0`), and turn FrameWarp on (`-DSTREAMLINE_FEATURE_LATEWARP=1`)
        *** Please note the disclaimers in the USE_SL=OFF path of NVWrapper.h ***
        *** If using Vulkan path with USE_SL=OFF, revert the patch to the donut/nvrhi/Vulkan-Headers repo ***
        2. Copy the entire contents of the NGX FrameWarp SDK (`include`, `lib`) to the sample app's `ngx` folder.
        3. Copy the entire contents of the nvapi SDK (`amd64`, `x86`) to the sample app's `nvapi` folder. This can be any of the public releases (version R560+)
        4. Though Reflex Low Latency is crucial for best performance with FrameWarp, it is not currently implemented in the NGX integration path, samples for Reflex integration can be found with the public SDK (any version)
    * Be sure to clean cmake artifacts when changing configurations
5. Pick features to enable
    * Compile time feature enabling can be toggled in CMakeLists.txt (many are enabled by default)
    * STREAMLINE_FEATURE_LATEWARP and STREAMLINE_FEATURE_DLSS_FG are mutually exclusive, this will be enforced
    * If Donut/ShaderMake is having trouble locating DXC/FXC or a SPIR-V-enabled DXC, manually specify them (refer to `donut\ShaderMake\CMakeLists.txt`):
        ```powershell
        # Example path to installed Windows SDK
        $winsdk_bin = "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.22621.0\x64"
        cmake -DSHADERMAKE_SEARCH_FOR_COMPILERS=0 -DDXC_PATH="$winsdk_bin/dxc.exe" -DDXC_SPIRV_PATH="$env:VULKAN_SDK/Bin/dxc.exe" -DFXC_PATH="$winsdk_bin/fxc.exe" ...
        ```
6. Use Cmake to make the project solution (or use `make.bat`). If using Streamline integration, Cmake will attempt to locate plugins by searching first the `streamline/bin/x64` and then the `streamline/bin/x64/development` folders for `sl.interposer.dll`. If found, it will copy all SL plugin DLLs from the folder where `sl.interposer.dll` was located.
7. Open the solution and build (or use `build.bat`)
8. Run the executable (or use `run.bat`)

## Integration notes
- D3D11 and D3D12 are integrated using the advanced 'hooking' mechanism by which we have two seperate native/proxy devices and swapchains that are passed into specific api calls. We statically link sl.interposer.lib instead of D3D libs.
- D3D12 additionally features swapchain re-creation when toggling DLSSG. This removes all swapchain interposition when it is not needed.
- Vulkan is integrated using the basic mechanism, by which streamline hooks all of the api calls. We dynamically link sl.
interposer.dll instead of vulkan-1.dll. 
- Runing make.bat with `-AMD_AGS`, adds support for AMD AGS, and shows how devs might go about integrating SL around this. 
- To enable to sl.imgui plugin for debugging, the app must use the development dlls. and the user must manualy place the sl.imgui.dll and sl.interposer.json in the executable directory.

## Useful commandLine arguments: 
Arguments                                                                                 | Effect
---                                                                                       | ---
-vk                                                                                       | Run with Vulkan
-d3d11                                                                                    | Run with D3D11
-d3d12                                                                                    | Run with D3D12
-height 1080                                                                              | Sets height
-fullscreen                                                                               | Sets fullscreen (by default game runs in windowed)
-verbose                                                                                  | Allows vebose info level logging logging
-logToFile                                                                                | Logs to file
-debug                                                                                    | Enables NVRHI and Graphics API validation Layer
-noSigCheck                                                                               | Does not do streamline dll signiture check 
-vsync                                                                                    | Enables Vsync
-sllog                                                                                    | Enables streamline logging
-scene "/myscene.fbx"                                                                     | Loads a custom scene
-maxFrames 100                                                                            | Sets number of frames to render before the app shuts down
-Reflex_mode 1                                                                            | Sets Reflex mode: 1:On 2:Boost
-Reflex_fpsCap 60                                                                         | Sets Refex FPS cap to a given number
-DLSS_mode 1                                                                              | Sets the DLSS mode startup: 0:Off 1:MaxPerf 2:Balanced 3:MaxQual 4:UtraPerf 5:DLAA
-viewport                                                                                 | Specifies backbuffer viewport extent in the format: (offsetLeft,offsetTop,widthxheight) e.g. (320,180,1280x720)
