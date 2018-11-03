/*
Vulkan API loader. Choice of public domain or MIT. See license statements at the end of this file.
vkbind - v<<vulkan_version>>.<<revision>> - <<date>>

David Reid - davidreidsoftware@gmail.com
*/

/*
ABOUT
=====
vkbind is a Vulkan API loader. It includes a full implementation of the Vulkan headers (auto-generated from the Vulkan
spec) so there's no need for any additional libraries or SDKs. Unlike the official headers, the platform-specific
sections are all contained within the same file.


USAGE
=====
vkbind is a single-file library. To use it, do something like the following in one .c file.
    #define VKBIND_IMPLEMENTATION
    #include "vkbind.h"

There is no need to link to any libraries.

Before attempting to call any Vulkan or vkbind APIs you first need to initialize vkbind with vkbInit(). Uninitialize
with vkbUninit(). This will bind statically implemented function pointers to the global vk* names.

Example:

    #define VKBIND_IMPLEMENTATION
    #include "vkbind.h"

    int main()
    {
        VkResult result = vkbInit(NULL);
        if (result != VK_SUCCESS) {
            printf("Failed to initialize vkbind.");
            return -1;
        }

        // Do stuff here.

        vkbUninit();
        return 0;
    }


If your platform does not statically expose all Vulkan APIs you need to load instance-specific APIs using
vkbInitInstanceAPI(), passing in a Vulkan VkInstance object. The returned function pointers can then be bound with
vkbBindAPI(). You can also load the Vulkan API on a per-device basis using vkbInitDeviceAPI() and passing in a Vulkan
VkDevice object. Note that you do not need to bind any APIs to global scope if you don't want to. Instead you can call
the functions directly from the VkbAPI structure.

Example:

    #define VKBIND_IMPLEMENTATION
    #include "vkbind.h"

    int main()
    {
        VkbAPI api;
        VkResult result = vkbInit(&api);
        if (result != VK_SUCCESS) {
            printf("Failed to initialize vkbind.");
            return -1;
        }

        // ... Create your Vulkan instance here (vkCreateInstance) ...

        result = vkbInitInstanceAPI(instance, &api);
        if (result != VK_SUCCESS) {
            printf("Failed to initialize instance API.");
            return -2;
        }

        result = vkbBindAPI(&api);
        if (result != VK_SUCCESS) {
            printf("Failed to bind instance API.");
            return -3;
        }

        // Do stuff here.

        vkbUninit();
        return 0;
    }
*/

#ifndef VKBIND_H
#define VKBIND_H

#ifdef __cplusplus
extern "C" {
#endif

/*
vkbind's vk_platform.h implementation. See: https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#boilerplate-platform-specific-calling-conventions

Here we define VKAPI_ATTR, VKAPI_CALL, VKAPI_PTR and make sure the sized types are defined. Support for more platforms
will be added later. Let me know what isn't supported properly and I'll look into it.
*/
#ifdef _WIN32
#define VKAPI_ATTR
#define VKAPI_CALL  __stdcall
#define VKAPI_PTR   __stdcall
#else
#define VKAPI_ATTR
#define VKAPI_CALL
#define VKAPI_PTR
#endif

/* Here we need to ensure sized types are defined. Only explicitly checking for MSVC for now. */
#ifndef VK_NO_STDINT_H
#if defined(_MSC_VER) && (_MSC_VER < 1600)
    typedef   signed char       int8_t;
    typedef unsigned char       uint8_t;
    typedef   signed short      int16_t;
    typedef unsigned short      uint16_t;
    typedef   signed int        int32_t;
    typedef unsigned int        uint32_t;
    typedef   signed __int64    int64_t;
    typedef unsigned __int64    uint64_t;
#else
    #include <stdint.h>
#endif
#endif  /* VK_NO_STDINT_H */

/* stddef.h is required for size_t */
#ifndef VK_NO_STDDEF_H
    #include <stddef.h>
#endif  /* VK_NO_STDDEF_H */


/*<<vulkan_main>>*/

/*<<vulkan_funcpointers_decl_global>>*/

typedef struct
{
    /*<<vulkan_funcpointers_decl_global:4>>*/
} VkbAPI;


/*
Initializes vkbind and attempts to load APIs statically.

Note that it should be expected that APIs are not necessarilly statically implemented on every platform. This function
will reliably load the following APIs (assuming platform support for the relevant version of Vulkan):
<<safe_global_api_docs>>

vkbInit() will attempt to load every API statically, but for robustness you should not assume this will be successful.
You should instead use vkCreateInstance() to create a Vulkan instance, then parse that to vkbInitInstanceAPI() and use
vkbBindInstanceAPI() if you want to bind the instance API to global scope.

pAPI is optional. On output it will contain pointers to all Vulkan APIs found by the loader.
*/
VkResult vkbInit(VkbAPI* pAPI);

/*
Uninitializes vkbind.

Each call to vkbInit() must be matched up with a call to vkbUninit().
*/
void vkbUninit();

/*
Loads per-instance function pointers into the specified API object.

This does not bind the function pointers to global scope. Use vkbBindAPI() for this.
*/
VkResult vkbInitInstanceAPI(VkInstance instance, VkbAPI* pAPI);

/*
Loads per-device function pointers into the specified API object.

This does not bind the function pointers to global scope. Use vkbBindAPI() for this.

This function only sets device-specific function pointers. The proper way to use this function is to first call
vkbInitInstanceAPI() first, then call vkbInitDeviceAPI() using the same VkbAPI object, like the following.

    VkbAPI api;
    vkbInitInstanceAPI(instance, &api);
    vkbInitDeviceAPI(device, &api);
    vkbBindAPI(&api);   // <-- Optional. Can also call function pointers directly from the "api" object.
    
Use of this function is optional. It is provided only for optimization purposes to avoid the cost of internal dispatching.
*/
VkResult vkbInitDeviceAPI(VkDevice device, VkbAPI* pAPI);

/*
Binds the function pointers in pAPI to global scope.
*/
VkResult vkbBindAPI(const VkbAPI* pAPI);

#ifdef __cplusplus
}
#endif
#endif  /* VKBIND_H */



/******************************************************************************
 ******************************************************************************

 IMPLEMENTATION

 ******************************************************************************
 ******************************************************************************/
#ifdef VKBIND_IMPLEMENTATION
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dlfcn.h>
#endif

typedef void* VkbHandle;
typedef void (* VkbProc)(void);

VkbHandle vkb_dlopen(const char* filename)
{
#ifdef _WIN32
    return (VkbHandle)LoadLibraryA(filename);
#else
    return (VkbHandle)dlopen(filename, RTLD_NOW);
#endif
}

void vkb_dlclose(VkbHandle handle)
{
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose((void*)handle);
#endif
}

VkbProc vkb_dlsym(VkbHandle handle, const char* symbol)
{
#ifdef _WIN32
    return (VkbProc)GetProcAddress((HMODULE)handle, symbol);
#else
    return (VkbProc)dlsym((void*)handle, symbol);
#endif
}


static unsigned int g_vkbInitCount = 0;
static VkbHandle g_vkbVulkanSO = NULL;

VkResult vkbLoadVulkanSO()
{
    size_t i;

    /* TODO: Add support for more platforms here. */
    const char* vulkanSONames[] = {
#if defined(_WIN32)
        "vulkan-1.dll"
#elif defined(__APPLE__)
        /* TODO: Set this if possible. May require compile-time linking. */
#elif defined(__ANDROID__)
        "libvulkan.so"
#else
        "libvulkan.so.1",
        "libvulkan.so"
#endif
    };

    for (i = 0; i < sizeof(vulkanSONames)/sizeof(vulkanSONames[0]); ++i) {
        VkbHandle handle = vkb_dlopen(vulkanSONames[i]);
        if (handle != NULL) {
            g_vkbVulkanSO = handle;
            return VK_SUCCESS;
        }
    }

    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

VkResult vkbBindGlobalAPI()
{
    /*<<load_global_api_funcpointers>>*/

    /*
    We can only safely guarantee that vkGetInstanceProcAddr was successfully returned from dlsym(). The Vulkan specification lists some APIs
    that should always work with vkGetInstanceProcAddr(), so I'm going ahead and doing that here just for robustness.
    */
    if (vkGetInstanceProcAddr == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    /*<<load_safe_global_api>>*/

    return VK_SUCCESS;
}

VkResult vkbInit(VkbAPI* pAPI)
{
    if (g_vkbInitCount == 0) {
        VkResult result = vkbLoadVulkanSO();
        if (result != VK_SUCCESS) {
            return result;
        }

        result = vkbBindGlobalAPI();
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    if (pAPI != NULL) {
        /*<<set_struct_api_from_global>>*/
    }

    g_vkbInitCount += 1;    /* <-- Only increment the init counter on success. */
    return VK_SUCCESS;
}

void vkbUninit()
{
    if (g_vkbInitCount == 0) {
        return;
    }

    g_vkbInitCount -= 1;
    if (g_vkbInitCount == 0) {
        vkb_dlclose(g_vkbVulkanSO);
        g_vkbVulkanSO = NULL;
    }
}

VkResult vkbInitInstanceAPI(VkInstance instance, VkbAPI* pAPI)
{
    if (g_vkbInitCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;  /* vkbind not initialized. */
    }

    if (pAPI == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    pAPI->vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    /*<<load_instance_api>>*/

    return VK_SUCCESS;
}

VkResult vkbInitDeviceAPI(VkDevice device, VkbAPI* pAPI)
{
    if (g_vkbInitCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;  /* vkbind not initialized. */
    }

    if (pAPI == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    /* We need to handle vkGetDeviceProcAddr() in a special way to ensure it's using the device-specific version instead of the per-instance version. */
    if (pAPI->vkGetDeviceProcAddr == NULL) {
        pAPI->vkGetDeviceProcAddr = vkGetDeviceProcAddr;
    }

    if (pAPI->vkGetDeviceProcAddr == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;  /* Don't have access to vkGetDeviceProcAddr(). Make sure vkbInitInstanceAPI() is called first on pAPI. */
    }

    pAPI->vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)pAPI->vkGetDeviceProcAddr(device, "vkGetDeviceProcAddr");
    /*<<load_device_api>>*/

    return VK_SUCCESS;
}

VkResult vkbBindAPI(const VkbAPI* pAPI)
{
    if (g_vkbInitCount == 0) {
        return VK_ERROR_INITIALIZATION_FAILED;  /* vkbind not initialized. */
    }

    if (pAPI == NULL) {
        return vkbBindGlobalAPI();
    }

    /*<<set_global_api_from_struct>>*/

    return VK_SUCCESS;
}

#endif  /* VKBIND_IMPLEMENTATION */



/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT
===============================================================================
Copyright 2018 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/