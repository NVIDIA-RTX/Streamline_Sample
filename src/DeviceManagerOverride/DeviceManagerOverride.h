/*
* Copyright (c) 2014-2022, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <donut/app/DeviceManager.h>
#if DONUT_WITH_DX11
#include <donut/app/DeviceManager_DX11.h>
#endif
#if DONUT_WITH_DX12
#include <donut/app/DeviceManager_DX12.h>
#endif
#if DONUT_WITH_VULKAN
#include <donut/app/DeviceManager_VK.h>
#endif

#if DONUT_WITH_DX11
donut::app::DeviceManager* CreateD3D11();
class DeviceManagerOverride_DX11 : public DeviceManager_DX11
{
public:
    DeviceManagerOverride_DX11();
    IDXGIAdapter* GetAdapter();

private:
    bool CreateDevice() final;
    void DestroyDeviceAndSwapChain() final;
    bool BeginFrame() final;

    nvrhi::RefCountPtr<ID3D11Device>            m_Device_native;
};
#endif

#if DONUT_WITH_DX12
donut::app::DeviceManager* CreateD3D12();
class DeviceManagerOverride_DX12 : public DeviceManager_DX12
{
public:
    DeviceManagerOverride_DX12();
    IDXGIAdapter* GetAdapter();

private:
    bool CreateDevice() final;
    void DestroyDeviceAndSwapChain() final;
    bool BeginFrame() final;
    void waitForQueue();

    nvrhi::RefCountPtr<ID3D12Device>            m_Device_native;
};
#endif

#if DONUT_WITH_VULKAN
donut::app::DeviceManager* CreateVK();
class DeviceManagerOverride_VK : public DeviceManager_VK
{
public:
    vk::Instance GetInstance() { return m_VulkanInstance; }
    vk::PhysicalDevice GetPhysicalDevice() { return m_VulkanPhysicalDevice; }
    const donut::app::DeviceCreationParameters& GetDeviceParams() { return m_DeviceParams; }
    DeviceManagerOverride_VK();

private:
    bool CreateDevice() final;
};
#endif
