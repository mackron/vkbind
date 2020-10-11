/*
Demonstrates how to draw a textured quad in Vulkan.

This example is completely flat. The only library it uses is vkbind which is just a Vulkan API loader. The vkbind.h
file contains all of the Vulkan API declarations you need and is therefore a replacement for the official Vulkan
headers. Unlike the official Vulkan headers, vkbind includes all of the platform specific headers as well, as opposed
to keeping them all separate which is just unnecessary.

In a real-world program you would not want to write Vulkan code as it's written in this example. This example is void
of abstractions and functions in order to make it easier to see what's actually going on with Vulkan. The idea is to
show how to use Vulkan, not how to architecture your program.

Currently, only Windows is supported. This example will show you how to connect Vulkan to the Win32 windowing system
which is something almost all programs will want to do, so including that here is something I think is valuable.
*/

/*
The first thing to do is define which platforms you want to use. In our case we're only supporting Win32 for now. This
is done by #define-ing one of the `VK_USE_PLATFORM_*_KHR` symbols before the header. This is standard Vulkan and is 
something you would need to do even if you were using the official headers instead of vkbind. This will enable some
platform-specific functionality that you'll need for displaying something in a window.
*/
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

/*
vkbind is a single file library. Use VKBIND_IMPLEMENTATION to define the implementation section. Make sure this is only
ever done in a single source file. All of the Vulkan symbols will be defined in vkbind.h, so no need for the official
Vulkan headers.
*/
#define VKBIND_IMPLEMENTATION
#include "../../vkbind.h"

/*
Vulkan requires shaders to be specified in a binary format called SPIR-V. Unfortunately I haven't been able to find a
good, small GLSL -> SPIR-V compiler library which would enable me to embed the shader code directly into this file,
which for demonstration purposes would be ideal. Instead I've pre-compiled the shader code with glslangValidator and
embedded the raw binary data into the source file below. Doing it this way means there's no need to worry about
distracting you with file IO. I'm keeping this in a separate file to just keep it separate from the focus of this
example which is the Vulkan API.
*/
#include "02_Textures_VFS.c"    /* <-- Compiled shader code is here. */

/*
This the raw texture data that we'll be uploading to the Vulkan API when we create texture object. The texel encoding
we're using in this example is RGBA8 (or VK_FORMAT_R8G8B8A8_UNORM). We use this format for it's wide spread support.
This texture is a small 2x2 texture. We'll be using nearest-neighbor filtering (also known as point filtering) when
displaying the texture on the quad. Moving counter clockwise starting from the left, the texture should be red, green,
blue, black. The alpha channel is always set to opaque, or 0xFF.
*/
static const uint32_t g_TextureSizeX = 2;
static const uint32_t g_TextureSizeY = 2;
static const uint32_t g_TextureDataRGBA[4] = {
    0xFF0000FF, 0xFF000000, /* Encoding is 0xAABBGGRR. */
    0xFF00FF00, 0xFFFF0000
};

#include <stdio.h>

/*
This is just a standard, and empty, Win32 window procedure for event handling. The contents of this do not matter for
this example, but in a real program you'd handle your input and resizing stuff here. For resizing in particular, you
will likely want to recreate your swapchain (explained below). In this example, however, I'm not doing this because
it would require a function which would mean I wouldn't be able to keep this example flat.
*/
#ifdef _WIN32
static const char* g_WndClassName = "vkbWindowClass";

static LRESULT DefaultWindowProcWin32(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
        {
            PostQuitMessage(0);
            return 0;
        }

        default: break;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL OnDebugReport(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    printf("%s\n", pMessage);

    /* Unused. */
    (void)flags;
    (void)objectType;
    (void)object;
    (void)location;
    (void)messageCode;
    (void)pLayerPrefix;

    return VK_FALSE;
}

/*
And here is the main function. Isn't it good to to have a Vulkan example where you can find main() without having to
search for it?!
*/
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /* Most Vulkan APIs return a result code. It's probably good practice to check these. :) */
    VkResult result;

    /*
    The first thing to do is initialize vkbind. We're specifying a VkbAPI object because we want to initialize some
    instance-specific function pointers (see extension section below). To do this we need this VkbAPI object.
    */
    VkbAPI vk;
    result = vkbInit(&vk);
    if (result != VK_SUCCESS) {
        printf("Failed to initialize vkbind.");
        return result;
    }

    /*
    This is where we create the window. This is not part of Vulkan. The idea is that you create your window, or more
    generally, your "surface", and then specify it when you create your Vulkan surface that will eventually become the
    target for your swapchain.
    */
#ifdef _WIN32
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.cbWndExtra    = sizeof(void*);
    wc.lpfnWndProc   = (WNDPROC)DefaultWindowProcWin32;
    wc.lpszClassName = g_WndClassName;
    wc.hCursor       = LoadCursorA(NULL, MAKEINTRESOURCEA(32512));
    wc.style         = CS_OWNDC | CS_DBLCLKS;
    if (!RegisterClassExA(&wc)) {
        printf("Failed to initialize window class.");
        return -1;
    }

    HWND hWnd = CreateWindowExA(0, g_WndClassName, "Vulkan Tutorial", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, NULL, NULL, NULL, NULL);
    if (hWnd == NULL) {
        printf("Failed to create window.");
        return -1;
    }

    ShowWindow(hWnd, SW_SHOWNORMAL);
#endif

    /*
    This is where we start getting into actual Vulkan programming. The first concept to be aware of is that of the
    "instance". This should be fairly obvious - it's basically just the global object that everything is ultimately
    created from.

    To create an instance, there's two concepts to be aware of: layers and extensions. Vulkan has a layering feature
    whereby certain functionality can be plugged into the API. This example is enabling the standard validation layer
    which you've probably heard of already. If you're enabling a layer or extension, you need to check that it's
    actually supported by the instance or else you'll get an error when trying to create the instance.
    */
    const char* ppDesiredLayers[] = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    const char* ppDesiredExtensions[] = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME  /* This is optional and is used for consuming validation errors. */
    };

    /* I'm using fixed sized arrays here for this example, but in a real program you may want to size these dynamically. */
    const char* ppEnabledLayerNames[32];
    uint32_t enabledLayerCount = 0;
    const char* ppEnabledExtensionNames[32];
    uint32_t enabledExtensionCount = 0;

    /*
    Here is where check for the availability of our desired layers. All layers are optional, so if they aren't
    supported we'll just silently ignore it and keep running - no big deal. In a real program, you'd almost certainly
    want to put this into a function called IsLayerSupported() or similar, but since we're keeping this flat, we'll do
    it right here.

    The first thing to do is retrieve the list of supported layers. You specify an array and capacity, and then call
    vkEnumerateInstanceLayerProperties() to extract the layers. Then you can just loop over each entry and check them
    against our desired layers.
    */
    VkLayerProperties pSupportedLayers[32];
    uint32_t supportedLayerCount = sizeof(pSupportedLayers)/sizeof(pSupportedLayers[0]);
    result = vkEnumerateInstanceLayerProperties(&supportedLayerCount, pSupportedLayers);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        printf("Failed to retrieve layers.");
        return -1;
    }

    for (uint32_t iDesiredLayer = 0; iDesiredLayer < sizeof(ppDesiredLayers)/sizeof(ppDesiredLayers[0]); iDesiredLayer += 1) {
        for (uint32_t iSupportedLayer = 0; iSupportedLayer < supportedLayerCount; iSupportedLayer += 1) {
            if (strcmp(ppDesiredLayers[iDesiredLayer], pSupportedLayers[iSupportedLayer].layerName) == 0) {
                ppEnabledLayerNames[enabledLayerCount++] = ppDesiredLayers[iDesiredLayer];  /* The layer is supported. Enable it. */
                break;
            }
        }
    }

    /*
    Now we do the same with extensions. The extension must be present or else initialization of Vulkan will fail so
    therefore we must ensure our optional extensions are only enabled if present. Some extensions are mandatory which
    means we *want* initialization to fail if they are not present.
    */

    /*
    This extension is required for outputting to a window (or "surface") which is what this example is doing which
    makes this extension mandatory. We cannot continue without this which means we always specify it in our enabled
    extensions list.

    The VK_KHR_WIN32_SURFACE_EXTENSION_NAME is the Win32-specific component to the surface extension. We enable this
    only for the Win32 build. On Windows this extension is mandatory for this example, so again, we always specify it
    in the enabled extensions list and let Vulkan return an error if it's not supported.
    */
    ppEnabledExtensionNames[enabledExtensionCount++] = VK_KHR_SURFACE_EXTENSION_NAME;
#ifdef VK_USE_PLATFORM_WIN32_KHR
    ppEnabledExtensionNames[enabledExtensionCount++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
#endif

    /*
    The extensions below are optional, so we'll selectively enable these based on whether or not it is supported. This
    is a little annoying because we will want to use a dynamic array to store the list of supported extensions since we
    don't really know how many will be returned. If you have used OpenGL in the past you probably remember the way in
    which extensions were retrieved back then: glGetString(GL_EXTENSION). This returns a string of space-delimited
    extension names, but the problem was that some programs decided to make a copy of this string into a fixed sized
    buffer. As time went on and more and more extensions were added, many of these programs broke because those fixed
    sized buffer's weren't big enough to store the whole string! We're not going to be making that same mistake again
    now, are we? ARE WE?!

    To do this, we call vkEnumerateInstanceExtensionProperties() twice. The first time will pass in NULL for the output
    buffer which will cause Vulkan to treat it as a query of the total number of extensions. We'll then use the result
    from that query to determine how large of a buffer we should allocate. Then we'll call that same function again,
    but this time with a valid output buffer which will fill it with actual data.

    We should probably also do this with layers, but since all layers are optional, I'm not as concerned about that.
    */
    VkExtensionProperties* pSupportedExtensions = NULL;
    uint32_t supportedExtensionCount;
    result = vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, NULL);
    if (result != VK_SUCCESS) {
        printf("Failed to query extension count.");
        return -1;
    }

    pSupportedExtensions = (VkExtensionProperties*)malloc(sizeof(*pSupportedExtensions) * supportedExtensionCount);
    if (pSupportedExtensions == NULL) {
        printf("Out of memory.");
        return -1;
    }

    result = vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, pSupportedExtensions);
    if (result != VK_SUCCESS) {
        printf("Failed to retrieve extensions.");
        return -1;
    }

    /* We have the supported extensions. Now selectively enable our optional ones. */
    for (uint32_t iDesiredExtension = 0; iDesiredExtension < sizeof(ppDesiredExtensions)/sizeof(ppDesiredExtensions[0]); iDesiredExtension += 1) {
        for (uint32_t iSupportedExtension = 0; iSupportedExtension < supportedExtensionCount; iSupportedExtension += 1) {
            if (strcmp(ppDesiredExtensions[iDesiredExtension], pSupportedExtensions[iSupportedExtension].extensionName) == 0) {
                ppEnabledExtensionNames[enabledExtensionCount++] = ppDesiredExtensions[iDesiredExtension];  /* The extension is supported. Enable it. */
                break;
            }
        }
    }

    /* We're not going to be using the pSupportedExtensions array anymore so it can be freed. */
    free(pSupportedExtensions);


    /*
    At this point we've selected the layers and extensions we want to enable, so now we can initialize the Vulkan
    instance. You're going to see a lot of this with Vulkan - you want to initialize an object, but before you can, you
    need to do a whole heap of setup beforehand. You'll need to just get used to it.

    Almost all objects in Vulkan are initialized using a info/create pattern where you first define a structure
    containing information about the object you want to initialize, and then you call a "vkCreate*()" function to
    create the actual object. The way to memorize it is like this. Take the type of the object you are creating. In
    this case, "VkInstance". The info structure will be the same, but with "CreateInfo" at the end. So for the example
    of VkInstance, it'll be VkInstanceCreateInfo.

    Each CreateInfo structure takes a type which is specified in the "sType" member. This is also fairly easy to
    remember. It's just "VK_STRUCTURE_TYPE_" and then "*_CREATE_INFO". So for VkInstanceCreateInfo, it's
    "VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO". Personally I don't like this system, and I think it's more intended for
    some future proofing, but in my opinion it's a bit over-engineered. But it is what it is, so let's move on.

    The layers and extensions that were created earlier are specified in the VkInstanceCreateInfo object. You can also
    specify some information about the application, but I don't really know what that's used for so I'm just leaving
    that set to NULL. I'll leave that as an excercise for you to figure that one out.

    The instance is created with vkCreateInstance(). The first parameter is the VkInstanceCreateInfo object that you
    just filled out. The second parameter is a pointer to an object containing callbacks for customizing memory
    allocations. For now you need not worry about that - you can consider that once you've figured out what you're
    doing. For the purpose of this example, it just gets in the way so this will always be set to NULL. All creation
    APIs will allow you to specify this. The final parameter is the VkInstance handle.
    */
    VkInstanceCreateInfo instanceInfo;
    instanceInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext                   = NULL;
    instanceInfo.flags                   = 0;
    instanceInfo.pApplicationInfo        = NULL;
    instanceInfo.enabledLayerCount       = enabledLayerCount;
    instanceInfo.ppEnabledLayerNames     = ppEnabledLayerNames;
    instanceInfo.enabledExtensionCount   = enabledExtensionCount;
    instanceInfo.ppEnabledExtensionNames = ppEnabledExtensionNames;

    VkInstance instance;
    result = vkCreateInstance(&instanceInfo, NULL, &instance);
    if (result != VK_SUCCESS) {
        printf("Failed to create Vulkan instance. Check that your hardware supports Vulkan and you have up to date drivers installed.");
        return -1;
    }

    /*
    If we've made it here it means we've successfully initialized the Vulkan instance. Here is where we do some vkbind
    specific stuff. Below we are going to reference some functions that are specific to an extension. The problem is
    that those APIs can only be retrieved if we have a valid instance. vkbind supports this. We need to call vkbind's
    vkbInitInstanceAPI() and then bind the results to global scope. The binding to global scope is optional, but it
    makes this example cleaner. Binding to global scope is only useful when your application is using only a single
    Vulkan instance. If you're using multiple, you should instead call functions directly from the VkbAPI object.
    */
    result = vkbInitInstanceAPI(instance, &vk);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(instance, NULL);
        printf("Failed to load instance-specific Vulkan APIs.");
        return -1;
    }

    vkbBindAPI(&vk);    /* <-- Bind Vulkan functions to global scope. */


    /*
    When we were setting up the extensions, we specified VK_EXT_DEBUG_REPORT_EXTENSION_NAME as one of our optional
    extensions. Here is where we're going to get this one configured. What this does is allows us to intercept and
    detect errors. Before configuring it we'll need to confirm that it's actually usable.
    */
    VkDebugReportCallbackEXT debugReportCallbackObject;
    for (uint32_t iEnabledExtension = 0; iEnabledExtension < enabledExtensionCount; iEnabledExtension += 1) {
        if (strcmp(ppEnabledExtensionNames[iEnabledExtension], VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
            VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo;
            debugReportCallbackCreateInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
            debugReportCallbackCreateInfo.pNext       = NULL;
            debugReportCallbackCreateInfo.flags       = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
            debugReportCallbackCreateInfo.pfnCallback = OnDebugReport;
            debugReportCallbackCreateInfo.pUserData   = NULL;

            result = vkCreateDebugReportCallbackEXT(instance, &debugReportCallbackCreateInfo, NULL, &debugReportCallbackObject);
            if (result != VK_SUCCESS) {
                printf("WARNING: Failed to create debug report callback.");
            }

            break;
        }
    }


    /*
    You probably remember right at the top where we created the application window. On Win32, this is where we created
    the HWND object. Up until this point, that window has been completely disconnected from Vulkan. Now is the time
    where we go ahead and connect them both up. To form this connection we now introduce the notion of a "surface". A
    surface is actually created in a platform specific manner, which makes perfect sense since we're interacting with
    platform specific objects such HWND, however the object itself (VkSurfaceKHR) is generic (it's just that it's
    created from a platform specific API).

    Think of the surface as representing the thing where the final image will be drawn to, in our case this being the
    application window.

    It's important that we create the surface pretty much immediately after the creation of the Vulkan instance. The
    reason for this is the selection of the physical device in the next section. A physical device may not be able to
    output to the specified window. To check for this we require a surface, which means we need to create it now,
    before enumerating over the physical devices.
    */
    VkSurfaceKHR surface;   /* <-- This is our platform-independant surface object. */

    /*
    On Win32 the creation of the surface works pretty much exactly as one would expect. You just specify the HINSTANCE
    and HWND which should be familiar to anybody whose done Win32 programming before. You can get the HINSTANCE from
    the HWND which is demonstrated below.
    */
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo;
    surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.pNext     = NULL;
    surfaceInfo.flags     = 0;
    surfaceInfo.hinstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
    surfaceInfo.hwnd      = hWnd;
    result = vkCreateWin32SurfaceKHR(instance, &surfaceInfo, NULL, &surface);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(instance, NULL);
        printf("Failed to create a Vulkan surface for the main window.");
        return -1;
    }
#endif

    /*
    At this point we have the Vulkan instance created and debugging set up. Now we can introduce the concept of
    "devices". There's two types of devices - physical and logical. These are pretty much self explanatory. The
    physical device represents a physical hardware device on your computer. A logical device is created from the
    physical device and is essentially a proxy. You'll do everything through a logical device. The physical device is
    basically only used for device selection and enumeration and to create logical devices.

    This example always uses the first enumerated device. I haven't seen anywhere that specifies whether or not the
    first enumerated device should be considered the default device, but this will work well enough for the purposes
    of this example. For demonstration purposes we'll enumerate over each device anyway. For simplicity we're limiting
    this to a fixed number of devices as defined by the pPhysicalDevices array.
    */

    /*
    This retrieves the physical devices and works the same way as layer and extension retrieval. You must call this or
    else you won't be able to get access to a VkPhysicalDevice object for creating the logical device.
    */
    VkPhysicalDevice pPhysicalDevices[16];
    uint32_t physicalDeviceCount = sizeof(pPhysicalDevices) / sizeof(pPhysicalDevices[0]);
    result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, pPhysicalDevices);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        printf("Failed to enumerate physical devices. Check that your hardware supports Vulkan and you have up to date drivers installed.");
        return -1;
    }


    /*
    At this point we have a list of physical devices and now we need to choose one. The selection of a physical device
    depends on whether or not it supports the surface we created earlier. Since this example is outputting to a window,
    this check becomes necessary. If you were doing purely off-screen rendering you wouldn't need to check this.

    This is where we're introduced to the first of the stranger concepts introduced with Vulkan - queue families. When
    you want to execute a command, you don't execute it directly. You instead post it to a queue which is then executed
    at a later stage of your choosing. These queues have certain capabilities which are defined by the queue family.
    The capabilities include things like graphics, compute and transfer. When you create the logical device, you need
    to specify the number of queues you want for each queue family (the capabilities of those queues) that you're
    needing to support.

    To determine the queue family, you need to retrieve a list of supported queue families, which is determined by the
    physical device. Then, you check the capabilities of those queue families. When you find a queue family that
    supports what you need, you can create the logical device. For this example it's quite simple. All we're doing is
    graphics which means we can just use the first queue family that reports support for graphics. When starting out, I
    recommend following this. When you get to more advanced stuff you might be able to use dedicated compute and
    transfer queues if that's what you'd prefer. Note that a queue family supporting graphics must also support
    transfer operations, as defined by the Vulkan spec.
    */
    uint32_t selectedPhysicalDeviceIndex = (uint32_t)-1;    /* Set to -1 so we can determine whether or not a device has been selected. */
    uint32_t selectedQueueFamilyIndex    = (uint32_t)-1;

    for (uint32_t iPhysicalDevice = 0; iPhysicalDevice < physicalDeviceCount; ++iPhysicalDevice) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(pPhysicalDevices[iPhysicalDevice], &properties);

        printf("Physical Device: %s\n", properties.deviceName);
        printf("    API Version: %d.%d\n", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion));

        /*
        Only try selecting the device if we haven't already found one. We don't break from the loop early because we
        want to show all devices to the user, not just those that happened to come before the chosen device.
        */
        if (selectedPhysicalDeviceIndex == (uint32_t)-1) {
            /*
            Before we can determine whether or not the physical device supports our surface, we need to find an appropriate
            queue family which is defined by an index. That's what we do now. For simplicity we're just retrieving a fixed
            maximum number of queue families, but a more robust implementation may want to dynamically allocate this.
            */
            VkQueueFamilyProperties queueFamilyProperties[16];
            uint32_t queueFamilyCount = sizeof(queueFamilyProperties) / sizeof(queueFamilyProperties[0]);
            vkGetPhysicalDeviceQueueFamilyProperties(pPhysicalDevices[iPhysicalDevice], &queueFamilyCount, queueFamilyProperties);

            /*
            Selection of an appropriate queue family is just a matter of checking some flags. We need a graphics queue. For
            simplicity we just use the first one we find.
            */
            uint32_t queueFamilyIndex_Graphics = (uint32_t)-1;
            for (uint32_t iQueueFamily = 0; iQueueFamily < queueFamilyCount; ++iQueueFamily) {
                if ((queueFamilyProperties[iQueueFamily].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                    queueFamilyIndex_Graphics = iQueueFamily;
                    break;
                }
            }


            /* We want to use double buffering which means */
            VkSurfaceCapabilitiesKHR surfaceCaps;
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pPhysicalDevices[iPhysicalDevice], surface, &surfaceCaps);
            if (result != VK_SUCCESS) {
                continue;
            }

            if (surfaceCaps.minImageCount < 2) {
                continue;   /* The surface and device combination do not support at least two images (required for double buffering). */
            }


            /* The physical device needs to support outputting to our surface. To determine support, we need the physical device and queue family. */
            VkBool32 isSupported;
            result = vkGetPhysicalDeviceSurfaceSupportKHR(pPhysicalDevices[iPhysicalDevice], queueFamilyIndex_Graphics, surface, &isSupported);
            if (result == VK_SUCCESS && isSupported == VK_TRUE) {
                selectedPhysicalDeviceIndex = iPhysicalDevice;
                selectedQueueFamilyIndex    = queueFamilyIndex_Graphics;
                /*break;*/ /* Intentionally not breaking here because we want to print all devices, including those we aren't selecting. */
            }
        }
    }

    /*
    If we couldn't find an appropriate physical device or queue family we'll need to abort. Note that a more complex
    program may require multiple queues, so the queue family selection might become a lot more complex.
    */
    if (selectedPhysicalDeviceIndex == (uint32_t)-1 || selectedQueueFamilyIndex == (uint32_t)-1) {
        vkDestroyInstance(instance, NULL);
        return -1;
    }


    /*
    Now that we have our list of physical devices we can create a logical device. The logical device is the what's used
    for interfacing with almost all Vulkan APIs. To create the device we need to specify the queues (and their families)
    that we need.
    */

    /*
    Queue priorities are in the range of [0..1] and work the way you would expect where 0 is lowest priority and 1 is
    highest priority. We're only using a single queue, so just setting this to 1 is fine.
    */
    float pQueuePriorities[1];
    pQueuePriorities[0] = 1;

    /*
    This is where we define how many queues we want to initialize with this device. Queues are not created dynamically;
    they're instead created statically with the device. Queues are grouped by queue families, so you need to specify
    the index of the queue family, and then the number of queues you want to create for that queue family.
    */
    VkDeviceQueueCreateInfo pQueueInfos[1]; /* <-- Set this to the number of queue families being used. */
    pQueueInfos[0].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    pQueueInfos[0].pNext            = NULL;
    pQueueInfos[0].flags            = 0;
    pQueueInfos[0].queueFamilyIndex = selectedQueueFamilyIndex;
    pQueueInfos[0].queueCount       = 1;    /* <-- We're only using a single queue for this example. */
    pQueueInfos[0].pQueuePriorities = pQueuePriorities;

    /*
    Now we're back to extensions. Some extensions are specific to devices. This is relevant for us because we're going
    to need one - the swapchain extension. The swapchain is a critical concept for displaying content on the screen and
    will be explained in a bit. For now, just trust me that you'll need it. This is a mandatory extension for us, so we
    can use a simplified system for this.
    */
    const char* ppEnabledDeviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME /* <-- Mandatory if we're displaying to a window. */
    };

    uint32_t enabledDeviceExtensionCount = sizeof(ppEnabledDeviceExtensionNames) / sizeof(ppEnabledDeviceExtensionNames[0]);
    

    /*
    When we initialize the device we need to specify a set of features that need to be enabled. If we enable a certain
    feature and that feature is not supported, device creation will fail. This is useful if your application has a hard
    requirement for specific features. For our purposes, however, we don't really have that concern since we're just
    doing basic stuff.

    One way to do this is to manually specify the features you need. However, for the purposes of this example, this is
    too tedious. Instead we are going to retrieve the supported features from the physical device and just pass that
    straight through. Since this example is only using a very basic feature set, this will work fine for us.
    */
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(pPhysicalDevices[selectedPhysicalDeviceIndex], &physicalDeviceFeatures);

    /* We now have enough information to create the device. */
    VkDeviceCreateInfo deviceInfo;
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = NULL;
    deviceInfo.flags                   = 0;
    deviceInfo.queueCreateInfoCount    = sizeof(pQueueInfos) / sizeof(pQueueInfos[0]);
    deviceInfo.pQueueCreateInfos       = pQueueInfos;
    deviceInfo.enabledLayerCount       = 0;
    deviceInfo.ppEnabledLayerNames     = NULL;
    deviceInfo.enabledExtensionCount   = enabledDeviceExtensionCount;
    deviceInfo.ppEnabledExtensionNames = ppEnabledDeviceExtensionNames;
    deviceInfo.pEnabledFeatures        = &physicalDeviceFeatures;  /* <-- Setting this to NULL is equivalent to disabling all features. */

    /*
    We now have all the information we need to create a device. The device is created from a physical device which we
    enumerated over earlier.
    */
    VkDevice device;
    result = vkCreateDevice(pPhysicalDevices[selectedPhysicalDeviceIndex], &deviceInfo, NULL, &device);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(instance, NULL);
        printf("Failed to create logical device.");
        return -1;
    }


    /*
    At this point we finally have our device initialized. What we just did was the easy part and before we'll see
    anything on the screen there's still a *lot* more needing to be done. The next concept to introduce is that of the
    swapchain. The swapchain is closely related to the surface. Indeed, you need a surface before you can create a
    swapchain. A swapchain is made up of a number of images which, as the name suggests, are swapped with each other
    when displaying a series of images onto the surface.

    In a double buffered environment, there will be two images in the swapchain. At any given moment, one of those
    images will be displayed on the window, while the other one, which is off-screen, is being drawn to by the graphics
    driver. When the off-screen image is ready to be displayed, the two images are swapped and their roles reversed.

    To create a swapchain, you'll first need a surface which we've already created. You'll also need to determine how
    many images you need in the swap chain. If you're using double buffering you'll need two which is what we'll be
    using in this example.
    */
    VkSurfaceCapabilitiesKHR surfaceCaps;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pPhysicalDevices[selectedPhysicalDeviceIndex], surface, &surfaceCaps);
    if (result != VK_SUCCESS) {
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        printf("Failed to retrieve surface capabilities.");
        return -1;
    }

    VkSurfaceFormatKHR supportedFormats[256];
    uint32_t supportedFormatsCount = sizeof(supportedFormats) / sizeof(supportedFormats[0]);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(pPhysicalDevices[selectedPhysicalDeviceIndex], surface, &supportedFormatsCount, supportedFormats);
    if (result != VK_SUCCESS) {
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        printf("Failed to retrieve physical device surface formats.");
        return -1;
    }

    uint32_t iFormat = 0;
    for (uint32_t i = 0; i < supportedFormatsCount; ++i) {
        if (supportedFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM || supportedFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {   /* <-- Can also use VK_FORMAT_*_SRGB */
            iFormat = i;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext                 = NULL;
    swapchainInfo.flags                 = 0;
    swapchainInfo.surface               = surface;
    swapchainInfo.minImageCount         = 2;                            /* <-- Set this to 2 for double buffering. Triple buffering would be 3, etc. */
    swapchainInfo.imageFormat           = supportedFormats[iFormat].format;
    swapchainInfo.imageColorSpace       = supportedFormats[iFormat].colorSpace;
    swapchainInfo.imageExtent           = surfaceCaps.currentExtent;    /* <-- The size of the images of the swapchain. */
    swapchainInfo.imageArrayLayers      = 1;                            /* <-- I'm not sure in what situation you would ever want to set this to anything other than 1. */
    swapchainInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;                            /* <-- Only used when imageSharingMode is VK_SHARING_MODE_CONCURRENT. */
    swapchainInfo.pQueueFamilyIndices   = NULL;                         /* <-- Only used when imageSharingMode is VK_SHARING_MODE_CONCURRENT. */
    swapchainInfo.preTransform          = surfaceCaps.currentTransform;
    swapchainInfo.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode           = VK_PRESENT_MODE_FIFO_KHR;     /* <-- This is what controls vsync. FIFO must always be supported, so use it by default. */
    swapchainInfo.clipped               = VK_TRUE;                      /* <-- Set this to true if you're only displaying to a window. */
    swapchainInfo.oldSwapchain          = VK_NULL_HANDLE;               /* <-- You would set this if you're creating a new swapchain to replace an old one, such as when resizing a window. */

    VkSwapchainKHR vkSwapchain;
    result = vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &vkSwapchain);
    if (result != VK_SUCCESS) {
        vkDestroyDevice(device, NULL);
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyInstance(instance, NULL);
        printf("Failed to create swapchain.");
        return -2;
    }



    /* Clearing the Swapchain and Presenting */

    // Grab each swapchain image.
    VkImage images[2];
    uint32_t imageCount = sizeof(images) / sizeof(images[0]);
    result = vkGetSwapchainImagesKHR(device, vkSwapchain, &imageCount, images);
    if (result != VK_SUCCESS) {
        printf("Failed to retrieve swapchain images.");
        return -2;
    }

    // Each of the swapchain images needs a view associated with it for use with the framebuffer.
    VkImageView imageViews[2];
    for (int i = 0; i < 2; ++i) {
        VkImageViewCreateInfo imageViewInfo;
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext = NULL;
        imageViewInfo.flags = 0;
        imageViewInfo.image = images[i];
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format = supportedFormats[iFormat].format;
        imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(device, &imageViewInfo, NULL, &imageViews[i]);
        if (result != VK_SUCCESS) {
            printf("Failed to create image views for swapchain images.");
            return -2;
        }
    }

    // Create a semaphore for synchronizing swap chain image swaps. The idea is to pass this to vkAcquireNextImageKHR(), then wait for it to get signaled before
    // drawing anything to it. You can do this by specifying the semaphore in pWaitSemaphore.
    VkSemaphoreCreateInfo semaphoreInfo;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = 0;
    semaphoreInfo.flags = 0;
    VkSemaphore semaphore;
    vkCreateSemaphore(device, &semaphoreInfo, NULL, &semaphore);




    // Renderpass.
    VkAttachmentDescription colorAttachmentDesc[1];
    colorAttachmentDesc[0].flags = 0;
    colorAttachmentDesc[0].format = supportedFormats[iFormat].format;
    colorAttachmentDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachment;
    attachment.attachment = 0;
    attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    uint32_t preservedAttachments[1];
    preservedAttachments[0] = 0;

    VkSubpassDescription subpassDesc;
    subpassDesc.flags = 0;
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.inputAttachmentCount = 0;
    subpassDesc.pInputAttachments = NULL;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &attachment;
    subpassDesc.pResolveAttachments = NULL;
    subpassDesc.pDepthStencilAttachment = NULL;
    subpassDesc.preserveAttachmentCount = sizeof(preservedAttachments) / sizeof(preservedAttachments[0]);
    subpassDesc.pPreserveAttachments = preservedAttachments;

    VkRenderPassCreateInfo renderpassInfo;
    renderpassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.pNext = NULL;
    renderpassInfo.flags = 0;
    renderpassInfo.attachmentCount = sizeof(colorAttachmentDesc) / sizeof(colorAttachmentDesc[0]);
    renderpassInfo.pAttachments = colorAttachmentDesc;
    renderpassInfo.subpassCount = 1;
    renderpassInfo.pSubpasses = &subpassDesc;
    renderpassInfo.dependencyCount = 0;
    renderpassInfo.pDependencies = NULL;

    VkRenderPass renderpass;
    result = vkCreateRenderPass(device, &renderpassInfo, NULL, &renderpass);
    if (result != VK_SUCCESS) {
        printf("Failed to create render pass.");
        return -2;
    }



    // Pipeline.
    VkShaderModuleCreateInfo vertexShaderInfo;
    vertexShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexShaderInfo.pNext = NULL;
    vertexShaderInfo.flags = 0;
    vertexShaderInfo.pCode = (const uint32_t*)vfs_map_file("shaders/02_Textures.glsl.vert.spirv", &vertexShaderInfo.codeSize);

    VkShaderModule vertexShaderModule;
    result = vkCreateShaderModule(device, &vertexShaderInfo, NULL, &vertexShaderModule);
    if (result != VK_SUCCESS) {
        printf("Failed to create shader module.");
        return -2;
    }

    VkShaderModuleCreateInfo fragmentShaderInfo;
    fragmentShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragmentShaderInfo.pNext = NULL;
    fragmentShaderInfo.flags = 0;
    fragmentShaderInfo.pCode = (const uint32_t*)vfs_map_file("shaders/02_Textures.glsl.frag.spirv", &fragmentShaderInfo.codeSize);

    VkShaderModule fragmentShaderModule;
    result = vkCreateShaderModule(device, &fragmentShaderInfo, NULL, &fragmentShaderModule);
    if (result != VK_SUCCESS) {
        printf("Failed to create shader module.");
        return -2;
    }

    VkPipelineShaderStageCreateInfo pipelineStages[2];
    pipelineStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineStages[0].pNext = NULL;
    pipelineStages[0].flags = 0;
    pipelineStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineStages[0].module = vertexShaderModule;
    pipelineStages[0].pName = "main";
    pipelineStages[0].pSpecializationInfo = NULL;
    pipelineStages[1] = pipelineStages[0];
    pipelineStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineStages[1].module = fragmentShaderModule;

    // In this example we are using an interleaved vertex buffer which means we use the same binding, but a different offset. If we
    // were to use separate buffers we would use a different binding, and leave the offset at 0.
    VkVertexInputBindingDescription vertexInputBindingDescriptions[1];
    vertexInputBindingDescriptions[0].binding   = 0;
    vertexInputBindingDescriptions[0].stride    = sizeof(float)*3 + sizeof(float)*3 + sizeof(float)*2;
    vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexInputAttributeDescriptions[3];
    vertexInputAttributeDescriptions[0].location = 0;   // Position
    vertexInputAttributeDescriptions[0].binding  = 0;
    vertexInputAttributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset   = 0;
    vertexInputAttributeDescriptions[1].location = 1;   // Color
    vertexInputAttributeDescriptions[1].binding  = 0;
    vertexInputAttributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset   = sizeof(float)*3;
    vertexInputAttributeDescriptions[2].location = 2;   // Texture Coordinates
    vertexInputAttributeDescriptions[2].binding  = 0;
    vertexInputAttributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[2].offset   = sizeof(float)*3 + sizeof(float)*3;

    VkPipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.pNext = NULL;
    vertexInputState.flags = 0;
    vertexInputState.vertexBindingDescriptionCount = sizeof(vertexInputBindingDescriptions)/sizeof(vertexInputBindingDescriptions[0]);
    vertexInputState.pVertexBindingDescriptions = vertexInputBindingDescriptions;
    vertexInputState.vertexAttributeDescriptionCount = sizeof(vertexInputAttributeDescriptions)/sizeof(vertexInputAttributeDescriptions[0]);
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.pNext = NULL;
    inputAssemblyState.flags = 0;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState;
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = NULL;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = NULL;    // <-- Set to NULL because we are using dynamic viewports.
    viewportState.scissorCount = 1;
    viewportState.pScissors = NULL;     // <-- Set to NULL because we are using dynamic scissor rectangles.

    VkPipelineRasterizationStateCreateInfo rasterizationState;
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.pNext = NULL;
    rasterizationState.flags = 0;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.depthBiasConstantFactor = 0;
    rasterizationState.depthBiasClamp = 1;
    rasterizationState.depthBiasSlopeFactor = 0;
    rasterizationState.lineWidth = 1;

    VkPipelineMultisampleStateCreateInfo multisampleState;
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.pNext = NULL;
    multisampleState.flags = 0;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable = VK_FALSE;
    multisampleState.minSampleShading = 0;
    multisampleState.pSampleMask = NULL;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.pNext = NULL;
    depthStencilState.flags = 0;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE; // <-- Experiment with this.
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front;
    depthStencilState.back;
    depthStencilState.minDepthBounds = 0;
    depthStencilState.maxDepthBounds = 1;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[1];  // <-- One for each color attachment used by the subpass.
    colorBlendAttachmentStates[0].blendEnable = VK_FALSE;
    colorBlendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentStates[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState;
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.pNext = NULL;
    colorBlendState.flags = 0;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendState.attachmentCount = sizeof(colorBlendAttachmentStates)/sizeof(colorBlendAttachmentStates[0]);
    colorBlendState.pAttachments = colorBlendAttachmentStates;
    colorBlendState.blendConstants[0] = 0;
    colorBlendState.blendConstants[1] = 0;
    colorBlendState.blendConstants[2] = 0;
    colorBlendState.blendConstants[3] = 0;

    VkDynamicState dynamicStates[2];
    dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamicState;
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext = NULL;
    dynamicState.flags = 0;
    dynamicState.dynamicStateCount = sizeof(dynamicStates)/sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates = dynamicStates;


    /*
    This example is using only a single texture. Vulkan supports separate textures and samplers (multiple samplers to 1 texture, for example). Since this
    is the more complicated way of doing textures, that's what we're going to do. You can also use a combined image/sampler which might be more useful in
    many situations, but that is easy to figure out on your own (hint: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER).
    */
    VkDescriptorSetLayout descriptorSetLayouts[1];

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[2];
    descriptorSetLayoutBindings[0].binding = 0;
    descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorSetLayoutBindings[0].descriptorCount = 1;                         /* <-- If you had an array of samplers defined in the shader, you would set this to the size of the array. */
    descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;   /* <-- We're only going to be accessing the sampler from the fragment shader. */
    descriptorSetLayoutBindings[0].pImmutableSamplers = NULL;                   /* <-- We'll use dynamic samplers for this example, but immutable samplers could be really useful in which case you'd set them here. */

    descriptorSetLayoutBindings[1].binding = 1;
    descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorSetLayoutBindings[1].descriptorCount = 1;                         /* <-- If you had an array of textures defined in the shader, you would set this to the size of the array. */
    descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;   /* <-- We're only going to be accessing the image from the fragment shader. */
    descriptorSetLayoutBindings[1].pImmutableSamplers = NULL;                   /* <-- Always NULL for images. */

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = NULL;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = sizeof(descriptorSetLayoutBindings) / sizeof(descriptorSetLayoutBindings[0]);
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

    result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayouts[0]);
    if (result != VK_SUCCESS) {
        printf("Failed to create descriptor set layout.");
        return -1;
    }


    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = NULL;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = NULL;

    VkPipelineLayout pipelineLayout;
    result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout);
    if (result != VK_SUCCESS) {
        printf("Failed to create pipeline layout.");
        return 2;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = NULL;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = 2;    // Vertex + Fragment
    pipelineInfo.pStages = pipelineStages;
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pTessellationState = NULL; // <-- Not using tessellation shaders here, so set to NULL.
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pMultisampleState = &multisampleState;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderpass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = 0;
    pipelineInfo.basePipelineIndex = 0;

    VkPipeline vkPipeline;
    result = vkCreateGraphicsPipelines(device, 0, 1, &pipelineInfo, NULL, &vkPipeline);
    if (result != VK_SUCCESS) {
        printf("Failed to create graphics pipeline.");
        return -2;
    }


    // We need two framebuffers - one for each image in the swapchain.
    VkFramebuffer framebuffers[2];
    for (int i = 0; i < 2; ++i) {
        VkImageView framebufferAttachments[1];
        framebufferAttachments[0] = imageViews[i];

        VkFramebufferCreateInfo framebufferInfo;
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.pNext = NULL;
        framebufferInfo.flags = 0;
        framebufferInfo.renderPass = renderpass;
        framebufferInfo.attachmentCount = sizeof(framebufferAttachments)/sizeof(framebufferAttachments[0]);
        framebufferInfo.pAttachments = framebufferAttachments;
        framebufferInfo.width = surfaceCaps.currentExtent.width;
        framebufferInfo.height = surfaceCaps.currentExtent.height;
        framebufferInfo.layers = 1;
        result = vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i]);
        if (result != VK_SUCCESS) {
            printf("Failed to create framebuffer.");
            return -2;
        }
    }



    // Command queue.
    VkCommandPoolCreateInfo cmdpoolInfo;
    cmdpoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdpoolInfo.pNext = NULL;
    cmdpoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cmdpoolInfo.queueFamilyIndex = selectedQueueFamilyIndex;

    VkCommandPool vkCommandPool;
    result = vkCreateCommandPool(device, &cmdpoolInfo, NULL, &vkCommandPool);
    if (result != VK_SUCCESS) {
        printf("Failed to create command pool.");
        return -2;
    }

    VkCommandBufferAllocateInfo cmdbufferInfo;
    cmdbufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdbufferInfo.pNext = NULL;
    cmdbufferInfo.commandPool = vkCommandPool;
    cmdbufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdbufferInfo.commandBufferCount = 1;

    VkCommandBuffer vkCmdBuffer;
    result = vkAllocateCommandBuffers(device, &cmdbufferInfo, &vkCmdBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate command buffer.");
        return -2;
    }

    VkQueue vkQueue;
    vkGetDeviceQueue(device, selectedQueueFamilyIndex, 0, &vkQueue); // <-- TODO: Change "0" to a variable.



    // Memory and Buffers
    //
    // Each device can use different types of memory. You retrieve these from the physical device (VkPhysicalDevice) using
    // vkGetPhysicalDeviceMemoryProperties(). The main two we care about is VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT and
    // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT. The former represents memory on the device (your video memory on your GPU) and
    // the later represents your normal system RAM.
    //
    // A buffer (VkBuffer) is bound to memory (VkDeviceMemory) using vkBindBufferMemory().
    //
    // In this example we are using the same buffer for both vertex and index data, with the idea being that we store them
    // both in a single memory allocation.




    // Geometry data. For this example it's simple - it's just 3 floats for position, 3 floats for color, 2 floats for texture
    // coordinates, interleaved. The hard part is allocating memory and uploading it to a device local buffer.
    float geometryVertexData[] = {
        -0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f
    };

    uint32_t geometryIndexData[] = {
        0, 1, 2, 2, 3, 0
    };

    VkBufferCreateInfo bufferInfo;
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = NULL;
    bufferInfo.flags = 0;
    bufferInfo.size = sizeof(geometryVertexData) + sizeof(geometryIndexData);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;   // <-- Ignored when sharingMode is not VK_SHARING_MODE_CONCURRENT.
    bufferInfo.pQueueFamilyIndices = NULL;

    VkBuffer vkBuffer;
    result = vkCreateBuffer(device, &bufferInfo, NULL, &vkBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create buffer for geometry.");
        return -2;
    }

    VkMemoryRequirements bufferReqs;
    vkGetBufferMemoryRequirements(device, vkBuffer, &bufferReqs);

    VkMemoryAllocateInfo bufferMemoryInfo;
    bufferMemoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferMemoryInfo.pNext = NULL;
    bufferMemoryInfo.allocationSize = bufferReqs.size;
    bufferMemoryInfo.memoryTypeIndex = (uint32_t)-1;    // <-- Set to -1 initially so we can check whether or not a valid memory type was found.

    // The memory type index is found from the physical device memory properties.
    VkPhysicalDeviceMemoryProperties memoryProps;
    vkGetPhysicalDeviceMemoryProperties(pPhysicalDevices[selectedPhysicalDeviceIndex], &memoryProps);
    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; ++i) {
        if ((bufferReqs.memoryTypeBits & (1U << i)) != 0) {
            // The bit is set which means the buffer can be used with this type of memory, but we want to make sure this buffer contains the memory type we want.
            if ((memoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                bufferMemoryInfo.memoryTypeIndex = i;
                break;
            }
        }
    }

    VkDeviceMemory vkBufferMemory;
    result = vkAllocateMemory(device, &bufferMemoryInfo, NULL, &vkBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate memory.");
        return -2;
    }

    result = vkBindBufferMemory(device, vkBuffer, vkBufferMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind buffer memory.");
        return -2;
    }

    // To get the data from our regular old C buffer into the VkBuffer we just need to map the VkBuffer, memcpy(), then unmap.
    void* pMappedBufferData;
    result = vkMapMemory(device, vkBufferMemory, 0, bufferReqs.size, 0, &pMappedBufferData);
    if (result != VK_SUCCESS) {
        printf("Failed to map buffer.");
        return -2;
    }

    memcpy(pMappedBufferData, geometryVertexData, sizeof(geometryVertexData));
    memcpy((void*)(((char*)pMappedBufferData) + sizeof(geometryVertexData)), geometryIndexData, sizeof(geometryIndexData));

    vkUnmapMemory(device, vkBufferMemory);
    


    /*
    Textures. Prepare yourself.

    In Vulkan, textures are referred to as an Image. In order to apply a texture to a piece of geometry and display it, there's three concepts
    to be aware of:
    
       1) Images - Think of this as a handle to the raw image data.
       2) Image Views - This is used to retrieve and reinterpret image data.
       3) Samplers - This is used by the shader to determine how to apply filtering to the image for display.

    Before you can create an image view, you'll need to create the image. Before you can do anything with the image, you need to allocate memory
    for it. Once you've allocated the image's memory, you need to fill it with image data which you can do using a staging buffer with host visible
    memory. To copy the data from the staging buffer to the image's memory, you need to run a command.
    */
    VkImageCreateInfo imageCreateInfo;
    imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                 = NULL;
    imageCreateInfo.flags                 = 0;
    imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                = VK_FORMAT_R8G8B8A8_UNORM;
    imageCreateInfo.extent.width          = g_TextureSizeX;
    imageCreateInfo.extent.height         = g_TextureSizeY;
    imageCreateInfo.extent.depth          = 1;
    imageCreateInfo.mipLevels             = 1;  /* We're not doing mipmapping in this example, but if we were you'd want to set this to > 1. */
    imageCreateInfo.arrayLayers           = 1;
    imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage                 = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;   /* SAMPLED for shader access, TRANSFER_DST because it'll be the destination for the filling from the staging buffer. */
    imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount = 0;
    imageCreateInfo.pQueueFamilyIndices   = NULL;   /* <-- Only used when sharingMode = VK_SHARING_MODE_CONCURRENT. */
    imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    result = vkCreateImage(device, &imageCreateInfo, NULL, &image);
    if (result != VK_SUCCESS) {
        printf("Failed to create image.");
        return -1;
    }

    /*
    With the image created we can now allocate some memory for the actual image data and then bind it to the image handle. Not doing this will
    result in a crash when creating the image view (and probably anything else that tries to reference the image). Note that allocating memory
    does not actually fill it with any meaningful data. That needs to be done afterwards with the use of a staging buffer.
    */
    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(device, image, &imageMemoryRequirements);

    VkMemoryAllocateInfo imageAllocateInfo;
    imageAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAllocateInfo.pNext           = NULL;
    imageAllocateInfo.allocationSize  = imageMemoryRequirements.size;
    imageAllocateInfo.memoryTypeIndex = (uint32_t)-1;    // <-- Set to a proper value below.

    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; ++i) {
        if ((imageMemoryRequirements.memoryTypeBits & (1U << i)) != 0) {
            if ((memoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {  /* DEVICE_LOCAL = GPU memory. */
                imageAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
    }

    VkDeviceMemory imageMemory;
    result = vkAllocateMemory(device, &imageAllocateInfo, NULL, &imageMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate image memory.");
        return -1;
    }

    result = vkBindImageMemory(device, image, imageMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind iamge memory.");
        return -1;
    }

    /*
    At this point we have created the image handle and allocated some memory for it. We now need to fill the memory with actual image data. To do this
    we use a staging buffer and then copy it over using a command.
    */
    VkBufferCreateInfo stagingBufferCreateInfo;
    stagingBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferCreateInfo.pNext                 = NULL;
    stagingBufferCreateInfo.flags                 = 0;
    stagingBufferCreateInfo.size                  = imageAllocateInfo.allocationSize;
    stagingBufferCreateInfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;   /* Will be the source of a transfer. */
    stagingBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    stagingBufferCreateInfo.queueFamilyIndexCount = 0;
    stagingBufferCreateInfo.pQueueFamilyIndices   = NULL;

    VkBuffer stagingBuffer;
    result = vkCreateBuffer(device, &stagingBufferCreateInfo, NULL, &stagingBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create staging buffer.");
        return -1;
    }

    /* Allocate memory for the staging buffer. */
    vkGetBufferMemoryRequirements(device, stagingBuffer, &bufferReqs);

    VkMemoryAllocateInfo stagingBufferAllocateInfo;
    stagingBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingBufferAllocateInfo.pNext = NULL;
    stagingBufferAllocateInfo.allocationSize = bufferReqs.size;
    stagingBufferAllocateInfo.memoryTypeIndex = (uint32_t)-1;   // <-- Set to a proper value below.

    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; ++i) {
        if ((bufferReqs.memoryTypeBits & (1U << i)) != 0) {
            if ((memoryProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                stagingBufferAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
    }

    VkDeviceMemory stagingBufferMemory;
    result = vkAllocateMemory(device, &stagingBufferAllocateInfo, NULL, &stagingBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate memory.");
        return -2;
    }

    /* Memory is allocated for the staging buffer. Now copy our image data into it. */
    void* pStagedBufferData;
    result = vkMapMemory(device, stagingBufferMemory, 0, bufferReqs.size, 0, &pStagedBufferData);
    if (result != VK_SUCCESS) {
        printf("Failed to map staging buffer memory.");
        return -2;
    }

    memcpy(pStagedBufferData, g_TextureDataRGBA, sizeof(g_TextureDataRGBA));

    vkUnmapMemory(device, stagingBufferMemory);

    /* The member can now be bound to the buffer. We could have also done this before the map/unmap part. */
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

    /*
    At this point we have our image data in our staging buffer. Now we need to copy the data from the staging buffer into the texture. The texture's data
    is stored on the GPU (device local), which means we need to do this by running a command (can't just copy from host memory to device memory using
    memcpy()). I'm just using the same command object we created earlier.
    */
    {
        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = NULL;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = NULL;
        vkBeginCommandBuffer(vkCmdBuffer, &beginInfo);
        {
            VkImageMemoryBarrier imageMemoryBarrier;
            imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.pNext                           = NULL;
            imageMemoryBarrier.srcAccessMask                   = 0;  /* <-- Don't need to wait for any memory to be avaialable before starting the transfer. */
            imageMemoryBarrier.dstAccessMask                   = 0;  /* <-- As above. */
            imageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex             = selectedQueueFamilyIndex;
            imageMemoryBarrier.dstQueueFamilyIndex             = selectedQueueFamilyIndex;
            imageMemoryBarrier.image                           = image;
            imageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
            imageMemoryBarrier.subresourceRange.levelCount     = 1;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
            imageMemoryBarrier.subresourceRange.layerCount     = 1;
            vkCmdPipelineBarrier(vkCmdBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  /* <-- Wait for nothing. */
                VK_PIPELINE_STAGE_TRANSFER_BIT,     /* <-- Block the transfer stage until our layout transition is complete. */
                0,
                0, NULL,
                0, NULL,
                1, &imageMemoryBarrier
            );

            VkBufferImageCopy region;
            region.bufferOffset                    = 0;
            region.bufferRowLength                 = g_TextureSizeX;
            region.bufferImageHeight               = g_TextureSizeY;
            region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel       = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount     = 1;
            region.imageOffset.x                   = 0;
            region.imageOffset.y                   = 0;
            region.imageOffset.z                   = 0;
            region.imageExtent.width               = g_TextureSizeX;
            region.imageExtent.height              = g_TextureSizeY;
            region.imageExtent.depth               = 1;
            vkCmdCopyBufferToImage(vkCmdBuffer, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            /*
            When the command above has been executed, the image data should have been copied. We now need to transition the image to a layout
            usable by the fragment shader.
            */
            imageMemoryBarrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;  /* <-- Wait for transfer writes to become available. */
            imageMemoryBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;     /* <-- Make sure memory is available for reading by shaders at the end of the barrier. */
            imageMemoryBarrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcQueueFamilyIndex             = selectedQueueFamilyIndex;
            imageMemoryBarrier.dstQueueFamilyIndex             = selectedQueueFamilyIndex;
            imageMemoryBarrier.image                           = image;
            imageMemoryBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
            imageMemoryBarrier.subresourceRange.levelCount     = 1;
            imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
            imageMemoryBarrier.subresourceRange.layerCount     = 1;
            vkCmdPipelineBarrier(vkCmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,         /* <-- Wait for the transfer stage to complete. */
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  /* <-- We access this image via the fragment shader, so don't start fragment shading until the transition is complete. This doesn't actually matter for this example, but good practice. */
                0,
                0, NULL,
                0, NULL,
                1, &imageMemoryBarrier
            );
        }
        vkEndCommandBuffer(vkCmdBuffer);

        VkSubmitInfo submitInfo;
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                = NULL;
        submitInfo.waitSemaphoreCount   = 0;
        submitInfo.pWaitSemaphores      = NULL;
        submitInfo.pWaitDstStageMask    = NULL;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &vkCmdBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores    = NULL;
        result = vkQueueSubmit(vkQueue, 1, &submitInfo, 0);
        if (result != VK_SUCCESS) {
            printf("Failed to submit buffer.");
            return -2;
        }

        vkQueueWaitIdle(vkQueue);
    }




    VkImageViewCreateInfo imageViewCreateInfo;
    imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.pNext                           = NULL;
    imageViewCreateInfo.flags                           = 0;
    imageViewCreateInfo.image                           = image;
    imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
    imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    imageViewCreateInfo.subresourceRange.levelCount     = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView;
    result = vkCreateImageView(device, &imageViewCreateInfo, NULL, &imageView);
    if (result != VK_SUCCESS) {
        printf("Failed to create image view.");
        return -1;
    }


    /* The sampler can be created independently of the image and image view, but we'll need it in order to see the texture when drawing. */
    VkSamplerCreateInfo samplerCreateInfo;
    samplerCreateInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext                   = NULL;
    samplerCreateInfo.flags                   = 0;
    samplerCreateInfo.magFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter               = VK_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;    /* We're using a low resolution texture of 2x2 so we'll want to use point filtering rather than linear. */
    samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.mipLodBias              = 0;
    samplerCreateInfo.anisotropyEnable        = VK_FALSE;  /* We're not using anisotropic filtering for this example. */
    samplerCreateInfo.maxAnisotropy           = 1;
    samplerCreateInfo.compareEnable           = VK_FALSE;
    samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.minLod                  = 0;
    samplerCreateInfo.maxLod                  = 0;
    samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;   /* Set to VK_FALSE for [0, 1) range. Set to VK_TRUE for [0, texture width) range. */
    
    VkSampler sampler;
    result = vkCreateSampler(device, &samplerCreateInfo, NULL, &sampler);
    if (result != VK_SUCCESS) {
        printf("Failed to create sampler.");
        return -1;
    }


    /*
    At this point we have the image and the sampler, but we're still not done. We need to actually create the descriptor sets which will eventually be
    bound with vkCmdBindDescriptorSets() before we draw our geometry. When we created the pipeline, we specified the *layout* of the descriptor sets,
    but now we need to create the actual descriptor sets.

    To create the descriptor sets we first need a descritor pool. The descriptor pool is where the descriptor sets are allocated.
    */
    VkDescriptorPoolSize descriptorPoolSizes[2];
    descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorPoolSizes[0].descriptorCount = 1; /* This example only has 1 sampler. */
    descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorPoolSizes[1].descriptorCount = 1; /* This example only has 1 image. */

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = NULL;
    descriptorPoolCreateInfo.flags = 0;     /* Could set this to VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT if we wanted to be able to free an individual descriptor set, but we're not doing that in this example. */
    descriptorPoolCreateInfo.maxSets = 1;   /* We're only going to be allocating a single descriptor set. */
    descriptorPoolCreateInfo.poolSizeCount = sizeof(descriptorPoolSizes) / sizeof(descriptorPoolSizes[0]);
    descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSizes;

    VkDescriptorPool descriptorPool;
    result = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool);
    if (result != VK_SUCCESS) {
        printf("Failed to create descriptor pool.");
        return -1;
    }

    /* We have the descriptor pool, so now we can create our descriptor set. */
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = NULL;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = 1;   /* <-- This is the size of the pSetLayouts array, and also the number of descriptor sets we want to allocate. */
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts;

    VkDescriptorSet descriptorSets[1];
    result = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSets);
    if (result != VK_SUCCESS) {
        printf("Failed to create descriptor sets.");
        return -1;
    }

    /* We have the descriptor sets, so now we need to associate the images with them. */
    VkDescriptorImageInfo descriptorSamplerInfo;
    descriptorSamplerInfo.sampler     = sampler;
    descriptorSamplerInfo.imageView   = VK_NULL_HANDLE;
    descriptorSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkDescriptorImageInfo descriptorImageInfo;
    descriptorImageInfo.sampler     = VK_NULL_HANDLE;
    descriptorImageInfo.imageView   = imageView;
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorSetWrites[2];
    descriptorSetWrites[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorSetWrites[0].pNext            = NULL;
    descriptorSetWrites[0].dstSet           = descriptorSets[0];
    descriptorSetWrites[0].dstBinding       = 0;
    descriptorSetWrites[0].dstArrayElement  = 0;
    descriptorSetWrites[0].descriptorCount  = 1;
    descriptorSetWrites[0].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorSetWrites[0].pImageInfo       = &descriptorSamplerInfo;
    descriptorSetWrites[0].pBufferInfo      = NULL;
    descriptorSetWrites[0].pTexelBufferView = NULL;

    descriptorSetWrites[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorSetWrites[1].pNext            = NULL;
    descriptorSetWrites[1].dstSet           = descriptorSets[0];
    descriptorSetWrites[1].dstBinding       = 1;
    descriptorSetWrites[1].dstArrayElement  = 0;
    descriptorSetWrites[1].descriptorCount  = 1;
    descriptorSetWrites[1].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorSetWrites[1].pImageInfo       = &descriptorImageInfo;
    descriptorSetWrites[1].pBufferInfo      = NULL;
    descriptorSetWrites[1].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(device, sizeof(descriptorSetWrites) / sizeof(descriptorSetWrites[0]), descriptorSetWrites, 0, NULL);


    /* Main loop. */
    for (;;) {
#ifdef _WIN32
        MSG msg;
        if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;  /* Received a quit message. */
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
#endif
     
        uint32_t imageIndex;
        result = vkAcquireNextImageKHR(device, vkSwapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &imageIndex);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            return -1;
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Here is where you might want to recreate the swapchain. I'm not doing this in this here because I want to
            // keep the code flat and this would require function-izing the swapchain initialization stuff which I don't
            // want to do in this example.
            //
            // You may get this error when you try resizing the window. This happens on Nvidia, but not Intel. I have not
            // tested AMD.
        }


        // Transition the layout of the swapchain image first.
        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = NULL;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = NULL;
        vkBeginCommandBuffer(vkCmdBuffer, &beginInfo);
        {
            VkClearValue clearValues[1];
            clearValues[0].color.float32[0] = 0.2f;
            clearValues[0].color.float32[1] = 0;
            clearValues[0].color.float32[2] = 0;
            clearValues[0].color.float32[3] = 1;

            VkRenderPassBeginInfo renderpassBeginInfo;
            renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderpassBeginInfo.pNext = NULL;
            renderpassBeginInfo.renderPass = renderpass;
            renderpassBeginInfo.framebuffer = framebuffers[imageIndex];
            renderpassBeginInfo.renderArea.offset.x = 0;
            renderpassBeginInfo.renderArea.offset.y = 0;
            renderpassBeginInfo.renderArea.extent = surfaceCaps.currentExtent;
            renderpassBeginInfo.clearValueCount = 1;
            renderpassBeginInfo.pClearValues = clearValues;
            vkCmdBeginRenderPass(vkCmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            {
                VkViewport viewport;
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)surfaceCaps.currentExtent.width;
                viewport.height = (float)surfaceCaps.currentExtent.height;
                viewport.minDepth = 0;
                viewport.maxDepth = 1;
                vkCmdSetViewport(vkCmdBuffer, 0, 1, &viewport);

                VkRect2D scissor;
                scissor.offset.x = 0;
                scissor.offset.y = 0;
                scissor.extent.width = surfaceCaps.currentExtent.width;
                scissor.extent.height = surfaceCaps.currentExtent.height;
                vkCmdSetScissor(vkCmdBuffer, 0, 1, &scissor);

                
                /* Bind the pipeline first. You can bind descriptor sets, vertex buffers and index buffers before binding the pipeline, but I like to do the pipeline first. */
                vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

                /* We need to bind the descriptor sets before we'll be able to use the texture in the shader. */
                vkCmdBindDescriptorSets(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, sizeof(descriptorSets) / sizeof(descriptorSets[0]), descriptorSets, 0, NULL);

                /* Vertex and index buffers need to be bound before drawing. */
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(vkCmdBuffer, 0, 1, &vkBuffer, &offset);
                vkCmdBindIndexBuffer(vkCmdBuffer, vkBuffer, sizeof(geometryVertexData), VK_INDEX_TYPE_UINT32);

                /* Only draw once the necessary things have been bound (pipeline, descriptor sets, vertex buffers, index buffers) */
                vkCmdDrawIndexed(vkCmdBuffer, sizeof(geometryIndexData) / sizeof(geometryIndexData[0]), 1, 0, 0, 0);
            }
            vkCmdEndRenderPass(vkCmdBuffer);
        }
        result = vkEndCommandBuffer(vkCmdBuffer);
        if (result != VK_SUCCESS) {
            printf("Command buffer recording failed.");
            return -2;
        }


        VkPipelineStageFlags pWaitDstStageMask[1];
        pWaitDstStageMask[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo;
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                = NULL;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &semaphore;
        submitInfo.pWaitDstStageMask    = pWaitDstStageMask;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &vkCmdBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &semaphore;
        result = vkQueueSubmit(vkQueue, 1, &submitInfo, 0);
        if (result != VK_SUCCESS) {
            printf("Failed to submit buffer.");
            return -2;
        }

        VkPresentInfoKHR info;
        info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.pNext              = NULL;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores    = &semaphore;
        info.swapchainCount     = 1;
        info.pSwapchains        = &vkSwapchain;
        info.pImageIndices      = &imageIndex;
        info.pResults           = NULL;
        vkQueuePresentKHR(vkQueue, &info);
    }

    /* Unreachable. */
    return 0;
}