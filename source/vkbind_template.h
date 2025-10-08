/*
Vulkan API loader. Choice of public domain or MIT-0. See license statements at the end of this file.
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

#ifdef _MSC_VER
    #define VKBIND_INLINE __forceinline
#elif defined(__GNUC__)
    /*
    I've had a bug report where GCC is emitting warnings about functions possibly not being inlineable. This warning happens when
    the __attribute__((always_inline)) attribute is defined without an "inline" statement. I think therefore there must be some
    case where "__inline__" is not always defined, thus the compiler emitting these warnings. When using -std=c89 or -ansi on the
    command line, we cannot use the "inline" keyword and instead need to use "__inline__". In an attempt to work around this issue
    I am using "__inline__" only when we're compiling in strict ANSI mode.
    */
    #if defined(__STRICT_ANSI__)
        #define VKBIND_GNUC_INLINE_HINT __inline__
    #else
        #define VKBIND_GNUC_INLINE_HINT inline
    #endif

    #if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)) || defined(__clang__)
        #define VKBIND_INLINE VKBIND_GNUC_INLINE_HINT __attribute__((always_inline))
    #else
        #define VKBIND_INLINE VKBIND_GNUC_INLINE_HINT
    #endif
#elif defined(__WATCOMC__)
    #define VKBIND_INLINE __inline
#else
    #define VKBIND_INLINE
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

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #if !defined(VKBIND_NO_WIN32_HEADERS)
        #include <windows.h>
    #endif
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
    #if !defined(VKBIND_NO_XLIB_HEADERS)
        #include <X11/Xlib.h>
    #endif
#endif

#if defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
    #if !defined(VKBIND_NO_XLIB_HEADERS)
        #include <X11/extensions/Xrandr.h>
    #endif
#endif

/* We need to define vkbind_Display and vkbind_Window */
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XLIB_XRANDR_EXT)
    #if !defined(VKBIND_NO_XLIB_HEADERS)
        typedef Display       vkbind_Display;
        typedef Window        vkbind_Window;
        typedef VisualID      vkbind_VisualID;
    #else
        typedef void*         vkbind_Display;
        typedef unsigned long vkbind_Window;
        typedef unsigned long vkbind_VisualID;
    #endif
#endif

/*<<vk_video>>*/
/*<<vulkan_main>>*/

/*<<vulkan_struct_initializers>>*/

#ifndef VKBIND_NO_GLOBAL_API
/*<<vulkan_funcpointers_decl_global:extern>>*/
#endif /*VKBIND_NO_GLOBAL_API*/

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
void vkbUninit(void);

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

#ifndef VKBIND_NO_GLOBAL_API
/*<<vulkan_funcpointers_decl_global>>*/
#endif /*VKBIND_NO_GLOBAL_API*/

typedef void* VkbHandle;
typedef void (* VkbProc)(void);

static VkbHandle vkb_dlopen(const char* filename)
{
#ifdef _WIN32
    return (VkbHandle)LoadLibraryA(filename);
#else
    return (VkbHandle)dlopen(filename, RTLD_NOW);
#endif
}

static void vkb_dlclose(VkbHandle handle)
{
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose((void*)handle);
#endif
}

static VkbProc vkb_dlsym(VkbHandle handle, const char* symbol)
{
#ifdef _WIN32
    return (VkbProc)GetProcAddress((HMODULE)handle, symbol);
#else
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
#endif
    return (VkbProc)dlsym((void*)handle, symbol);
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
    #pragma GCC diagnostic pop
#endif
#endif
}


static unsigned int g_vkbInitCount = 0;
static VkbHandle g_vkbVulkanSO = NULL;

static VkResult vkbLoadVulkanSO(void)
{
    size_t i;

    const char* vulkanSONames[] = {
    #if defined(VKBIND_VULKAN_SO)
        VKBIND_VULKAN_SO,
    #endif
    #if defined(_WIN32)
        "vulkan-1.dll"
    #elif defined(__APPLE__)
        /*
        The idea here is that since MoltenVK seems to be the de facto standard for Vulkan on Apple platforms at the moment we'll try
        that first. If Apple ever decides to officially support Vulkan we can perhaps consider dropping it to the bottom of the priority
        list. Not sure if this reasoning is sound, but it makes sense in my head!
        */
        "libMoltenVK.dylib",
        "libvulkan.dylib.1",
        "libvulkan.dylib"
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

static VkResult vkbLoadVulkanSymbols(VkbAPI* pAPI)
{
    /*<<load_global_api_funcpointers>>*/

    /*
    We can only safely guarantee that vkGetInstanceProcAddr was successfully returned from dlsym(). The Vulkan specification lists some APIs
    that should always work with vkGetInstanceProcAddr(), so I'm going ahead and doing that here just for robustness.
    */
    if (pAPI->vkGetInstanceProcAddr == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    /*<<load_safe_global_api>>*/

    return VK_SUCCESS;
}

#ifndef VKBIND_NO_GLOBAL_API
static void vkbInitFromGlobalAPI(VkbAPI* pAPI)
{
    /*<<set_struct_api_from_global>>*/
}
#endif /*VKBIND_NO_GLOBAL_API*/

VkResult vkbInit(VkbAPI* pAPI)
{
    VkResult result;

    /* If the Vulkan SO has not been loaded, do that first thing. */
    if (g_vkbInitCount == 0) {
        result = vkbLoadVulkanSO();
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    #if defined(VKBIND_NO_GLOBAL_API)
    {
        /* Getting here means the global API has been disabled. Therefore the caller *must* provide a VkbAPI object. */
        if (pAPI == NULL) {
            return VK_ERROR_INITIALIZATION_FAILED;
        } else {
            /* Since we don't have global function pointers we'll have to use dlsym() every time we initialize. */
            result = vkbLoadVulkanSymbols(pAPI);
            if (result != VK_SUCCESS) {
                return result;
            }
        }
    }
    #else
    {
        if (pAPI == NULL) {
            if (g_vkbInitCount == 0) {
                VkbAPI api;
                result = vkbInit(&api);
                if (result != VK_SUCCESS) {
                    return result;
                }

                /* The call to vkbInit() will have incremented the reference counter. Bring it back down since we're just going to increment it again later. */
            } else {
                /* The global API has already been bound. No need to do anything here. */
            }
        } else {
            if (g_vkbInitCount == 0) {
                result = vkbLoadVulkanSymbols(pAPI);
                if (result != VK_SUCCESS) {
                    return result;
                }

                result = vkbBindAPI(pAPI);
                if (result != VK_SUCCESS) {
                    return result;
                }
            } else {
                /* The global API has already been bound. We need only retrieve the pointers from the globals. Using dlsym() would be too inefficient. */
                vkbInitFromGlobalAPI(pAPI);
            }
        }
    }
    #endif

    g_vkbInitCount += 1;    /* <-- Only increment the init counter on success. */
    return VK_SUCCESS;
}

void vkbUninit(void)
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

    /*
    We need an instance of vkGetInstanceProcAddr(). If it's not available in pAPI we'll try pulling it
    from global scope.
    */
    if (pAPI->vkGetInstanceProcAddr == NULL) {
        #if !defined(VKBIND_NO_GLOBAL_API)
        {
            pAPI->vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        }
        #endif
    }

    if (pAPI->vkGetInstanceProcAddr == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;  /* We don't have a vkGetInstanceProcAddr(). We need to abort. */
    }

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
        #if !defined(VKBIND_NO_GLOBAL_API)
        {
            pAPI->vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        }
        #endif
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
    if (pAPI == NULL) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

#if defined(VKBIND_NO_GLOBAL_API)
    return VK_ERROR_INITIALIZATION_FAILED;  /* The global API has been disabled. */
#else
    /*<<set_global_api_from_struct>>*/

    return VK_SUCCESS;
#endif
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
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2019 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
