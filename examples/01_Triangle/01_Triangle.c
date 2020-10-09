#define VK_USE_PLATFORM_WIN32_KHR
#define VKBIND_IMPLEMENTATION
#include "../../vkbind.h"

#include "01_Triangle_VFS.c"    /* <-- Compiled shader code is here. */

#include <stdio.h>

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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    VkResult result;

    /* API */
    result = vkbInit(NULL);

    /* Window */
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

    /* Vulkan Instance */
    VkLayerProperties layers[32];
    uint32_t layerCount = sizeof(layers)/sizeof(layers[0]);
    result = vkEnumerateInstanceLayerProperties(&layerCount, layers);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        printf("Failed to retrieve layers.");
        return -1;
    }


    const char* ppEnabledLayerNames[] = {
         "VK_LAYER_LUNARG_standard_validation"
    };

    uint32_t enabledLayerCount = sizeof(ppEnabledLayerNames) / sizeof(ppEnabledLayerNames[0]);


    const char* ppEnabledExtensionNames[] =  {
        VK_KHR_SURFACE_EXTENSION_NAME
#ifdef VK_USE_PLATFORM_WIN32_KHR
      , VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#endif
    };

    uint32_t enabledExtensionCount = sizeof(ppEnabledExtensionNames) / sizeof(ppEnabledExtensionNames[0]);


    VkInstanceCreateInfo instanceInfo;
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = NULL;
    instanceInfo.flags = 0;
    instanceInfo.pApplicationInfo = NULL;
    instanceInfo.enabledLayerCount = enabledLayerCount;
    instanceInfo.ppEnabledLayerNames = ppEnabledLayerNames;
    instanceInfo.enabledExtensionCount = enabledExtensionCount;
    instanceInfo.ppEnabledExtensionNames = ppEnabledExtensionNames;
    VkInstance vkInstance;
    result = vkCreateInstance(&instanceInfo, NULL, &vkInstance);
    if (result != VK_SUCCESS) {
        printf("Failed to create Vulkan instance. Check that your hardware supports Vulkan and you have up to date drivers installed.");
        return -2;
    }


    /* Physical Device */
    VkPhysicalDevice vkPhysicalDevices[16];
    uint32_t vkPhysicalDeviceCount = sizeof(vkPhysicalDevices) / sizeof(vkPhysicalDevices[0]);
    result = vkEnumeratePhysicalDevices(vkInstance, &vkPhysicalDeviceCount, vkPhysicalDevices);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        printf("Failed to enumerate physical devices. Check that your hardware supports Vulkan and you have up to date drivers installed.");
        return -2;
    }

    /* Physical Device Info */
    for (uint32_t i = 0; i < vkPhysicalDeviceCount; ++i) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(vkPhysicalDevices[i], &properties);

        printf("Physical Device: %s\n", properties.deviceName);
        printf("    API Version: %d.%d\n", VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion));
    }
    
    uint32_t iPhysicalDevice = 0;   // <-- Change this to choose which physical device to use.


    /* Logical Device */

    // We need a queue supporting graphics. We just use the first one we can find.
    VkQueueFamilyProperties queueFamilyProperties[16];
    uint32_t queueFamilyCount = sizeof(queueFamilyProperties) / sizeof(queueFamilyProperties[0]);
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevices[iPhysicalDevice], &queueFamilyCount, queueFamilyProperties);

    uint32_t queueFamilyIndex_Graphics = (uint32_t)-1;
    for (uint32_t iQueueFamily = 0; iQueueFamily < queueFamilyCount; ++iQueueFamily) {
        if ((queueFamilyProperties[iQueueFamily].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            queueFamilyIndex_Graphics = iQueueFamily;
            break;
        }
    }

    if (queueFamilyIndex_Graphics == (uint32_t)-1) {
        printf("Default device does not support a graphics queue.");
        return -2;
    }

    float queuePriority = 1;

    VkDeviceQueueCreateInfo queueInfo;
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.pNext = NULL;
    queueInfo.flags = 0;
    queueInfo.queueFamilyIndex = queueFamilyIndex_Graphics;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(vkPhysicalDevices[iPhysicalDevice], &physicalDeviceFeatures);

    const char* ppEnabledDeviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t enabledDeviceExtensionCount = sizeof(ppEnabledDeviceExtensionNames) / sizeof(ppEnabledDeviceExtensionNames[0]);


    VkDeviceCreateInfo deviceInfo;
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = NULL;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledLayerCount = 0;
    deviceInfo.ppEnabledLayerNames = NULL;
    deviceInfo.enabledExtensionCount = enabledDeviceExtensionCount;
    deviceInfo.ppEnabledExtensionNames = ppEnabledDeviceExtensionNames;
    deviceInfo.pEnabledFeatures = &physicalDeviceFeatures;  // <-- Can this be NULL?

    VkDevice vkDevice;
    result = vkCreateDevice(vkPhysicalDevices[iPhysicalDevice], &deviceInfo, NULL, &vkDevice);
    if (result != VK_SUCCESS) {
        printf("Failed to create logical device.");
        return -2;
    }


    /* Surface and Swapchain */
    VkSurfaceKHR vkSurface;
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surfaceInfo;
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.pNext = NULL;
    surfaceInfo.flags = 0;
    surfaceInfo.hinstance = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
    surfaceInfo.hwnd = hWnd;
    VkResult vkResult = vkCreateWin32SurfaceKHR(vkInstance, &surfaceInfo, NULL, &vkSurface);
    if (vkResult != VK_SUCCESS) {
        printf("Failed to create a Vulkan surface for the main window.");
        return -2;
    }
#endif

    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevices[iPhysicalDevice], vkSurface, &surfaceCaps);
    if (vkResult != VK_SUCCESS) {
        printf("Failed to retrieve surface capabilities.");
        return -2;
    }

    if (surfaceCaps.minImageCount < 2) {
        printf("Surface must support at least 2 images for double buffering.");
        return -2;
    }

    VkBool32 isSurfaceSupported;
    vkResult = vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevices[iPhysicalDevice], queueFamilyIndex_Graphics, vkSurface, &isSurfaceSupported);
    if (vkResult != VK_SUCCESS || isSurfaceSupported == VK_FALSE) {
        printf("Surface is not supported on the physical device.");
        return -2;
    }


    VkSurfaceFormatKHR supportedFormats[256];
    uint32_t supportedFormatsCount = sizeof(supportedFormats) / sizeof(supportedFormats[0]);
    vkResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevices[iPhysicalDevice], vkSurface, &supportedFormatsCount, supportedFormats);
    if (vkResult != VK_SUCCESS) {
        printf("Failed to retrieve physical device surface formats.");
        return -2;
    }

    uint32_t iFormat = 0;
    for (uint32_t i = 0; i < supportedFormatsCount; ++i) {
        if (supportedFormats[i].format == VK_FORMAT_R8G8B8A8_UNORM || supportedFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {   // <-- Can also use VK_FORMAT_*_SRGB
            iFormat = i;
            break;
        }
    }


    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext = NULL;
    swapchainInfo.flags = 0;
    swapchainInfo.surface = vkSurface;
    swapchainInfo.minImageCount = 2;
    swapchainInfo.imageFormat = supportedFormats[iFormat].format;
    swapchainInfo.imageColorSpace = supportedFormats[iFormat].colorSpace;
    swapchainInfo.imageExtent = surfaceCaps.currentExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TRANSFER_DST is required for clearing.
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = NULL;
    swapchainInfo.preTransform = surfaceCaps.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;   // <-- FIFO must always be supported, so use it by default.
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = 0;

    VkSwapchainKHR vkSwapchain;
    vkResult = vkCreateSwapchainKHR(vkDevice, &swapchainInfo, NULL, &vkSwapchain);
    if (vkResult != VK_SUCCESS) {
        printf("Failed to create swapchain.");
        vkDestroySurfaceKHR(vkInstance, vkSurface, NULL);
        return -2;
    }



    /* Clearing the Swapchain and Presenting */

    // Grab each swapchain image.
    VkImage images[2];
    uint32_t imageCount = sizeof(images) / sizeof(images[0]);
    result = vkGetSwapchainImagesKHR(vkDevice, vkSwapchain, &imageCount, images);
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
        result = vkCreateImageView(vkDevice, &imageViewInfo, NULL, &imageViews[i]);
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
    vkCreateSemaphore(vkDevice, &semaphoreInfo, NULL, &semaphore);




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
    result = vkCreateRenderPass(vkDevice, &renderpassInfo, NULL, &renderpass);
    if (result != VK_SUCCESS) {
        printf("Failed to create render pass.");
        return -2;
    }



    // Pipeline.
    VkShaderModuleCreateInfo vertexShaderInfo;
    vertexShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexShaderInfo.pNext = NULL;
    vertexShaderInfo.flags = 0;
    vertexShaderInfo.pCode = (const uint32_t*)vfs_map_file("shaders/01_Triangle.glsl.vert.spirv", &vertexShaderInfo.codeSize);

    VkShaderModule vertexShaderModule;
    result = vkCreateShaderModule(vkDevice, &vertexShaderInfo, NULL, &vertexShaderModule);
    if (result != VK_SUCCESS) {
        printf("Failed to create shader module.");
        return -2;
    }

    VkShaderModuleCreateInfo fragmentShaderInfo;
    fragmentShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragmentShaderInfo.pNext = NULL;
    fragmentShaderInfo.flags = 0;
    fragmentShaderInfo.pCode = (const uint32_t*)vfs_map_file("shaders/01_Triangle.glsl.frag.spirv", &fragmentShaderInfo.codeSize);

    VkShaderModule fragmentShaderModule;
    result = vkCreateShaderModule(vkDevice, &fragmentShaderInfo, NULL, &fragmentShaderModule);
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
    vertexInputBindingDescriptions[0].stride    = sizeof(float)*3 + sizeof(float)*3;
    vertexInputBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexInputAttributeDescriptions[2];
    vertexInputAttributeDescriptions[0].location = 0;   // Position
    vertexInputAttributeDescriptions[0].binding  = 0;
    vertexInputAttributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset   = 0;
    vertexInputAttributeDescriptions[1].location = 1;   // Color
    vertexInputAttributeDescriptions[1].binding  = 0;
    vertexInputAttributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset   = sizeof(float)*3;

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
    depthStencilState.depthTestEnable = VK_FALSE;   // <-- TODO: Turn this on when we have something showing up on the screen.
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

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = NULL;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = NULL;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = NULL;

    VkPipelineLayout pipelineLayout;
    result = vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, NULL, &pipelineLayout);
    if (result != VK_SUCCESS) {
        printf("Failed to create pipeline layout.");
        return 2;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = NULL;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = 2;    // Verex + Fragment
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
    result = vkCreateGraphicsPipelines(vkDevice, 0, 1, &pipelineInfo, NULL, &vkPipeline);
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
        result = vkCreateFramebuffer(vkDevice, &framebufferInfo, NULL, &framebuffers[i]);
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
    cmdpoolInfo.queueFamilyIndex = queueFamilyIndex_Graphics;

    VkCommandPool vkCommandPool;
    result = vkCreateCommandPool(vkDevice, &cmdpoolInfo, NULL, &vkCommandPool);
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
    result = vkAllocateCommandBuffers(vkDevice, &cmdbufferInfo, &vkCmdBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate command buffer.");
        return -2;
    }

    VkQueue vkQueue;
    vkGetDeviceQueue(vkDevice, queueFamilyIndex_Graphics, 0, &vkQueue); // <-- TODO: Change "0" to a variable.



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




    // Geometry data. For this example it's simple - it's just 3 floats for position, 3 floats for color, interleaved. The hard part
    // is allocating memory and uploading it to a device local buffer.
    float geometryVertexData[] = {
         0.0f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f
    };

    uint32_t geometryIndexData[] = {
        0, 1, 2
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
    result = vkCreateBuffer(vkDevice, &bufferInfo, NULL, &vkBuffer);
    if (result != VK_SUCCESS) {
        printf("Failed to create buffer for geometry.");
        return -2;
    }

    VkMemoryRequirements bufferReqs;
    vkGetBufferMemoryRequirements(vkDevice, vkBuffer, &bufferReqs);

    VkMemoryAllocateInfo bufferMemoryInfo;
    bufferMemoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    bufferMemoryInfo.pNext = NULL;
    bufferMemoryInfo.allocationSize = bufferReqs.size;
    bufferMemoryInfo.memoryTypeIndex = (uint32_t)-1;    // <-- Set to -1 initially so we can check whether or not a valid memory type was found.

    // The memory type index is found from the physical device memory properties.
    VkPhysicalDeviceMemoryProperties memoryProps;
    vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevices[iPhysicalDevice], &memoryProps);
    for (uint32_t i = 0; i < memoryProps.memoryTypeCount; ++i) {
        if ((bufferReqs.memoryTypeBits & (1U << i)) != 0) {
            // The big is set which means the buffer can be used with this type of memory, but we want to make sure this buffer contains the memory
            // type we want.
            if ((memoryProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                bufferMemoryInfo.memoryTypeIndex = i;
                break;
            }
        }
    }

    VkDeviceMemory vkBufferMemory;
    result = vkAllocateMemory(vkDevice, &bufferMemoryInfo, NULL, &vkBufferMemory);
    if (result != VK_SUCCESS) {
        printf("Failed to allocate memory.");
        return -2;
    }

    result = vkBindBufferMemory(vkDevice, vkBuffer, vkBufferMemory, 0);
    if (result != VK_SUCCESS) {
        printf("Failed to bind buffer memory.");
        return -2;
    }

    // To get the data from our regular old C buffer into the VkBuffer we just need to map the VkBuffer, memcpy(), then unmap.
    void* pMappedBufferData;
    result = vkMapMemory(vkDevice, vkBufferMemory, 0, bufferReqs.size, 0, &pMappedBufferData);
    if (result != VK_SUCCESS) {
        printf("Failed to map buffer.");
        return -2;
    }

    memcpy(pMappedBufferData, geometryVertexData, sizeof(geometryVertexData));
    memcpy((void*)(((char*)pMappedBufferData) + sizeof(geometryVertexData)), geometryIndexData, sizeof(geometryIndexData));

    vkUnmapMemory(vkDevice, vkBufferMemory);
    


    static float r = 0.2f;

    // Main loop.
    for (;;) {
#ifdef _WIN32
        MSG msg;
        if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return msg.wParam;  // Received a quit message.
            }

            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
#endif
     
        uint32_t imageIndex;
        vkResult = vkAcquireNextImageKHR(vkDevice, vkSwapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &imageIndex);
        if (vkResult != VK_SUCCESS && vkResult != VK_SUBOPTIMAL_KHR) {
            return -1;
        }

        if (vkResult == VK_ERROR_OUT_OF_DATE_KHR) {
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
            clearValues[0].color.float32[0] = r; //r += 0.02f;
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

                vkCmdBindPipeline(vkCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(vkCmdBuffer, 0, 1, &vkBuffer, &offset);
                vkCmdBindIndexBuffer(vkCmdBuffer, vkBuffer, sizeof(geometryVertexData), VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(vkCmdBuffer, 3, 1, 0, 0, 0);
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
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = NULL;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &semaphore;
        submitInfo.pWaitDstStageMask = pWaitDstStageMask;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vkCmdBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &semaphore;
        result = vkQueueSubmit(vkQueue, 1, &submitInfo, 0);
        if (result != VK_SUCCESS) {
            printf("Failed to submit buffer.");
            return -2;
        }

        VkPresentInfoKHR info;
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.pNext = NULL;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &semaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &vkSwapchain;
        info.pImageIndices = &imageIndex;
        info.pResults = NULL;
        vkQueuePresentKHR(vkQueue, &info);
    }

    /* Unreachable. */
    /*return 0;*/
}