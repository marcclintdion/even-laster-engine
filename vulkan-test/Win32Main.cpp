// vulkan-test.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "vulkan.h"

using namespace vulkan;

#define MAX_LOADSTRING 100

// Global Variables:
static HWND hWnd;

// Forward declarations of functions included in this code module:
static ATOM registerWindowClass(HINSTANCE hInstance);
static BOOL initInstance(HINSTANCE, int);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

static VkSurfaceKHR surface;

#include <vector>

std::vector<uint8_t> loadBinaryFile(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		throw new std::exception("no such file!");

	fseek(fp, 0L, SEEK_END);
	long int size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	std::vector<uint8_t> data;
	data.resize(size);
	size_t read = fread(data.data(), size, 1, fp);
	assert(read == 1);

	return data;
}

VkShaderModule loadShaderModule(const char *path, VkDevice device, VkShaderStageFlagBits stage)
{
	std::vector<uint8_t> shaderCode = loadBinaryFile(path);
	assert(shaderCode.size() > 0);

	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo moduleCreateInfo;
	VkResult err;

	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = NULL;

	moduleCreateInfo.codeSize = shaderCode.size();
	moduleCreateInfo.pCode = (uint32_t *)shaderCode.data();
	moduleCreateInfo.flags = 0;
	err = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule);
	assert(!err);

	return shaderModule;
}

VkPipelineShaderStageCreateInfo loadShader(const char * fileName, VkDevice device, VkShaderStageFlagBits stage, const char *name = "main")
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = loadShaderModule(fileName, device, stage);
	shaderStage.pName = name;
	assert(shaderStage.module != NULL);
	return shaderStage;
}

int setup(int width, int height, bool fullscreen, const char *appName, HINSTANCE hInstance, int nCmdShow)
{
	// Initialize global strings
	ATOM windowClass = registerWindowClass(hInstance);

	if (fullscreen) {
		DEVMODE devmode;
		memset(&devmode, 0, sizeof(DEVMODE));
		devmode.dmSize = sizeof(DEVMODE);
		devmode.dmPelsWidth = width;
		devmode.dmPelsHeight = height;
		devmode.dmBitsPerPel = 32;
		devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		if (ChangeDisplaySettings(&devmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			return false;

		ShowCursor(FALSE);
	}

	// Perform application initialization:
	DWORD ws = WS_OVERLAPPEDWINDOW;
	RECT rect = { 0, 0, width, height };
	if (!fullscreen)
		AdjustWindowRect(&rect, ws, FALSE);
	else
		ws = WS_POPUP;

	hWnd = CreateWindow((LPCSTR)windowClass, appName, ws, 0, 0, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hInstance, NULL);
	if (!hWnd)
		return false;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return true;
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkBuffer *buffer)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	VkResult err = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);
	assert(err == VK_SUCCESS);
}

void allocateDeviceMemory(const VkMemoryRequirements &memoryRequirements, VkDeviceMemory *deviceMemory, VkDeviceSize *deviceMemorySize, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;

	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
		if (((memoryRequirements.memoryTypeBits >> i) & 1) == 1) {
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
				memoryAllocateInfo.memoryTypeIndex = i;
				break;
			}
		}
	}

	VkResult err = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, deviceMemory);
	if (deviceMemorySize != nullptr)
		*deviceMemorySize = memoryRequirements.size;
	assert(err == VK_SUCCESS);
}

void allocateBufferDeviceMemory(VkBuffer buffer, VkDeviceMemory *deviceMemory, VkDeviceSize *deviceMemorySize, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

	allocateDeviceMemory(memoryRequirements, deviceMemory, deviceMemorySize, propertyFlags);

	VkResult err = vkBindBufferMemory(device, buffer, *deviceMemory, 0);
	assert(!err);
}

void allocateImageDeviceMemory(VkImage image, VkDeviceMemory *deviceMemory, VkDeviceSize *deviceMemorySize, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, image, &memoryRequirements);

	allocateDeviceMemory(memoryRequirements, deviceMemory, deviceMemorySize, propertyFlags);

	VkResult err = vkBindImageMemory(device, image, *deviceMemory, 0);
	assert(!err);
}

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	const char *appName = "some excess demo";
	int width = 1280, height = 720;
#ifdef NDEBUG
	bool fullscreen = true;
#else
	bool fullscreen = false;
#endif

	if (!setup(width, height, fullscreen, appName, hInstance, nCmdShow))
		return FALSE;

	VkResult err = init(appName);
	if (err != VK_SUCCESS)
		return FALSE;

//	vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32* pSupported);

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = hInstance;
	surfaceCreateInfo.hwnd = hWnd;
	err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
	assert(err == VK_SUCCESS);

#if 0
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
	assert(queueCount >= 1);

	bool *supportsPresent = new bool[queueCount], anySupportsPresent = false;
	for (uint32_t i = 0; i < queueCount; i++) {
		VkBool32 tmp;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &tmp);
		supportsPresent[i] = !!tmp;
		anySupportsPresent = anySupportsPresent || supportsPresent[i];
	}
	assert(anySupportsPresent);
#endif

	// cheapo-version of the above:
	VkBool32 surfaceSupported = VK_FALSE;
	err = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueIndex, surface, &surfaceSupported);
	assert(err == VK_SUCCESS);
	assert(surfaceSupported == VK_TRUE);

	VkSurfaceCapabilitiesKHR surfCaps;
	err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);
	assert(err == VK_SUCCESS);

	uint32_t surfaceFormatCount = 0;
	err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL);
	assert(err == VK_SUCCESS);
	assert(surfaceFormatCount > 0);
	auto surfaces = new VkSurfaceFormatKHR[surfaceFormatCount];
	err = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaces);
	assert(err == VK_SUCCESS);

	VkFormat colorFormat = surfaces[0].format; // TODO: find optimal

	uint32_t presentModeCount = 0;
	err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);
	assert(err == VK_SUCCESS);
	assert(presentModeCount > 0);
	auto presentModes = new VkPresentModeKHR[presentModeCount];
	err = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);
	assert(err == VK_SUCCESS);

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = min(surfCaps.minImageCount + 1, surfCaps.maxImageCount);
	swapchainCreateInfo.imageFormat = colorFormat;
	swapchainCreateInfo.imageColorSpace = surfaces[0].colorSpace;
	swapchainCreateInfo.imageExtent = { width, height };
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = NULL;

	swapchainCreateInfo.presentMode = presentModes[0];
	for (uint32_t i = 0; i < presentModeCount; ++i)
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			swapchainCreateInfo.presentMode = presentModes[i];

	swapchainCreateInfo.oldSwapchain = NULL;
	swapchainCreateInfo.clipped = true;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	VkSwapchainKHR swapChain = VK_NULL_HANDLE;
	err = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapChain);
	assert(err == VK_SUCCESS);

	uint32_t imageCount;
	err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL);
	assert(err == VK_SUCCESS);
	assert(imageCount > 0);

	// Get the swap chain images
	auto images = new VkImage[imageCount];
	auto imageViews = new VkImageView[imageCount];
	err = vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images);
	assert(err == VK_SUCCESS);

	for (uint32_t i = 0; i < imageCount; i++) {
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = colorFormat;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;
		colorAttachmentView.image = images[i];

		err = vkCreateImageView(device, &colorAttachmentView, nullptr, &imageViews[i]);
		assert(err == VK_SUCCESS);
	}

	VkAttachmentDescription attachments[1];
	attachments[0].flags = 0;
	attachments[0].format = colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;

	VkRenderPassCreateInfo renderpassCreateInfo = {};
	renderpassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassCreateInfo.attachmentCount = ARRAY_SIZE(attachments);
	renderpassCreateInfo.pAttachments = attachments;
	renderpassCreateInfo.subpassCount = 1;
	renderpassCreateInfo.pSubpasses = &subpass;

	VkRenderPass renderPass;
	err = vkCreateRenderPass(device, &renderpassCreateInfo, nullptr, &renderPass);
	assert(err == VK_SUCCESS);

	VkImageView framebufferAttachments[1];
	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = renderPass;
	framebufferCreateInfo.attachmentCount = ARRAY_SIZE(framebufferAttachments);
	framebufferCreateInfo.pAttachments = framebufferAttachments;
	framebufferCreateInfo.width = width;
	framebufferCreateInfo.height = height;
	framebufferCreateInfo.layers = 1;

	auto framebuffers = new VkFramebuffer[imageCount];
	for (uint32_t i = 0; i < imageCount; i++) {
		framebufferAttachments[0] = imageViews[i];
		VkResult err = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffers[i]);
		assert(err == VK_SUCCESS);
	}

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = graphicsQueueIndex;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool commandPool;
	err = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
	assert(err == VK_SUCCESS);

	VkCommandBufferAllocateInfo commandAllocInfo = { };
	commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandAllocInfo.commandPool = commandPool;
	commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandAllocInfo.commandBufferCount = imageCount;
	
	auto commandBuffers = new VkCommandBuffer[imageCount];
	err = vkAllocateCommandBuffers(device, &commandAllocInfo, commandBuffers);
	assert(err == VK_SUCCESS);

	// OK, let's prepare for rendering!

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR; // TODO: VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.flags = 0;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.extent = { 64, 64, 1 };
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

	VkImage textureImage;
	err = vkCreateImage(device, &imageCreateInfo, nullptr, &textureImage);
	assert(err == VK_SUCCESS);

/*
	VkImageLayout textureImageLayout;
	textureImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
*/

	VkDeviceMemory textureDeviceMemory;
	VkDeviceSize textureDeviceMemorySize;
	allocateImageDeviceMemory(textureImage, &textureDeviceMemory, &textureDeviceMemorySize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT); // TODO: get rid of VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT

	{
		VkImageSubresource subRes = {};
		subRes.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		VkSubresourceLayout subresourceLayout;
		vkGetImageSubresourceLayout(device, textureImage, &subRes, &subresourceLayout);

		void *mappedMemory;
		// ERROR: [DS] Code 6 : Cannot map an image with layout VK_IMAGE_LAYOUT_UNDEFINED. Only GENERAL or PREINITIALIZED are supported.
		err = vkMapMemory(device, textureDeviceMemory, 0, textureDeviceMemorySize, 0, &mappedMemory);
		assert(err == VK_SUCCESS);
		for (int y = 0; y < 64; ++y) {
			uint8_t *row = (uint8_t *)mappedMemory + subresourceLayout.rowPitch * y;
			for (int x = 0; x < 64; ++x) {
				uint8_t tmp = ((x ^ y) & 16) != 0 ? 0xFF : 0x00;
				row[x * 4 + 0] = tmp;
				row[x * 4 + 1] = ~tmp;
				row[x * 4 + 2] = tmp;
				row[x * 4 + 3] = 0xFF;
			}
		}
		vkUnmapMemory(device, textureDeviceMemory);
	}

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = textureImage;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = imageCreateInfo.format;
	imageViewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.subresourceRange.levelCount = 1;

	VkImageView textureImageView;
	err = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &textureImageView);
	assert(err == VK_SUCCESS);

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.maxAnisotropy = 8;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

	VkSampler textureSampler;
	err = vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureSampler);
	assert(err == VK_SUCCESS);

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		loadShader("shaders/triangle.vert.spv", device, VK_SHADER_STAGE_VERTEX_BIT),
		loadShader("shaders/triangle.frag.spv", device, VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkVertexInputBindingDescription vertexInputBindingDesc[1];
	vertexInputBindingDesc[0].binding = 0;
	vertexInputBindingDesc[0].stride = sizeof(float) * 3;
	vertexInputBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexInputAttributeDescription[1];
	vertexInputAttributeDescription[0].binding = 0;
	vertexInputAttributeDescription[0].location = 0;
	vertexInputAttributeDescription[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexInputAttributeDescription[0].offset = 0;

	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = {};
	pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = ARRAY_SIZE(vertexInputBindingDesc);
	pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindingDesc;
	pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = ARRAY_SIZE(vertexInputAttributeDescription);
	pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription;

	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo = {};
	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineInputAssemblyStateCreateInfo.flags = 0;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo = {};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.flags = 0;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_TRUE;

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState[1];
	pipelineColorBlendAttachmentState[0].colorWriteMask = 0xf;
	pipelineColorBlendAttachmentState[0].blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo = {};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.attachmentCount = ARRAY_SIZE(pipelineColorBlendAttachmentState);
	pipelineColorBlendStateCreateInfo.pAttachments = pipelineColorBlendAttachmentState;

	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo = {};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo = {};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.scissorCount = 1;

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo = {};
	pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;

	VkDescriptorSetLayoutBinding layoutBindings[2] = {};
	layoutBindings[0].binding = 0;
	layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBindings[0].descriptorCount = 1;
	layoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBindings[1].binding = 1;
	layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBindings[1].descriptorCount = 1;
	layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = { };
	descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descSetLayoutCreateInfo.bindingCount = ARRAY_SIZE(layoutBindings);
	descSetLayoutCreateInfo.pBindings = layoutBindings;

	VkDescriptorSetLayout descriptorSetLayout;
	err = vkCreateDescriptorSetLayout(device, &descSetLayoutCreateInfo, NULL, &descriptorSetLayout);
	assert(err == VK_SUCCESS);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { };
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreateInfo.setLayoutCount = 1;

	VkPipelineLayout pipelineLayout;
	vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &pipelineLayout);

	VkDynamicState dynamicStateEnables[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo = {};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStateEnables;
	pipelineDynamicStateCreateInfo.dynamicStateCount = ARRAY_SIZE(dynamicStateEnables);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	pipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	pipelineCreateInfo.stageCount = ARRAY_SIZE(shaderStages);
	pipelineCreateInfo.pStages = shaderStages;

	VkPipeline pipeline;
	err = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(err == VK_SUCCESS);

	VkDescriptorPoolSize descriptorPoolSizes[2];
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.poolSizeCount = ARRAY_SIZE(descriptorPoolSizes);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
	descriptorPoolCreateInfo.maxSets = 1;

	VkDescriptorPool descriptorPool;
	err = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, NULL, &descriptorPool);
	assert(err == VK_SUCCESS);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	VkBuffer uniformBuffer;
	VkDeviceSize uniformBufferSize = sizeof(float) * 4 * 4;
	createBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &uniformBuffer);

	VkDeviceMemory uniformDeviceMemory;
	VkDeviceSize uniformDeviceMemorySize;
	allocateBufferDeviceMemory(uniformBuffer, &uniformDeviceMemory, &uniformDeviceMemorySize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	VkDescriptorBufferInfo descriptorBufferInfo = {};
	descriptorBufferInfo.buffer = uniformBuffer;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = VK_WHOLE_SIZE;

	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfo.imageView = textureImageView;
	descriptorImageInfo.sampler = textureSampler;

	VkDescriptorSet descriptorSet;
	err = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
	assert(err == VK_SUCCESS);

	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = descriptorSet;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
	writeDescriptorSet.dstBinding = 0;
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.pBufferInfo = NULL;
	writeDescriptorSet.pImageInfo = &descriptorImageInfo;
	writeDescriptorSet.dstBinding = 1;
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, NULL);

	// Go make vertex buffer yo!

	float vertexData[] = {
		 1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		 0.0f, -1.0f, 0.0f
	};

	VkBuffer vertexBuffer;
	createBuffer(sizeof(vertexData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vertexBuffer);

	VkDeviceMemory vertexDeviceMemory;
	VkDeviceSize vertexDeviceMemorySize;
	allocateBufferDeviceMemory(vertexBuffer, &vertexDeviceMemory, &vertexDeviceMemorySize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	{
		void *mappedMemory;
		err = vkMapMemory(device, vertexDeviceMemory, 0, vertexDeviceMemorySize, 0, &mappedMemory);
		assert(err == VK_SUCCESS);
		memcpy(mappedMemory, vertexData, sizeof(vertexData));
		vkUnmapMemory(device, vertexDeviceMemory);
	}

	bool done = false;
	DWORD startTick = GetTickCount();
	while (!done) {
		DWORD tick = GetTickCount() - startTick;
		float time = tick / 1000.0f;

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkSemaphore presentCompleteSemaphore;
		err = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &presentCompleteSemaphore);
		assert(err == VK_SUCCESS);

		uint32_t currentSwapImage;
		err = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, NULL, &currentSwapImage);
		assert(err == VK_SUCCESS);

		VkCommandBuffer commandBuffer = commandBuffers[currentSwapImage];
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		err = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		assert(err == VK_SUCCESS);

		VkClearValue clearValue;
		clearValue = { { {
			0.5f,
			0.5f,
			0.5f,
			1.0f
		} } };

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;
		renderPassBeginInfo.framebuffer = framebuffers[currentSwapImage];

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = {};
		viewport.height = (float)height;
		viewport.width = (float)width;
		viewport.minDepth = (float)0.0f;
		viewport.maxDepth = (float)1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor = {};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		float th = time;
		float a = float(width) / height;
		float uniformData[] = {
			cos(th) / a, -sin(th), 0.0f, 0.0f,
			sin(th) / a,  cos(th), 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};

		{
			void *mappedUniformMemory = nullptr;
			err = vkMapMemory(device, uniformDeviceMemory, 0, uniformBufferSize, 0, &mappedUniformMemory);
			assert(err == VK_SUCCESS);

			memcpy(mappedUniformMemory, uniformData, sizeof(uniformData));
			vkUnmapMemory(device, uniformDeviceMemory);
		}

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		err = vkEndCommandBuffer(commandBuffer);
		assert(err == VK_SUCCESS);

		VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
		submitInfo.pWaitDstStageMask = &waitDstStageMask;
		submitInfo.waitSemaphoreCount = 0;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		// Submit draw command buffer
		err = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		assert(err == VK_SUCCESS);

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &currentSwapImage;
		err = vkQueuePresentKHR(graphicsQueue, &presentInfo);
		assert(err == VK_SUCCESS);

		err = vkQueueWaitIdle(graphicsQueue);
		assert(err == VK_SUCCESS);

		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			/* handle keys-events */
	
			if (WM_QUIT == msg.message ||
			    WM_CLOSE == msg.message ||
			    (WM_KEYDOWN == msg.message && VK_ESCAPE == LOWORD(msg.wParam)))
				done = true;
		}
	}

	return 0;
}

ATOM registerWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style         = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc   = WndProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = hInstance;
	wcex.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)NULL;
	wcex.lpszMenuName  = NULL;
	wcex.lpszClassName = "vulkanwin";
	wcex.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

	return RegisterClassEx(&wcex);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}