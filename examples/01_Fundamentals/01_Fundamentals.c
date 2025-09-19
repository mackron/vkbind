/*
Demonstrates the fundamentals of the Vulkan API, including:

  * Layers
  * Extensions
  * Physical and logical devices
  * Queue families
  * Swapchains
  * Pipelines
  * Vertex and index buffers
  * Textures
  * Uniform buffers
  * Descriptor sets and how they connect to shaders
  * Command buffers

This example is completely flat. The only library it uses is vkbind which is just a Vulkan API loader. The vkbind.h
file contains all of the Vulkan API declarations you need and is therefore a replacement for the official Vulkan
headers. Unlike the official Vulkan headers, vkbind includes all of the platform specific headers as well, as opposed
to keeping them all separate which is just unnecessary.

In a real-world program you would not want to write Vulkan code as it's written in this example. This example is void
of abstractions and functions in order to make it easier to see what's actually going on with Vulkan. The idea is to
show how to use Vulkan, not how to architecture your program. Also, resource cleanup is intentionally left out just to
keep things clean. A real program would probably want to clean everything up properly.

Currently only Windows is supported. This example will show you how to connect Vulkan to the Win32 windowing system
which is something almost all programs will want to do, so including that here is something I think is valuable. I will
look into adding X11 support at some point later on.

This example is focused on how to use the Vulkan API, not how to achieve specific graphics effects. If you're looking
for an example for lighting, PBR, etc. you'll need to look elsewhere.

Note that the program will close if you attempt to resize the window. This is due to the swapchain becoming invalid
since it's dimensions must always match that of the surface (the window, in this example). A normal program would want
to detect this and re-create the swapchain. The proper way to do this would be to wrap swapchain creation in a
function, but since we're keeping this example free of functions I'm just leaving it out. Once you understand how to
create a swapchain, which is demonstrated in this example, it should be easy enough for you to figure out how to handle
window resizes and re-create it.
*/

/*
The first thing to do is define which platforms you want to use. In our case we're only supporting Win32 for now. This
is done by #define-ing one of the `VK_USE_PLATFORM_*_KHR` symbols before the header. This is standard Vulkan and is 
something you would need to do even if you were using the official headers instead of vkbind. This will enable some
platform-specific functionality that you'll need for displaying something in a window.
*/
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
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
#include "01_Fundamentals_VFS.c"    /* <-- Compiled shader code is here. */

/*
This is the raw texture data that we'll be uploading to the Vulkan API when we create texture object. The texel
encoding we're using in this example is RGBA8 (or VK_FORMAT_R8G8B8A8_UNORM). We use this format for it's wide spread
support. This texture is a small 2x2 texture. We'll be using nearest-neighbor filtering (also known as point
filtering) when displaying the texture on the quad. Moving counter clockwise starting from the left, the texture
should be red, green, blue, black. The alpha channel is always set to opaque, or 0xFF.
*/
static const uint32_t g_TextureSizeX = 2;
static const uint32_t g_TextureSizeY = 2;
static const uint32_t g_TextureDataRGBA[4] = {
    0xFF0000FF, 0xFF000000, /* Encoding is 0xAABBGGRR. */
    0xFF00FF00, 0xFFFF0000
};

#include <stdio.h>

/* Platform-specific includes for X11 */
#ifndef _WIN32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>
#endif

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

/*
This callback is used with the VK_EXT_debug_report extension. This just prints any messages that come through.
*/
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
    (void)pUserData;

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
#else
    /* X11 window creation */
    Display* pDisplay = XOpenDisplay(NULL);
    if (pDisplay == NULL) {
        printf("Failed to open X11 display.\n");
        return -1;
    }

    int screen = DefaultScreen(pDisplay);
    
    /* Create a simple window */
    Window window = XCreateSimpleWindow(pDisplay, RootWindow(pDisplay, screen), 0, 0, 640, 480, 1, BlackPixel(pDisplay, screen), WhitePixel(pDisplay, screen));
    if (window == 0) {
        printf("Failed to create X11 window.\n");
        XCloseDisplay(pDisplay);
        return -1;
    }

    Atom wmDeleteMessage = XInternAtom(pDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(pDisplay, window, &wmDeleteMessage, 1);
    
    /* Set window title */
    XStoreName(pDisplay, window, "Vulkan Tutorial");

    /* Select input events */
    XSelectInput(pDisplay, window, ExposureMask | KeyPressMask | StructureNotifyMask);

    /* Make the window visible */
    XMapWindow(pDisplay, window);
    XFlush(pDisplay);
#endif

    /*
    This is where we start getting into actual Vulkan programming. The first concept to be aware of is that of the
    "instance". This should be fairly obvious - it's basically just the global object that everything is ultimately
    created from.

    To create an instance, there's two concepts to be aware of: layers and extensions. Vulkan has a layering feature
    whereby certain functionality can be plugged into (or layered on top of) the API. This example is enabling the
    standard validation layer which you've probably heard of already. If you're enabling a layer or extension, you need
    to check that it's actually supported by the instance or else you'll get an error when trying to create the
    instance.

    Note that if the VK_LAYER_KHRONOS_validation layer is not detected, you should try installing the official Vulkan
    SDK for your platform.
    */

    /* I'm using fixed sized arrays here for this example, but in a real program you may want to size these dynamically. */
    const char* ppEnabledLayerNames[32];
    uint32_t enabledLayerCount = 0;

    /* This is the list of layers that we'd like, but aren't strictly necessary. */
    const char* ppDesiredLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

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

    /* I'm using fixed sized arrays here for this example, but in a real program you may want to size these dynamically. */
    const char* ppEnabledExtensionNames[32];
    uint32_t enabledExtensionCount = 0;

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
#ifdef VK_USE_PLATFORM_XLIB_KHR
    ppEnabledExtensionNames[enabledExtensionCount++] = VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
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
    const char* ppDesiredExtensions[] = {
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME  /* This is optional and is used for consuming validation errors. */
    };

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
            debugReportCallbackCreateInfo.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
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
#else
    /* X11 surface creation */
    VkXlibSurfaceCreateInfoKHR surfaceInfo;
    surfaceInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.pNext  = NULL;
    surfaceInfo.flags  = 0;
    surfaceInfo.dpy    = pDisplay;
    surfaceInfo.window = window;
    result = vkCreateXlibSurfaceKHR(instance, &surfaceInfo, NULL, &surface);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(instance, NULL);
        XCloseDisplay(pDisplay);
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


            /*
            We want to use double buffering which means we'll need to retrieve the capabilities of the surface and check
            the maxImageCount property.
            */
            VkSurfaceCapabilitiesKHR surfaceCaps;
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pPhysicalDevices[iPhysicalDevice], surface, &surfaceCaps);
            if (result != VK_SUCCESS) {
                continue;
            }

            if (surfaceCaps.maxImageCount < 2) {
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
    We're going to need these memory properties for when we allocate memory. The way it works is there's basically
    different types of memory, the main ones being "host visible" and "device local". Host visible basically system RAM
    whereas device local is GPU memory. There's other flags to consider, but they aren't important while you're just
    getting started. These memory flags are grouped into a memory type, which are referenced by an index. When you
    allocate memory, you need to specify the index of an appropriate memory type which you select by iterating over the
    available memory types and checking if they have the appropriate flags. The memory types are retrieved by calling
    the vkGetPhysicalDeviceMemoryProperties() function.

    You'll see an example on how to iterate over these memory types later on. Since we're going to be doing this
    multiple times, I'm just going to retrieve the memory types once at the top right here rather than retrieving them
    multiple times. This process of iterating over memory types to find an appropriate index will probably be one of
    the first things you'll want to wrap in a helper function. We're not doing that in this example, however, because
    the point is to keep this completely flat.

    When you allocate memory, you first get the memory requirements (VkMemoryRequirements) of the relevant object which
    will be a buffer or an image. In the returned object, there will be a memoryTypesBits variable. The index of each
    set bit, starting from the least significant bit, defines which memory type in the VkPhysicalDeviceMemoryProperties
    object can be used for the memory allocation. From there, you inspect the memory type's property flags which will
    define whether or not the memory type is host visible and/or device local (among others). This will provide you
    with enough information to determine the memory type index. When we allocate memory later on for vertex buffers and
    textures you'll see more clearly how to choose an appropriate memory type.
    */
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProps;
    vkGetPhysicalDeviceMemoryProperties(pPhysicalDevices[selectedPhysicalDeviceIndex], &physicalDeviceMemoryProps);


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
    The next concept to introduce is that of the swapchain. The swapchain is closely related to the surface. Indeed,
    you need a surface before you can create a swapchain. A swapchain is made up of a number of images which, as the
    name suggests, are swapped with each other at display time.

    In a double buffered environment, there will be two images in the swapchain. At any given moment, one of those
    images will be displayed on the window, while the other one, which is off-screen, is being drawn to by the graphics
    driver. When the off-screen image is ready to be displayed, the two images are swapped and their roles reversed.

    To create a swapchain, you'll first need a surface which we've already created. You'll also need to determine how
    many images you need in the swap chain. If you're using double buffering you'll need two which is what we'll be
    using in this example. Triple buffering will require three images.

    Since the swapchain is made up of a number of images, we'll also need to specify the format and size of the images.
    If we try specifying an unsupported image format, creation of the swapchain will fail. We'll need to first
    enumerate over each of our supported formats. While you're just getting started, just use either VK_R8G8B8A8_UNORM
    or VK_B8G8R8A8_UNORM and move on. From there you can start experimenting with sRGB formats if that's important for
    you. For robustness you may want to check for available formats beforehand which is what this example does.
    */
    VkSurfaceFormatKHR supportedSwapchainImageFormats[256];
    uint32_t supportedFormatsCount = sizeof(supportedSwapchainImageFormats) / sizeof(supportedSwapchainImageFormats[0]);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(pPhysicalDevices[selectedPhysicalDeviceIndex], surface, &supportedFormatsCount, supportedSwapchainImageFormats);
    if (result != VK_SUCCESS) {
        printf("Failed to retrieve physical device surface formats.");
        return -1;
    }

    uint32_t swapchainImageFormatIndex = (uint32_t)-1;
    for (uint32_t i = 0; i < supportedFormatsCount; ++i) {
        if (supportedSwapchainImageFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM || supportedSwapchainImageFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {   /* <-- Can also use VK_FORMAT_*_SRGB */
            swapchainImageFormatIndex = i;
            break;
        }
    }

    if (swapchainImageFormatIndex == (uint32_t)-1) {
        printf("Could not find suitable display format.");
        return -1;
    }


    /*
    A this point we'll have our format selected, but there's just a few more pieces of information we'll need to
    retrieve real quick. The swapchain CreateInfo structure will ask for the size of the images and a transform, which
    is 90 degree rotations and flips. These can be retrieved from the current state of the surface. It's not entirely
    necessary to do this, but it's an easy way to get suitable values.
    */
    VkSurfaceCapabilitiesKHR surfaceCaps;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pPhysicalDevices[selectedPhysicalDeviceIndex], surface, &surfaceCaps);
    if (result != VK_SUCCESS) {
        printf("Failed to retrieve surface capabilities.");
        return -1;
    }

    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext                 = NULL;
    swapchainInfo.flags                 = 0;
    swapchainInfo.surface               = surface;
    swapchainInfo.minImageCount         = 2;                                    /* Set this to 2 for double buffering. Triple buffering would be 3, etc. */
    swapchainInfo.imageFormat           = supportedSwapchainImageFormats[swapchainImageFormatIndex].format;     /* The format we selected earlier. */
    swapchainInfo.imageColorSpace       = supportedSwapchainImageFormats[swapchainImageFormatIndex].colorSpace; /* The color space we selected earlier. */
    swapchainInfo.imageExtent           = surfaceCaps.currentExtent;            /* The size of the images of the swapchain. Keep this the same size as the surface. */
    swapchainInfo.imageArrayLayers      = 1;                                    /* I'm not sure in what situation you would ever want to set this to anything other than 1. */
    swapchainInfo.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;                                    /* Only used when imageSharingMode is VK_SHARING_MODE_CONCURRENT. */
    swapchainInfo.pQueueFamilyIndices   = NULL;                                 /* Only used when imageSharingMode is VK_SHARING_MODE_CONCURRENT. */
    swapchainInfo.preTransform          = surfaceCaps.currentTransform;         /* Rotations (90 degree increments) and flips. Just use the current transform from the surface and move on. */
    swapchainInfo.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode           = VK_PRESENT_MODE_FIFO_KHR;             /* This is what controls vsync. FIFO must always be supported, so use it by default. */
    swapchainInfo.clipped               = VK_TRUE;                              /* Set this to true if you're only displaying to a window. */
    swapchainInfo.oldSwapchain          = VK_NULL_HANDLE;                       /* You would set this if you're creating a new swapchain to replace an old one, such as when resizing a window. */

    VkSwapchainKHR swapchain;
    result = vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain);
    if (result != VK_SUCCESS) {
        printf("Failed to create swapchain.");
        return -2;
    }


    /*
    At this point the swapchain has been created, but there's a little bit more to do. Later on we're going to be
    creating a framebuffer for each of the swapchain images. Framebuffers interact with swapchain images through an
    image view so we'll need to create those too.

    We'll first determine the number of images making up the swapchain. When you created the swapchain you specified
    the *minimum* number of images making up the swapchain. That doesn't mean the driver didn't give you more. You'll
    need to handle this if you want a robust program. In practice, my testing has yielded an identical image count as
    specified in minImageCount for 2 (double buffered) and 3 (triple buffered) which will meet the needs of the vast
    majority of cases. When minImageCount is set to 1, my NVIDIA driver always allocates 2 images. I have not been
    able to do single-buffered rendering in Vulkan.

    I'm allocating the image and image view objects statically on the stack for simplicity and to avoid cluttering the
    code with non-Vulkan code. Also, the validation layer likes us to explicitly call vkGetSwapchainImagesKHR() with
    pSwapchainImages set to NULL in order to get the image count before retrieving the actual images.
    */
    uint32_t swapchainImageCount;
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL);    /* Set the output buffer to NULL to just retrieve a count. */
    if (result != VK_SUCCESS) {
        printf("Failed to retrieve swapchain image count.");
        return -1;
    }

    VkImage swapchainImages[16];    /* <-- Overkill. Consider allocating this dynamically. */
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);
    if (result != VK_SUCCESS) {
        printf("Failed to retrieve swapchain image count.");
        return -1;
    }

    /* Once we have the swapchain images we can create the views. The views will be used with the framebuffers later. */
    VkImageView swapchainImageViews[16];
    for (uint32_t iSwapchainImage = 0; iSwapchainImage < swapchainImageCount; iSwapchainImage += 1) {
        VkImageViewCreateInfo imageViewInfo;
        imageViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.pNext                           = NULL;
        imageViewInfo.flags                           = 0;
        imageViewInfo.image                           = swapchainImages[iSwapchainImage];
        imageViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.format                          = supportedSwapchainImageFormats[swapchainImageFormatIndex].format;
        imageViewInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
        imageViewInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
        imageViewInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
        imageViewInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
        imageViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel   = 0;
        imageViewInfo.subresourceRange.levelCount     = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount     = 1;
        result = vkCreateImageView(device, &imageViewInfo, NULL, &swapchainImageViews[iSwapchainImage]);
        if (result != VK_SUCCESS) {
            printf("Failed to create image views for swapchain images.");
            return -2;
        }
    }

    /*
    There's one last bit of prep work we're needing to do for the swapchain. Swapping images within the swapchain need
    to be synchronized which we achieve by using a semaphore which is passed in to vkAcquireNextImageKHR() and then
    waited for in the call to vkQueuePresentKHR() by adding it to the pWaitSemaphore member. This'll be explained later
    in the main loop, but we'll create the semapahore now in preparation.

    Semaphores are deleted with vkDestroySemaphore().
    */
    VkSemaphoreCreateInfo semaphoreInfo;
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = 0;
    semaphoreInfo.flags = 0;

    VkSemaphore swapchainSemaphore;
    vkCreateSemaphore(device, &semaphoreInfo, NULL, &swapchainSemaphore);


    
    /*
    Pipelines. To put it simply, a pipeline object defines the settings to use when drawing something, such as the
    structure of the vertex buffer, which shaders to use, shader inputs, whether or not depth testing is enabled, etc.
    Most programs will have many pipeline objects - you'll likely want to wrap this in a function for anything real-
    world.

    The creation of a pipeline object has a lot of dependencies. I'm not going to explain it all here, but will explain
    it as we go. The order in which I do things below is unimportant and you can do it in whatever order you'd like,
    but the way I do it here is intuitive to me.
    */

    /*
    Shaders. The first thing we'll define are the necessary shaders. We're just displaying a simple textured quad so
    all well need is a vertex and fragment shader. There are additional types of shaders for different stages of the
    pipeline, but that's an excersise for later. All real-world graphics projects will need a vertex and fragment
    shader, however.

    Shaders are specified in a binary format called SPIR-V. In this example we compile shaders from GLSL to SPIR-V
    using an offline tool called glslangValidator. The vfs_map_file() function is just some auto-generated code used by
    this example so we can embed the SPIR-V data directly into the program and avoid the need to load files and worry
    about startup paths and all that.
    */
    VkShaderModuleCreateInfo vertexShaderInfo;
    vertexShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexShaderInfo.pNext = NULL;
    vertexShaderInfo.flags = 0;
    vertexShaderInfo.pCode = (const uint32_t*)vfs_map_file("shaders/01_Fundamentals.glsl.vert.spirv", &vertexShaderInfo.codeSize);

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
    fragmentShaderInfo.pCode = (const uint32_t*)vfs_map_file("shaders/01_Fundamentals.glsl.frag.spirv", &fragmentShaderInfo.codeSize);

    VkShaderModule fragmentShaderModule;
    result = vkCreateShaderModule(device, &fragmentShaderInfo, NULL, &fragmentShaderModule);
    if (result != VK_SUCCESS) {
        printf("Failed to create shader module.");
        return -2;
    }

    VkPipelineShaderStageCreateInfo pipelineStages[2];
    pipelineStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineStages[0].pNext  = NULL;
    pipelineStages[0].flags  = 0;
    pipelineStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineStages[0].module = vertexShaderModule;
    pipelineStages[0].pName  = "main";
    pipelineStages[0].pSpecializationInfo = NULL;

    pipelineStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineStages[1].pNext  = NULL;
    pipelineStages[1].flags  = 0;
    pipelineStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineStages[1].module = fragmentShaderModule;
    pipelineStages[1].pName  = "main";
    pipelineStages[1].pSpecializationInfo = NULL;
    

    /*
    Vertex formats. Here is where we define the format of the data as passed to the vertex shader.

    There's two concepts to consider - bindings and attributes. Think of a binding as a grouping of related vertex
    attributes inside a single, interleaved buffer. A common example is interleaved vertex data. With this setup, which
    we're using in this example, the position, color, texture coordinate and whatever else you want to group together
    are part of a single binding. The position will be one attribute, the color will be another, etc. If, however, you
    were wanting to keep your individual vertex attributes (position, color, etc.) in separate buffers, you would have
    one binding for each buffer. So to summarize, binding = vertex buffer; attributes = individual elements (position,
    color, etc.) within a vertex buffer.

    Attributes are associated with a location which is just a number. This is an important concept to understand
    because it's how Vulkan is able to map vertex attribute to inputs into the vertex shader. It's important that
    locations map properly with how they're specified in the shader. In the vertex shader, you'll have vertex input
    declarations that look like this:

        layout(location = 0) in vec3 VERT_Position;
        layout(location = 1) in vec3 VERT_Color;
        layout(location = 2) in vec2 VERT_TexCoord;

    The locations you see specified in the vertex shader are specified in the code segment below. The "location" member
    of VkVertexInputAttributeDescription needs to map with how you've defined it in the shader. You'll note that you
    don't need to specify the binding in the shader. Instead, the binding is specified when we bind the vertex buffer
    with vkCmdBindVertexBuffers() which we do before issuing a draw command.
    */

    /*
    This is where we define our bindings. Remember: a binding is a vertex buffer. When interleaving vertex attributes
    such as position and color, we just have a single buffer. If we were *not* interleaving, we'd have a separate
    buffer, and therefore separate bindings, for each attribute.
    */
    VkVertexInputBindingDescription vertexInputBindingDescriptions[1];  /* <-- Just one binding since we're using a single, interleaved buffer. */
    vertexInputBindingDescriptions[0].binding   = 0;
    vertexInputBindingDescriptions[0].stride    = sizeof(float)*3 + sizeof(float)*3 + sizeof(float)*2;
    vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    /*
    This is where we specify the individual vertex attributes (position, color, etc.), and it's here where the location
    will be specified. In addition to the location, a binding is also required. The binding is an index into the array
    we just declared above (vertexInputBindingDescriptions[]). Since we're only using a single binding, we set this to
    0 for all of our vertex attributes.

    The format of the vertex attribute is also required. In this example, everything uses floating point. The position
    is defined in 3 dimensions. So as an example, the position attribute will be set to VK_FORMAT_R32G32B32_SFLOAT
    which should be simple enough to understand, albeit a little unintuitive due to it looking more like a texture
    format rather than a vertex format. The "SFLOAT" part means signed floating point. The "R", "G" and "B" parts
    represent "X", "Y" and "Z" respectively. The "32" means 32-bit.

    The last thing you need to specify is the offset in bytes of the start of the data for that attribute within the
    buffer. In the example below, the position is the first element which means it'll have no offset. The color comes
    immediately after the position, so therefore the offset needs to be set to the size of a single position element
    which is 3 floating point numbers, or sizeof(float)*3. Texture coordinates come immediately after color, so that
    will need to be set to sizeof(float)*3 (position) plus sizeof(float)*3 (color).
    */
    VkVertexInputAttributeDescription vertexInputAttributeDescriptions[3];

    /*
    Position.
    
    Shader Declaration: layout(location = 0) in vec3 VERT_Position;
    */
    vertexInputAttributeDescriptions[0].location = 0;   /* layout(location = 0) in the vertex shader. */
    vertexInputAttributeDescriptions[0].binding  = 0;   /* Maps to a binding in vertexInputBindingDescriptions[]. Referenced in vkCmdBindVertexBuffers() */
    vertexInputAttributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset   = 0;

    /*
    Color.

    Shader Declaration: layout(location = 1) in vec3 VERT_Color;
    */
    vertexInputAttributeDescriptions[1].location = 1;   /* layout(location = 1) in the vertex shader.*/
    vertexInputAttributeDescriptions[1].binding  = 0;   /* Maps to a binding in vertexInputBindingDescriptions[]. Referenced in vkCmdBindVertexBuffers() */
    vertexInputAttributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset   = sizeof(float)*3;

    /*
    Texture Coordinates.

    Shader Declaration: layout(location = 2) in vec3 VERT_TexCoord;
    */
    vertexInputAttributeDescriptions[2].location = 2;   /* layout(location = 2) in the vertex shader. */
    vertexInputAttributeDescriptions[2].binding  = 0;   /* Maps to a binding in vertexInputBindingDescriptions[]. Referenced in vkCmdBindVertexBuffers() */
    vertexInputAttributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[2].offset   = sizeof(float)*3 + sizeof(float)*3;

    /*
    The VkPipelineVertexInputStateCreateInfo object is where we put the vertex format defined above all together which
    will eventually be passed to the pipeline CreateInfo object.
    */
    VkPipelineVertexInputStateCreateInfo vertexInputState;
    vertexInputState.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.pNext                           = NULL;
    vertexInputState.flags                           = 0;
    vertexInputState.vertexBindingDescriptionCount   = sizeof(vertexInputBindingDescriptions)/sizeof(vertexInputBindingDescriptions[0]);
    vertexInputState.pVertexBindingDescriptions      = vertexInputBindingDescriptions;
    vertexInputState.vertexAttributeDescriptionCount = sizeof(vertexInputAttributeDescriptions)/sizeof(vertexInputAttributeDescriptions[0]);
    vertexInputState.pVertexAttributeDescriptions    = vertexInputAttributeDescriptions;


    /*
    The input assembly state basically controls the topology of the vertex data (whether or not the rasterizer should
    treat the vertex data as triangles, lines, etc.). Where the section above defined the structure of the vertex
    buffer(s), this defines how the rasterizer should interpret the vertex buffer(s) when rasterizing.
    */
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    inputAssemblyState.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.pNext                  = NULL;
    inputAssemblyState.flags                  = 0;
    inputAssemblyState.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;


    /*
    Viewport state. This is the glViewport() and glScissor() of Vulkan. These can actually be set dynamically rather
    than statically. We're going to use this property in this example. By setting pViewports and pScissors to NULL,
    we're telling Vulkan that we'll set these dynamically with vkCmdSetViewport() and vkCmdSetScissor() respectively.
    */
    VkPipelineViewportStateCreateInfo viewportState;
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext         = NULL;
    viewportState.flags         = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = NULL; /* <-- Set to NULL because we are using dynamic viewports. Set with vkCmdSetViewport(). */
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = NULL; /* <-- Set to NULL because we are using dynamic scissor rectangles. Set with vkCmdSetScissor(). */


    /*
    Rasterization state. This is where you set your fill modes, backface culling, polygon winding, etc.
    */
    VkPipelineRasterizationStateCreateInfo rasterizationState;
    rasterizationState.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.pNext                   = NULL;
    rasterizationState.flags                   = 0;
    rasterizationState.depthClampEnable        = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode                = VK_CULL_MODE_BACK_BIT;
    rasterizationState.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;   /* <-- This is the polygon winding. */
    rasterizationState.depthBiasEnable         = VK_FALSE;
    rasterizationState.depthBiasConstantFactor = 0;
    rasterizationState.depthBiasClamp          = 1;
    rasterizationState.depthBiasSlopeFactor    = 0;
    rasterizationState.lineWidth               = 1;


    /*
    Multisample state. We're not doing multisample anti-aliasing in this example, so we're just setting this to basic
    values. Note that rasterizationSamples should be set to VK_SAMPLE_COUNT_1_BIT to disable MSAA.
    */
    VkPipelineMultisampleStateCreateInfo multisampleState;
    multisampleState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.pNext                 = NULL;
    multisampleState.flags                 = 0;
    multisampleState.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable   = VK_FALSE;
    multisampleState.minSampleShading      = 1;
    multisampleState.pSampleMask           = NULL;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable      = VK_FALSE;


    /*
    Depth/stencil state. 
    */
    VkPipelineDepthStencilStateCreateInfo depthStencilState;
    depthStencilState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.pNext                 = NULL;
    depthStencilState.flags                 = 0;
    depthStencilState.depthTestEnable       = VK_TRUE;  /* This example will be using depth testing. */
    depthStencilState.depthWriteEnable      = VK_TRUE;  /* We'll want to write to the depth buffer. */
    depthStencilState.depthCompareOp        = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable     = VK_FALSE; /* No stencil testing in this example. The "front" and "back" settings don't matter here. */
    depthStencilState.front.failOp          = VK_STENCIL_OP_KEEP;
    depthStencilState.front.passOp          = VK_STENCIL_OP_KEEP;
    depthStencilState.front.depthFailOp     = VK_STENCIL_OP_KEEP;
    depthStencilState.front.compareOp       = VK_COMPARE_OP_ALWAYS;
    depthStencilState.front.compareMask     = 0xFFFFFFFF;
    depthStencilState.front.writeMask       = 0xFFFFFFFF;
    depthStencilState.front.reference       = 0;
    depthStencilState.back                  = depthStencilState.front;  /* We're not doing stencil testing, so set to whatever you want (but make sure it's valid for correctness sake). */
    depthStencilState.minDepthBounds        = 0;
    depthStencilState.maxDepthBounds        = 1;


    /*
    Color blend state. We have one color blend attachment state for each color attachment in the subpass. This'll be
    explained below in when creating the render pass.
    */
    VkPipelineColorBlendAttachmentState colorBlendAttachmentStates[1];  /* <-- One for each color attachment used by the subpass. */
    colorBlendAttachmentStates[0].blendEnable         = VK_FALSE;
    colorBlendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentStates[0].alphaBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachmentStates[0].colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState;
    colorBlendState.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.pNext             = NULL;
    colorBlendState.flags             = 0;
    colorBlendState.logicOpEnable     = VK_FALSE;
    colorBlendState.logicOp           = VK_LOGIC_OP_CLEAR;
    colorBlendState.attachmentCount   = sizeof(colorBlendAttachmentStates)/sizeof(colorBlendAttachmentStates[0]);
    colorBlendState.pAttachments      = colorBlendAttachmentStates;
    colorBlendState.blendConstants[0] = 0;
    colorBlendState.blendConstants[1] = 0;
    colorBlendState.blendConstants[2] = 0;
    colorBlendState.blendConstants[3] = 0;


    /*
    Dynamic state. Vulkan allows for some state to be dynamically set instead of statically. We're using a dynamic
    viewport and scissor in this example, mainly just to demonstrate how to use it. This requires us to call
    vkCmdSetViewport() and vkCmdSetScissor() at some point when building the command queue.
    */
    VkDynamicState dynamicStates[2];
    dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamicState;
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pNext             = NULL;
    dynamicState.flags             = 0;
    dynamicState.dynamicStateCount = sizeof(dynamicStates)/sizeof(dynamicStates[0]);
    dynamicState.pDynamicStates    = dynamicStates;
    

    /*
    Pipeline layout. This defines descriptor set layouts and push constants. Descriptor set layouts basically define the
    uniform variables in your shaders. Example shader:

        layout(set = 0, binding = 0) uniform sampler   FRAG_Sampler;
        layout(set = 0, binding = 1) uniform texture2D FRAG_Texture;

    The declarations above declare a sampler and a 2D texture. The "set" element in the layout decoration represent the
    descriptor set layout defined below. Each set can be associated with multiple bindings. You bind data at the level
    of a descriptor set, so it makes sense to group shader resources by the frequency at which they're updated. For
    example, you may want to have one descriptor set for global data that is updated once per frame, and then another
    descriptor set for data that is updated per object.
    */

    /*
    This example is using only a single texture. Vulkan supports separate textures and samplers (multiple samplers to 1 texture, for example). Since this
    is the more complicated way of doing textures, that's what we're going to do. You can also use a combined image/sampler which might be more useful in
    many situations, but that is easy to figure out on your own (hint: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER).
    */
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings_Set0[2];
    descriptorSetLayoutBindings_Set0[0].binding            = 0;                                  /* The "binding" in "layout(set = 0, binding = 0)" */
    descriptorSetLayoutBindings_Set0[0].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;         /* The "sampler" in "uniform sampler FRAG_Sampler" */
    descriptorSetLayoutBindings_Set0[0].descriptorCount    = 1;                                  /* If you had an array of samplers defined in the shader, you would set this to the size of the array. */
    descriptorSetLayoutBindings_Set0[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;       /* We're only going to be accessing the sampler from the fragment shader. */
    descriptorSetLayoutBindings_Set0[0].pImmutableSamplers = NULL;                               /* We'll use dynamic samplers for this example, but immutable samplers could be really useful in which case you'd set them here. */

    descriptorSetLayoutBindings_Set0[1].binding            = 1;                                  /* The "binding" in "layout(set = 0, binding = 1)" */
    descriptorSetLayoutBindings_Set0[1].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;   /* The "texture2D" in "uniform texture2D FRAG_Texture". Same thing for texture1D, texture3D and textureCube. */
    descriptorSetLayoutBindings_Set0[1].descriptorCount    = 1;                                  /* If you had an array of textures defined in the shader, you would set this to the size of the array. */
    descriptorSetLayoutBindings_Set0[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;       /* We're only going to be accessing the image from the fragment shader. */
    descriptorSetLayoutBindings_Set0[1].pImmutableSamplers = NULL;                               /* Always NULL for images. */

    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings_Set1[1];
    descriptorSetLayoutBindings_Set1[0].binding            = 0;                                  /* The "binding" in "layout(set = 1, binding = 0)" */
    descriptorSetLayoutBindings_Set1[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;  /* Self explanatory - we're updating a uniform buffer. */
    descriptorSetLayoutBindings_Set1[0].descriptorCount    = 1;                                  /* Not an array, so set to 1. */
    descriptorSetLayoutBindings_Set1[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;         /* Descriptor set 1 is only accessed via the vertex buffer. */
    descriptorSetLayoutBindings_Set1[0].pImmutableSamplers = NULL;                               /* No textures are being used in descriptor set 1. */


    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
    descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext        = NULL;
    descriptorSetLayoutCreateInfo.flags        = 0;
    descriptorSetLayoutCreateInfo.bindingCount = sizeof(descriptorSetLayoutBindings_Set0) / sizeof(descriptorSetLayoutBindings_Set0[0]);
    descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBindings_Set0;

    VkDescriptorSetLayout descriptorSetLayouts[2];
    result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayouts[0]);   /* layout(set = 0 ...) */
    if (result != VK_SUCCESS) {
        printf("Failed to create descriptor set layout 0.");
        return -1;
    }

    descriptorSetLayoutCreateInfo.bindingCount = sizeof(descriptorSetLayoutBindings_Set1) / sizeof(descriptorSetLayoutBindings_Set1[0]);
    descriptorSetLayoutCreateInfo.pBindings    = descriptorSetLayoutBindings_Set1;

    result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, NULL, &descriptorSetLayouts[1]);   /* layout(set = 1 ...) */
    if (result != VK_SUCCESS) {
        printf("Failed to create descriptor set layout 1.");
        return -1;
    }


    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext                  = NULL;
    pipelineLayoutInfo.flags                  = 0;
    pipelineLayoutInfo.setLayoutCount         = sizeof(descriptorSetLayouts) / sizeof(descriptorSetLayouts[0]);
    pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = NULL;

    VkPipelineLayout pipelineLayout;
    result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout);
    if (result != VK_SUCCESS) {
        printf("Failed to create pipeline layout.");
        return 2;
    }


    /*
    Render pass.
    */
    VkAttachmentDescription attachmentDesc[2];  /* 1 color attachment, 1 depth/stencil attachment. */

    /* Color attachment. */
    attachmentDesc[0].flags          = 0;
    attachmentDesc[0].format         = supportedSwapchainImageFormats[swapchainImageFormatIndex].format;   /* <-- This needs to be the same as our swapchain image format. */
    attachmentDesc[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachmentDesc[0].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc[0].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDesc[0].finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    /* Depth/stencil attachment. */
    attachmentDesc[1].flags          = 0;
    attachmentDesc[1].format         = VK_FORMAT_D24_UNORM_S8_UINT;
    attachmentDesc[1].samples        = VK_SAMPLE_COUNT_1_BIT;
    attachmentDesc[1].loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc[1].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDesc[1].initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDesc[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachment;
    colorAttachment.attachment = 0;         /* Index into attachmentDesc[] */
    colorAttachment.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthStencilAttachment;
    depthStencilAttachment.attachment = 1;  /* Index into attachmentDescp[] */
    depthStencilAttachment.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc;
    subpassDesc.flags = 0;
    subpassDesc.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.inputAttachmentCount    = 0;
    subpassDesc.pInputAttachments       = NULL;
    subpassDesc.colorAttachmentCount    = 1;
    subpassDesc.pColorAttachments       = &colorAttachment;
    subpassDesc.pResolveAttachments     = NULL;
    subpassDesc.pDepthStencilAttachment = &depthStencilAttachment;
    subpassDesc.preserveAttachmentCount = 0;    /* Only attachments that are *not* used by this subpass can be specified here. */
    subpassDesc.pPreserveAttachments    = NULL;

    VkRenderPassCreateInfo renderpassInfo;
    renderpassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderpassInfo.pNext           = NULL;
    renderpassInfo.flags           = 0;
    renderpassInfo.attachmentCount = sizeof(attachmentDesc) / sizeof(attachmentDesc[0]);
    renderpassInfo.pAttachments    = attachmentDesc;
    renderpassInfo.subpassCount    = 1;
    renderpassInfo.pSubpasses      = &subpassDesc;
    renderpassInfo.dependencyCount = 0;
    renderpassInfo.pDependencies   = NULL;

    VkRenderPass renderPass;
    result = vkCreateRenderPass(device, &renderpassInfo, NULL, &renderPass);
    if (result != VK_SUCCESS) {
        printf("Failed to create render pass.");
        return -2;
    }


    /*
    At this point we finally have everything we need to create the pipeline object. Creating a pipeline object is
    slightly different to the creation of most other Vulkan objects. For pipelines, you can actually create multiple
    pipeline objects with a single call. In this example we're only using a single pipeline object, but most real-world
    projects will require multiple.
    */
    VkGraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext               = NULL;
    pipelineInfo.flags               = 0;
    pipelineInfo.stageCount          = sizeof(pipelineStages) / sizeof(pipelineStages[0]);
    pipelineInfo.pStages             = pipelineStages;
    pipelineInfo.pVertexInputState   = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pTessellationState  = NULL; /* <-- Not using tessellation shaders here, so set to NULL. */
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pMultisampleState   = &multisampleState;
    pipelineInfo.pDepthStencilState  = &depthStencilState;
    pipelineInfo.pColorBlendState    = &colorBlendState;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = pipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = 0;
    pipelineInfo.basePipelineIndex   = 0;

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(device, 0, 1, &pipelineInfo, NULL, &pipeline);
    if (result != VK_SUCCESS) {
        printf("Failed to create graphics pipeline.");
        return -2;
    }




    /*
    In order for Vulkan to know which images to draw to and which images to use for the depth and stencil buffers, we
    use a VkFramebuffer object. We need to create one for each swapchain image. In our particular example, we can get
    away with just a single depth/stencil buffer. If you were using triple buferring you'd need two (one for each of
    the in-progress back buffers).

    Since we don't have a depth/stencil buffer yet, we'll need to create one. Fortunately this is a bit simpler than
    the creation of a texture which you'll see below because we don't need to worry about copying data.
    */

    /*
    Like swapchain images, the depth/stencil buffer is made up of both a VkImage and a VkImageView. We'll need to
    create the VkImage first, followed by the VkImageView.
    */
    VkImageCreateInfo depthStencilImageCreateInfo;
    depthStencilImageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthStencilImageCreateInfo.pNext                 = NULL;
    depthStencilImageCreateInfo.flags                 = 0;
    depthStencilImageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
    depthStencilImageCreateInfo.format                = VK_FORMAT_D24_UNORM_S8_UINT;    /* <-- Typical depth/stencil format. Should be supported most everywhere. */
    depthStencilImageCreateInfo.extent.width          = surfaceCaps.currentExtent.width;
    depthStencilImageCreateInfo.extent.height         = surfaceCaps.currentExtent.height;
    depthStencilImageCreateInfo.extent.depth          = 1;
    depthStencilImageCreateInfo.mipLevels             = 1;
    depthStencilImageCreateInfo.arrayLayers           = 1;
    depthStencilImageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
    depthStencilImageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
    depthStencilImageCreateInfo.usage                 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthStencilImageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    depthStencilImageCreateInfo.queueFamilyIndexCount = 0;
    depthStencilImageCreateInfo.pQueueFamilyIndices   = NULL;
    depthStencilImageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;  /* <-- Will be transitioned to VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL by the render pass. */

    VkImage depthStencilImage;
    result = vkCreateImage(device, &depthStencilImageCreateInfo, NULL, &depthStencilImage);
    if (result != VK_SUCCESS) {
        printf("Failed to create depth/stencil image.");
        return -1;
    }


    /*
    The image object for the depth/stencil image has been created, but it doesn't yet have any memory allocated for it.
    This will be explained in more detail when we get to creating a texture.
    */
    VkMemoryRequirements depthStencilImageMemoryReqs;
    vkGetImageMemoryRequirements(device, depthStencilImage, &depthStencilImageMemoryReqs);

    VkMemoryAllocateInfo depthStencilImageMemoryAllocateInfo;
    depthStencilImageMemoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depthStencilImageMemoryAllocateInfo.pNext           = NULL;
    depthStencilImageMemoryAllocateInfo.allocationSize  = depthStencilImageMemoryReqs.size;
    depthStencilImageMemoryAllocateInfo.memoryTypeIndex = (uint32_t)-1; /* <-- Set to a proper value in the loop below. */

    for (uint32_t iMemoryType = 0; iMemoryType < physicalDeviceMemoryProps.memoryTypeCount; iMemoryType += 1) { /* Check each supported memory type. */
        if ((depthStencilImageMemoryReqs.memoryTypeBits & (1 << iMemoryType)) != 0) {  /* Check if the image memory can be allocated by this memory type. */
            if ((physicalDeviceMemoryProps.memoryTypes[iMemoryType].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {  /* Check that this memory type supports GPU-side memory (device local). */
                depthStencilImageMemoryAllocateInfo.memoryTypeIndex = iMemoryType;
                break;
            }
        }
    }

    VkDeviceMemory depthStencilImageMemory;
    result = vkAllocateMemory(device, &depthStencilImageMemoryAllocateInfo, NULL, &depthStencilImageMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate memory for depth/stencil image.");
        return -1;
    }

    result = vkBindImageMemory(device, depthStencilImage, depthStencilImageMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind memory for depth/stencil image.");
        return -1;
    }


    /*
    The image view can only be created after allocating and binding memory for the depth/stencil image. Not doing this
    will result in a crash.
    */
    VkImageViewCreateInfo depthStencilImageViewCreateInfo;
    depthStencilImageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthStencilImageViewCreateInfo.pNext                           = NULL;
    depthStencilImageViewCreateInfo.flags                           = 0;
    depthStencilImageViewCreateInfo.image                           = depthStencilImage;
    depthStencilImageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilImageViewCreateInfo.format                          = VK_FORMAT_D24_UNORM_S8_UINT;
    depthStencilImageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_R;
    depthStencilImageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_G;
    depthStencilImageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_B;
    depthStencilImageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_A;
    depthStencilImageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    depthStencilImageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    depthStencilImageViewCreateInfo.subresourceRange.levelCount     = 1;
    depthStencilImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthStencilImageViewCreateInfo.subresourceRange.layerCount     = 1;

    VkImageView depthStencilImageView;
    result = vkCreateImageView(device, &depthStencilImageViewCreateInfo, NULL, &depthStencilImageView);
    if (result != VK_SUCCESS) {
        printf("Failed to create depth/stencil image view.");
        return -1;
    }



    /*
    We need one framebuffer for each swapchain image, but we can use the same depth/stencil buffer.
    */
    VkFramebuffer swapchainFramebuffers[16];
    for (uint32_t iSwapchainImage = 0; iSwapchainImage < swapchainImageCount; ++iSwapchainImage) {
        VkImageView framebufferAttachments[2];
        framebufferAttachments[0] = swapchainImageViews[iSwapchainImage];
        framebufferAttachments[1] = depthStencilImageView;

        VkFramebufferCreateInfo framebufferInfo;
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.pNext           = NULL;
        framebufferInfo.flags           = 0;
        framebufferInfo.renderPass      = renderPass;
        framebufferInfo.attachmentCount = sizeof(framebufferAttachments)/sizeof(framebufferAttachments[0]);
        framebufferInfo.pAttachments    = framebufferAttachments;
        framebufferInfo.width           = surfaceCaps.currentExtent.width;
        framebufferInfo.height          = surfaceCaps.currentExtent.height;
        framebufferInfo.layers          = 1;    /* <-- I'm not sure in what situation you would set to anything other than 1. */
        result = vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[iSwapchainImage]);
        if (result != VK_SUCCESS) {
            printf("Failed to create framebuffer.");
            return -2;
        }
    }


    /*
    Here is where we introduce command buffers. We need this before we create our vertex and index buffer and texture
    because we need to execute a command for copying memory from system memory to GPU memory (explained later).

    This example is only simple so we can get away with using only a single command buffer. Command buffers are created
    from a pool called VkCommandPool. When you draw stuff, you first record a list of commands into a command buffer.
    These commands are not executed immediately - they're executed later when you submit the command buffer to a queue.
    This design allows you to pre-record command buffers at an earlier stage (perhaps at load time) and reuse them
    without needing to incur the cost of re-issuing the drawing commands. This system, however, isn't always practical
    and sometimes you will want the command buffer to be reset so you can reuse them. This option needs to be specified
    at the level of the command pool, which is what we do in this example.

    You've probably seen or heard that one of the main points of difference between Vulkan and OpenGL is Vulkan's well
    specified multithreading support. Command buffers are where this really starts to become apparent. With Vulkan, you
    can record multiple command buffers across different threads. When allocating command buffers across different
    threads you would have separate command pools available on a per-thread basis.
    */
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext            = NULL;
    commandPoolCreateInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;   /* <-- Allows us to reuse the same command buffer (though not strictly necessary for this example). */
    commandPoolCreateInfo.queueFamilyIndex = selectedQueueFamilyIndex;                          /* <-- The dreaded queue family needs to be specified for command pools... */

    VkCommandPool commandPool;
    result = vkCreateCommandPool(device, &commandPoolCreateInfo, NULL, &commandPool);
    if (result != VK_SUCCESS) {
        printf("Failed to create command pool.");
        return -2;
    }

    /*
    Once we have the command pool we can create our command buffers. In our example we only need a single command
    which we'll be recording dynamically each frame. You don't need to do it like that - you can instead pre-record
    your command buffers and reuse them without being reset, but those command buffers will need to be allocated from
    a separate command pool that was created without the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag.

    Command buffers can be primary or secondary. Basically, primary command buffers are submitted directly to the queue
    whereas secondary command buffers are submitted to a primary command buffer. We're not using secondary command
    buffers here, but if you were wanting to use them, you'd use vkCmdExecuteCommands() to record them to a primary
    command buffer. There's other details with secondary command buffers which make them particularly useful in
    certain situations, such as being able to be used simultaneously by multiple primary command buffers (provided the
    necessary flag has been set on the secondary command buffer - VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT). I'll
    leave this an excercise for you.
    */
    VkCommandBufferAllocateInfo commandBufferCreateInfo;
    commandBufferCreateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferCreateInfo.pNext              = NULL;
    commandBufferCreateInfo.commandPool        = commandPool;
    commandBufferCreateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferCreateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    result = vkAllocateCommandBuffers(device, &commandBufferCreateInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate command buffer.");
        return -2;
    }

    /*
    Since we are using a reusable command buffer we need to use a fence to synchronize access to it. We cannot be
    trying to record some fresh commands into the buffer while it is still in a pending state from the previous frame.
    The fence that we create below will be passed into the vkQueueSubmit() which will signal the fence once the buffer
    has finished processing.
    */
    VkFenceCreateInfo commandBufferFenceCreateInfo;
    commandBufferFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    commandBufferFenceCreateInfo.pNext = NULL;
    commandBufferFenceCreateInfo.flags = 0;

    VkFence commandBufferFence;
    result = vkCreateFence(device, &commandBufferFenceCreateInfo, NULL, &commandBufferFence);
    if (result != VK_SUCCESS) {
        printf("Failed to create command buffer fence.");
        return -1;
    }


    /*
    We'll need a queue before we'll be able to execute any commands. When we created the logical device (VkDevice)
    we specified how many queues to make available. Here is were we retrieve it. Note that conceptually these queues
    were created internally when we created the device. Here we just retrieve it by an index rather than creating it.
    */
    VkQueue queue;
    vkGetDeviceQueue(device, selectedQueueFamilyIndex, 0, &queue); /* 0 = queue index. Only using a single queue in this example so will never be anything other than zero. */



    /*
    Vertex and index buffers.

    In this example we'll be storing vertex and index buffers in GPU memory which is probably where you'll want to
    store this data in most situations. This involves the use of an intermediary buffer which is allocated to system
    memory. Then, a command is used to transfer memory from the intermediary buffer to a device local buffer (GPU
    memory). This transfer is why we were needing to allocate a command buffer first. In my testing, vertex and index
    buffers don't actually need to be stored in GPU memory, but since it'll be a common thing to do, I'll demonstrate
    it in this example.

    In this example we're going to use a single VkBuffer for both vertex and index data. The idea is that both can be
    used with only a single memory allocation.

    The format of your vertex buffer and index buffers must match the format specified when you created the pipeline.
    */

    /*
    This is our geometry data. This needs to match the format we specified when we created the pipeline. These are the
    attributes used for each vertex which are interleaved.

        - Position:      3xfloat32
        - Color:         3xfloat32
        - Texture Coord: 2xfloat32
    */
    float geometryVertexData[] = {
        -0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 0.0f,   /* Vertex 0 */
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   0.0f, 1.0f,   /* Vertex 1 */
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 1.0f,   /* Vertex 2 */
         0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f    /* Vertex 3 */
    };

    /*
    This is our index data. These are specified based on the topology that was specified when we created the pipeline,
    which in this example is a triangle list. That means each group of 3 indices specify a triangle. Another thing to
    consider is the winding, which was also specified when we created the pipeline, which in this example is counter
    clockwise.
    */
    uint32_t geometryIndexData[] = {
        0, 1, 2, 2, 3, 0
    };


    /*
    Before we can copy out vertex and index data over to the GPU we first need to copy it into a staging buffer. The
    staging buffer is used as the intermediary between system memory and GPU memory and is just a normal buffer, but
    with the appropriate usage mode and a host visible memory allocation.
    */
    VkBufferCreateInfo vertexIndexStagingBufferCreateInfo;
    vertexIndexStagingBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexIndexStagingBufferCreateInfo.pNext                 = NULL;
    vertexIndexStagingBufferCreateInfo.flags                 = 0;
    vertexIndexStagingBufferCreateInfo.size                  = sizeof(geometryVertexData) + sizeof(geometryIndexData);
    vertexIndexStagingBufferCreateInfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;  /* We're going to be using this buffer as the transfer source. */
    vertexIndexStagingBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    vertexIndexStagingBufferCreateInfo.queueFamilyIndexCount = 0;   /* <-- Ignored when sharingMode is not VK_SHARING_MODE_CONCURRENT. */
    vertexIndexStagingBufferCreateInfo.pQueueFamilyIndices   = NULL;

    VkBuffer vertexIndexStagingBuffer;
    result = vkCreateBuffer(device, &vertexIndexStagingBufferCreateInfo, NULL, &vertexIndexStagingBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create vertex/index staging buffer.");
        return -1;
    }

    VkMemoryRequirements vertexIndexStagingBufferMemoryReqs;
    vkGetBufferMemoryRequirements(device, vertexIndexStagingBuffer, &vertexIndexStagingBufferMemoryReqs);

    VkMemoryAllocateInfo vertexIndexStagingBufferMemoryInfo;
    vertexIndexStagingBufferMemoryInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertexIndexStagingBufferMemoryInfo.pNext           = NULL;
    vertexIndexStagingBufferMemoryInfo.allocationSize  = vertexIndexStagingBufferMemoryReqs.size;
    vertexIndexStagingBufferMemoryInfo.memoryTypeIndex = (uint32_t)-1;    /* <-- Set to a proper value below. */

    for (uint32_t iMemoryType = 0; iMemoryType < physicalDeviceMemoryProps.memoryTypeCount; ++iMemoryType) {
        if ((vertexIndexStagingBufferMemoryReqs.memoryTypeBits & (1 << iMemoryType)) != 0) {
            if ((physicalDeviceMemoryProps.memoryTypes[iMemoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {  /* <-- Staging buffer must be host visible. */
                vertexIndexStagingBufferMemoryInfo.memoryTypeIndex = iMemoryType;
                break;
            }
        }
    }

    VkDeviceMemory vertexIndexStagingBufferMemory;
    result = vkAllocateMemory(device, &vertexIndexStagingBufferMemoryInfo, NULL, &vertexIndexStagingBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate vertex/index staging buffer memory.");
        return -2;
    }

    result = vkBindBufferMemory(device, vertexIndexStagingBuffer, vertexIndexStagingBufferMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind vertex/index staging buffer memory.");
        return -2;
    }

    /* To get the data from our regular old C buffer into the VkBuffer we just need to map the VkBuffer, memcpy(), then unmap. */
    void* pMappedBufferData;
    result = vkMapMemory(device, vertexIndexStagingBufferMemory, 0, vertexIndexStagingBufferMemoryReqs.size, 0, &pMappedBufferData);
    if (result != VK_SUCCESS) {
        printf("Failed to map vertex/index staging buffer.");
        return -2;
    }

    memcpy(pMappedBufferData, geometryVertexData, sizeof(geometryVertexData));
    memcpy((void*)(((char*)pMappedBufferData) + sizeof(geometryVertexData)), geometryIndexData, sizeof(geometryIndexData));

    vkUnmapMemory(device, vertexIndexStagingBufferMemory);



    /* At this point the staging buffer has been set up, so now we need to create the actual GPU-side buffer. */
    VkBufferCreateInfo vertexIndexBufferCreateInfo;
    vertexIndexBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexIndexBufferCreateInfo.pNext                 = NULL;
    vertexIndexBufferCreateInfo.flags                 = 0;
    vertexIndexBufferCreateInfo.size                  = sizeof(geometryVertexData) + sizeof(geometryIndexData);
    vertexIndexBufferCreateInfo.usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vertexIndexBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    vertexIndexBufferCreateInfo.queueFamilyIndexCount = 0;   /* <-- Ignored when sharingMode is not VK_SHARING_MODE_CONCURRENT. */
    vertexIndexBufferCreateInfo.pQueueFamilyIndices   = NULL;

    VkBuffer vertexIndexBuffer;
    result = vkCreateBuffer(device, &vertexIndexBufferCreateInfo, NULL, &vertexIndexBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create buffer for geometry.");
        return -2;
    }

    VkMemoryRequirements vertexIndexBufferMemoryReqs;
    vkGetBufferMemoryRequirements(device, vertexIndexBuffer, &vertexIndexBufferMemoryReqs);

    VkMemoryAllocateInfo vertexIndexBufferMemoryInfo;
    vertexIndexBufferMemoryInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertexIndexBufferMemoryInfo.pNext           = NULL;
    vertexIndexBufferMemoryInfo.allocationSize  = vertexIndexBufferMemoryReqs.size;
    vertexIndexBufferMemoryInfo.memoryTypeIndex = (uint32_t)-1;    /* <-- Set to a proper value below. */

    for (uint32_t iMemoryType = 0; iMemoryType < physicalDeviceMemoryProps.memoryTypeCount; ++iMemoryType) {
        if ((vertexIndexBufferMemoryReqs.memoryTypeBits & (1 << iMemoryType)) != 0) {
            if ((physicalDeviceMemoryProps.memoryTypes[iMemoryType].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                vertexIndexBufferMemoryInfo.memoryTypeIndex = iMemoryType;
                break;
            }
        }
    }

    VkDeviceMemory vertexIndexBufferMemory;
    result = vkAllocateMemory(device, &vertexIndexBufferMemoryInfo, NULL, &vertexIndexBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate memory.");
        return -2;
    }

    result = vkBindBufferMemory(device, vertexIndexBuffer, vertexIndexBufferMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind buffer memory.");
        return -2;
    }
    

    /*
    We've created both the staging buffer and the actual buffer, so now we need to transfer. This will be our first use
    of the command buffer we created earlier. With the way we're doing things in this example we don't need to worry
    about pipeline barriers, however in a real program you'll need to consider this for things such as making sure the
    vertex stage of the pipeline does not start until the transfer has completed. Pipeline barriers will be shown when
    we get to textures.
    */
    {
        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = NULL;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = NULL;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        {
            VkBufferCopy region;
            region.srcOffset = 0;
            region.dstOffset = 0;
            region.size = vertexIndexStagingBufferCreateInfo.size;
            vkCmdCopyBuffer(commandBuffer, vertexIndexStagingBuffer, vertexIndexBuffer, 1, &region);
        }
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo;
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                = NULL;
        submitInfo.waitSemaphoreCount   = 0;
        submitInfo.pWaitSemaphores      = NULL;
        submitInfo.pWaitDstStageMask    = NULL;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores    = NULL;
        result = vkQueueSubmit(queue, 1, &submitInfo, 0);
        if (result != VK_SUCCESS) {
            printf("Failed to submit buffer.");
            return -2;
        }

        vkQueueWaitIdle(queue);
    }

    /* The staging buffer is no longer needed. */
    vkDestroyBuffer(device, vertexIndexStagingBuffer, NULL);
    vkFreeMemory(device, vertexIndexStagingBufferMemory, NULL);



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

    for (uint32_t i = 0; i < physicalDeviceMemoryProps.memoryTypeCount; ++i) {
        if ((imageMemoryRequirements.memoryTypeBits & (1U << i)) != 0) {
            if ((physicalDeviceMemoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {  /* DEVICE_LOCAL = GPU memory. */
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
    VkBufferCreateInfo imageStagingBufferCreateInfo;
    imageStagingBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    imageStagingBufferCreateInfo.pNext                 = NULL;
    imageStagingBufferCreateInfo.flags                 = 0;
    imageStagingBufferCreateInfo.size                  = imageAllocateInfo.allocationSize;
    imageStagingBufferCreateInfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;   /* Will be the source of a transfer. */
    imageStagingBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    imageStagingBufferCreateInfo.queueFamilyIndexCount = 0;
    imageStagingBufferCreateInfo.pQueueFamilyIndices   = NULL;

    VkBuffer imageStagingBuffer;
    result = vkCreateBuffer(device, &imageStagingBufferCreateInfo, NULL, &imageStagingBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create staging buffer.");
        return -1;
    }

    /* Allocate memory for the staging buffer. */
    vkGetBufferMemoryRequirements(device, imageStagingBuffer, &imageMemoryRequirements);

    VkMemoryAllocateInfo imageStagingBufferAllocateInfo;
    imageStagingBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageStagingBufferAllocateInfo.pNext = NULL;
    imageStagingBufferAllocateInfo.allocationSize = imageMemoryRequirements.size;
    imageStagingBufferAllocateInfo.memoryTypeIndex = (uint32_t)-1;   // <-- Set to a proper value below.

    for (uint32_t i = 0; i < physicalDeviceMemoryProps.memoryTypeCount; ++i) {
        if ((imageMemoryRequirements.memoryTypeBits & (1U << i)) != 0) {
            if ((physicalDeviceMemoryProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                imageStagingBufferAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
    }

    VkDeviceMemory imageStagingBufferMemory;
    result = vkAllocateMemory(device, &imageStagingBufferAllocateInfo, NULL, &imageStagingBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate memory.");
        return -2;
    }

    /* Memory is allocated for the staging buffer. Now copy our image data into it. */
    void* pImageStagedBufferData;
    result = vkMapMemory(device, imageStagingBufferMemory, 0, imageMemoryRequirements.size, 0, &pImageStagedBufferData);
    if (result != VK_SUCCESS) {
        printf("Failed to map staging buffer memory.");
        return -2;
    }

    memcpy(pImageStagedBufferData, g_TextureDataRGBA, sizeof(g_TextureDataRGBA));

    vkUnmapMemory(device, imageStagingBufferMemory);

    /* The member can now be bound to the buffer. We could have also done this before the map/unmap part. */
    vkBindBufferMemory(device, imageStagingBuffer, imageStagingBufferMemory, 0);

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
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
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
            vkCmdPipelineBarrier(commandBuffer,
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
            vkCmdCopyBufferToImage(commandBuffer, imageStagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

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
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,         /* <-- Wait for the transfer stage to complete. */
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,  /* <-- We access this image via the fragment shader, so don't start fragment shading until the transition is complete. This doesn't actually matter for this example, but good practice. */
                0,
                0, NULL,
                0, NULL,
                1, &imageMemoryBarrier
            );
        }
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo;
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext                = NULL;
        submitInfo.waitSemaphoreCount   = 0;
        submitInfo.pWaitSemaphores      = NULL;
        submitInfo.pWaitDstStageMask    = NULL;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores    = NULL;
        result = vkQueueSubmit(queue, 1, &submitInfo, 0);
        if (result != VK_SUCCESS) {
            printf("Failed to submit buffer.");
            return -2;
        }

        vkQueueWaitIdle(queue);
    }

    /* The image staging buffer is no longer needed and can be freed. */
    vkDestroyBuffer(device, imageStagingBuffer, NULL);
    vkFreeMemory(device, imageStagingBufferMemory, NULL);


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
    Our scene is going to require the use of some uniform variables. This is basically how we pass in application
    defined data into shaders. This is an important concept, so I'm including it in this example for completeness.

    For uniform buffers to work, you guessed it, we need to allocate another buffer. However, this time it's a bit
    simpler (finally!) because we don't need to worry about uploading this to the GPU. In fact, what we're going to
    do is permanently map a host visible buffer and make sure it's configured as coherent so that writes to that
    memory are immediately visible.

    Also, we're going to use dynamic uniform buffers. For each object in the scene, we're just going to use a single
    descriptor set, but use a different offset. This simplifies the management of descriptor sets, but does make
    alignment a little bit more complicated because we need to ensure uniform buffers are aligned based on the
    minUniformBufferOffsetAlignment member of the physical device limits. 
    */
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(pPhysicalDevices[selectedPhysicalDeviceIndex], &physicalDeviceProperties);

    VkDeviceSize uniformBufferSizePerObject = sizeof(float)*4;  /* The per-object uniform buffer size is only a 4 dimensional floating point vector. */
    VkDeviceSize uniformBufferSizePerObjectAligned = uniformBufferSizePerObject / physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;
    if (uniformBufferSizePerObject % physicalDeviceProperties.limits.minUniformBufferOffsetAlignment > 0) {
        uniformBufferSizePerObjectAligned += 1;
    }
    uniformBufferSizePerObjectAligned *= physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;

    VkBufferCreateInfo uniformBufferCreateInfo;
    uniformBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uniformBufferCreateInfo.pNext                 = NULL;
    uniformBufferCreateInfo.flags                 = 0;
    uniformBufferCreateInfo.size                  = uniformBufferSizePerObjectAligned * 2; /* Multiply by two since we are using the same buffer for two objects. */
    uniformBufferCreateInfo.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    uniformBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    uniformBufferCreateInfo.queueFamilyIndexCount = 0;
    uniformBufferCreateInfo.pQueueFamilyIndices   = NULL;

    VkBuffer uniformBuffer;
    result = vkCreateBuffer(device, &uniformBufferCreateInfo, NULL, &uniformBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create uniform buffer.");
        return -1;
    }

    /*
    The backing memory for our uniform buffer can be host visible and coherent. We'll keep this permanently mapped.
    */
    VkMemoryRequirements uniformBufferMemoryReqs;
    vkGetBufferMemoryRequirements(device, uniformBuffer, &uniformBufferMemoryReqs);

    VkMemoryAllocateInfo uniformBufferMemoryAllocateInfo;
    uniformBufferMemoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    uniformBufferMemoryAllocateInfo.pNext           = NULL;
    uniformBufferMemoryAllocateInfo.allocationSize  = uniformBufferMemoryReqs.size;
    uniformBufferMemoryAllocateInfo.memoryTypeIndex = (uint32_t)-1; /* Set to a proper value in the loop below. */

    for (uint32_t i = 0; i < physicalDeviceMemoryProps.memoryTypeCount; ++i) {
        if ((uniformBufferMemoryReqs.memoryTypeBits & (1U << i)) != 0) {
            if ((physicalDeviceMemoryProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                uniformBufferMemoryAllocateInfo.memoryTypeIndex = i;
                break;
            }
        }
    }

    VkDeviceMemory uniformBufferMemory;
    result = vkAllocateMemory(device, &uniformBufferMemoryAllocateInfo, NULL, &uniformBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate uniform buffer memory.");
        return -1;
    }

    result = vkBindBufferMemory(device, uniformBuffer, uniformBufferMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind uniform buffer memory.");
        return -1;
    }

    /* Now we can create our permanent mapping. */
    void* pUniformBufferData;
    result = vkMapMemory(device, uniformBufferMemory, 0, uniformBufferMemoryReqs.size, 0, &pUniformBufferData);
    if (result != VK_SUCCESS) {
        printf("Failed to map uniform buffer memory.");
        return -1;
    }

    /* We will now declare some pointers for the per-object uniform data. This example has two objects in the scene. */
    float* pObjectUniformBuffer[2];
    for (size_t iObject = 0; iObject < sizeof(pObjectUniformBuffer) / sizeof(pObjectUniformBuffer[0]); iObject += 1) {
        pObjectUniformBuffer[iObject] = (float*)((char*)pUniformBufferData + (iObject * uniformBufferSizePerObjectAligned));
    }

    /* Set up some per-object data. */
    pObjectUniformBuffer[0][0] = -0.4f;
    pObjectUniformBuffer[0][1] = -0.4f;
    pObjectUniformBuffer[0][2] =  0.0f;
    pObjectUniformBuffer[0][3] =  0.0f;

    pObjectUniformBuffer[1][0] =  0.4f;
    pObjectUniformBuffer[1][1] =  0.4f;
    pObjectUniformBuffer[1][2] =  1.0f;  /* Put this one (bottom right) behind the other. */
    pObjectUniformBuffer[1][3] =  0.0f;



    /*
    At this point we have the image and the sampler, but we're still not done. We need to actually create the descriptor sets which will eventually be
    bound with vkCmdBindDescriptorSets() before we draw our geometry. When we created the pipeline, we specified the *layout* of the descriptor sets,
    but now we need to create the actual descriptor sets.

    To create the descriptor sets we first need a descritor pool. The descriptor pool is where the descriptor sets are allocated.
    */
    VkDescriptorPoolSize descriptorPoolSizes[3];
    descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorPoolSizes[0].descriptorCount = 1; /* This example only has 1 sampler. */
    descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorPoolSizes[1].descriptorCount = 1; /* This example only has 1 image. */
    descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorPoolSizes[2].descriptorCount = 1; /* This example only has 1 uniform buffer. */

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext         = NULL;
    descriptorPoolCreateInfo.flags         = 0; /* Could set this to VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT if we wanted to be able to free an individual descriptor set, but we're not doing that in this example. */
    descriptorPoolCreateInfo.maxSets       = 2; /* We're only going to be allocating two descriptor sets. */
    descriptorPoolCreateInfo.poolSizeCount = sizeof(descriptorPoolSizes) / sizeof(descriptorPoolSizes[0]);
    descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSizes;

    VkDescriptorPool descriptorPool;
    result = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool);
    if (result != VK_SUCCESS) {
        printf("Failed to create descriptor pool.");
        return -1;
    }

    /* We have the descriptor pool, so now we can create our descriptor set. */
    VkDescriptorSetLayout descriptorSetLayoutsToAllocate[2];    /* 1 descriptor set for textures, 1 descriptor set for dynamic uniform buffer. */
    descriptorSetLayoutsToAllocate[0] = descriptorSetLayouts[0];
    descriptorSetLayoutsToAllocate[1] = descriptorSetLayouts[1];

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext              = NULL;
    descriptorSetAllocateInfo.descriptorPool     = descriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = sizeof(descriptorSetLayoutsToAllocate) / sizeof(descriptorSetLayoutsToAllocate[0]);
    descriptorSetAllocateInfo.pSetLayouts        = descriptorSetLayoutsToAllocate;

    VkDescriptorSet descriptorSets[2];
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


    VkDescriptorBufferInfo descriptorBufferInfo;
    descriptorBufferInfo.buffer = uniformBuffer;
    descriptorBufferInfo.offset = 0;
    descriptorBufferInfo.range  = sizeof(float)*4;

    VkWriteDescriptorSet descriptorSetWrites_UBO[1];
    descriptorSetWrites_UBO[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorSetWrites_UBO[0].pNext            = NULL;
    descriptorSetWrites_UBO[0].dstSet           = descriptorSets[1];
    descriptorSetWrites_UBO[0].dstBinding       = 0;
    descriptorSetWrites_UBO[0].dstArrayElement  = 0;
    descriptorSetWrites_UBO[0].descriptorCount  = 1;
    descriptorSetWrites_UBO[0].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    descriptorSetWrites_UBO[0].pImageInfo       = NULL;
    descriptorSetWrites_UBO[0].pBufferInfo      = &descriptorBufferInfo;
    descriptorSetWrites_UBO[0].pTexelBufferView = NULL;

    vkUpdateDescriptorSets(device, sizeof(descriptorSetWrites_UBO) / sizeof(descriptorSetWrites_UBO[0]), descriptorSetWrites_UBO, 0, NULL);



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
    #else
        /* X11 event handling */
        XEvent event;
        if (XPending(pDisplay)) {
            XNextEvent(pDisplay, &event);
            if (event.type == ClientMessage) {
                if ((Atom)event.xclient.data.l[0] == wmDeleteMessage) {
                    break;  /* Received a quit message. */
                }
            } else if (event.type == DestroyNotify) {
                break;  /* Window was destroyed, probably by the window manager. */
            }
        }
    #endif
     
        uint32_t swapchainImageIndex;
        result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, swapchainSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            return -1;
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            /*
            Here is where you might want to recreate the swapchain. I'm not doing this in this here because I want to
            keep the code flat and this would require function-izing the swapchain initialization stuff which I don't
            want to do in this example.
            
            You may get this error when you try resizing the window. This happens on Nvidia, but not Intel. I have not
            tested AMD.
            */
        }

        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = NULL;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = NULL;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        {
            VkClearValue clearValues[2];

            /* Color attachment. */
            clearValues[0].color.float32[0] = 0.2f;
            clearValues[0].color.float32[1] = 0;
            clearValues[0].color.float32[2] = 0;
            clearValues[0].color.float32[3] = 1;

            /* Depth/stencil attachment. */
            clearValues[1].depthStencil.depth   = 1;    /* 0 = closer to viewer; 1 = further away. */
            clearValues[1].depthStencil.stencil = 0;

            VkRenderPassBeginInfo renderpassBeginInfo;
            renderpassBeginInfo.sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderpassBeginInfo.pNext               = NULL;
            renderpassBeginInfo.renderPass          = renderPass;
            renderpassBeginInfo.framebuffer         = swapchainFramebuffers[swapchainImageIndex];
            renderpassBeginInfo.renderArea.offset.x = 0;
            renderpassBeginInfo.renderArea.offset.y = 0;
            renderpassBeginInfo.renderArea.extent   = surfaceCaps.currentExtent;
            renderpassBeginInfo.clearValueCount     = sizeof(clearValues) / sizeof(clearValues[0]);
            renderpassBeginInfo.pClearValues        = clearValues;
            vkCmdBeginRenderPass(commandBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            {
                VkViewport viewport;
                viewport.x        = 0;
                viewport.y        = 0;
                viewport.width    = (float)surfaceCaps.currentExtent.width;
                viewport.height   = (float)surfaceCaps.currentExtent.height;
                viewport.minDepth = 0;  /* Careful here. Vulkan does not use the same coordinate system as OpenGL. Requires a clipping matrix to be applied to the projection matrix to get this in the [0..1] range. */
                viewport.maxDepth = 1;
                vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

                VkRect2D scissor;
                scissor.offset.x      = 0;
                scissor.offset.y      = 0;
                scissor.extent.width  = surfaceCaps.currentExtent.width;
                scissor.extent.height = surfaceCaps.currentExtent.height;
                vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

                
                /* Bind the pipeline first. You can bind descriptor sets, vertex buffers and index buffers before binding the pipeline, but I like to do the pipeline first. */
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                /* Vertex and index buffers need to be bound before drawing. */
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexIndexBuffer, &offset);
                vkCmdBindIndexBuffer(commandBuffer, vertexIndexBuffer, sizeof(geometryVertexData), VK_INDEX_TYPE_UINT32);

                /* We need to bind the descriptor sets before we'll be able to use the texture in the shader. */
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0], 0, NULL);


                /* Object 1. */
                uint32_t uniformOffset1 = 0;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &descriptorSets[1], 1, &uniformOffset1);

                /* Only draw once the necessary things have been bound (pipeline, descriptor sets, vertex buffers, index buffers) */
                vkCmdDrawIndexed(commandBuffer, sizeof(geometryIndexData) / sizeof(geometryIndexData[0]), 1, 0, 0, 0);


                /* Object 2. */
                uint32_t uniformOffset2 = (uint32_t)uniformBufferSizePerObjectAligned;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &descriptorSets[1], 1, &uniformOffset2);
                
                /* Only draw once the necessary things have been bound (pipeline, descriptor sets, vertex buffers, index buffers) */
                vkCmdDrawIndexed(commandBuffer, sizeof(geometryIndexData) / sizeof(geometryIndexData[0]), 1, 0, 0, 0);
            }
            vkCmdEndRenderPass(commandBuffer);
        }
        result = vkEndCommandBuffer(commandBuffer);
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
        submitInfo.pWaitSemaphores      = &swapchainSemaphore;
        submitInfo.pWaitDstStageMask    = pWaitDstStageMask;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &swapchainSemaphore;
        result = vkQueueSubmit(queue, 1, &submitInfo, commandBufferFence);
        if (result != VK_SUCCESS) {
            printf("Failed to submit buffer.");
            return -2;
        }

        /*
        In the call above to vkQueueSubmit() we specified a fence that will be signalled when the command buffer has
        completed processing. We need to wait on that now so that the next call to vkBeginCommandBuffer() does not
        complain about the command buffer being in a pending state. This is only required in our case because we're
        reusing the same command buffer.
        */
        vkWaitForFences(device, 1, &commandBufferFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &commandBufferFence);

        VkPresentInfoKHR info;
        info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.pNext              = NULL;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores    = &swapchainSemaphore;
        info.swapchainCount     = 1;
        info.pSwapchains        = &swapchain;
        info.pImageIndices      = &swapchainImageIndex;
        info.pResults           = NULL;
        vkQueuePresentKHR(queue, &info);
    }

    /*
    Teardown. This isn't a complete teardown - you would need to destroy anything that was created with vkCreate*()
    and free any memory that was created with vkAllocateMemory() with vkFreeMemory(). Unfortunately the flat nature of
    this example just makes this too annoying to deal with so I'm not doing a complete teardown for the sake of
    simplicity. Note that the validation layer will result in a bunch of errors being reported at this point due to
    devices being uninitialized while other objects are still active. To avoid this, uninitialize your objects in the
    correct order.
    */
    vkDestroySwapchainKHR(device, swapchain, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    return 0;
}
