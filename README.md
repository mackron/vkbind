
<h4 align="center">A single file Vulkan header and API loader.</h4>

<p align="center">
    <a href="https://discord.gg/9vpqbjU"><img src="https://img.shields.io/discord/712952679415939085?label=discord&logo=discord" alt="discord"></a>
    <a href="https://fosstodon.org/@mackron"><img src="https://img.shields.io/mastodon/follow/109293691403797709?color=blue&domain=https%3A%2F%2Ffosstodon.org&label=mastodon&logo=mastodon&style=flat-square" alt="mastodon"></a>
</p>

vkbind is a Vulkan API loader and includes a full implementation of the Vulkan headers (auto-generated from the
Vulkan spec) so there's no need for the offical headers or SDK. Unlike the official headers, the platform-specific
sections are all contained within the same file.


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
You can find some general Vulkan examples in the "examples" folder. The first example, 01_Fundamentals, is completely
flat and self contained in a single code file. This is unlike most of the other popular example projects out there which
are full of abstractions which make things too hard to follow. It covers most (all?) of the fundamentals any real Vulkan
program is going to need, and tries to explain how things like vertex layouts and descriptor sets interact with shaders
which I think is something other examples seem to overlook.

For practicality, future examples will not be entirely flat, but should still be significantly better than other
popular examples out there in terms of readability. 

Note that shaders are pre-compiled with glslangValidator (GLSL to SPIR-V compiler) and embedded into a source file for
each example. This is just to avoid the need to worry about file IO for shaders and to focus on Vulkan itself.


License
=======
Public domain or MIT-0 (No Attribution). Choose whichever you prefer.