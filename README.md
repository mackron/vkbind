vkbind
======
vkbind is a single file Vulkan API loader. It includes a full implementation of the Vulkan headers (auto-generated
from the Vulkan spec) so there's no need for the offical headers or SDK. Unlike the official headers, the
platform-specific sections are all contained within the same file.

Usage
=====
For platforms that statically expose all Vulkan APIs you can use vkbind like so:
```c
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
```
For those platforms that do not statically expose all Vulkan APIs, you should do something like this:
```c
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

    result = vkbBindAPI(&api);  // <-- Optional. Can call APIs directly like api.vkDestroyInstance().
    if (result != VK_SUCCESS) {
        printf("Failed to bind instance API.");
        return -3;
    }

    // Do stuff here.

    vkbUninit();
    return 0;
}
```
The code above uses the `VkbAPI` structure which contains function pointers to all Vulkan APIs. The idea is that you
first initialize with `vkbInit()`, then call `vkbInitInstanceAPI()` after you've created your Vulkan instance. The
`VkbAPI` object will be filled with usable function pointers at this point (so long as they're supported by the platform).
In this example, the instance APIs are bound to global scope using `vkbBindAPI()`, however if you have multiple
instances running at the same time, you should instead invoke the functions directly from the `VkbAPI` object.

You can also initialize the `VkbAPI` object from a Vulkan device. This is optional, and is intended as an optimization
to avoid the cost of internal dispatching. To use this, you first initialize your `VkbAPI` object with
`vkbInitializeInstanceAPI()`, then call `vkbInitializeDeviceAPI()`, passing in the same `VkbAPI` object.
```c
VkbAPI api;
vkbInitInstanceAPI(instance, &api);
vkbInitDeviceAPI(device, &api);
vkbBindAPI(&api);
```

Examples
========
You can find some general Vulkan examples in the "examples" folder. Unlike many other examples out there, these ones
are flat, all self contained in a single code file, and easy to read. Note that shaders have been pre-compiled by
glslangValidator (GLSL to SPIR-V compiler). You can see the relevant shaders in the "examples/shaders" folder.

License
=======
Public domain or MIT-0 (No Attribution). Choose whichever you prefer.