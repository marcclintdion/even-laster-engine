#ifndef RENDERTARGET_H
#define RENDERTARGET_H

#include "../vulkan.h"

class RenderTargetBase {
protected:
	RenderTargetBase(VkFormat format, VkImageType imageType, VkImageViewType imageViewType, int width, int height, int depth, int arrayLayers, VkImageUsageFlags usage, VkImageAspectFlags aspect) :
		format(format),
		width(width),
		height(height),
		depth(depth),
		arrayLayers(arrayLayers)
	{
		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = imageType;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { (uint32_t)width, (uint32_t)height, (uint32_t)depth };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = arrayLayers;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = usage;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult err = vkCreateImage(device, &imageCreateInfo, nullptr, &image);
		assert(err == VK_SUCCESS);

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, image, &memoryRequirements);

		auto memoryTypeIndex = getMemoryTypeIndex(memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		auto deviceMemory = allocateDeviceMemory(memoryRequirements.size, memoryTypeIndex);

		err = vkBindImageMemory(device, image, deviceMemory, 0);
		assert(err == VK_SUCCESS);

		VkImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = aspect;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = arrayLayers;

		imageView = createImageView(image, imageViewType, format, subresourceRange);
	}

public:
	VkFormat getFormat() { return format; }

	int getWidth() const { return width; }
	int getHeight() const { return height; }
	int getDepth() const { return depth; }

	int getArrayLayers() const { return arrayLayers; }

	VkImage getImage() { return image; }
	VkImageView getImageView() { return imageView; }

protected:
	VkFormat format;

	int width, height, depth;
	int arrayLayers;

	VkImage image;
	VkImageView imageView;
};

class ColorRenderTarget : public RenderTargetBase {
public:
	ColorRenderTarget(VkFormat format, int width, int height, VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) :
		RenderTargetBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, width, height, 1, 1, usage, VK_IMAGE_ASPECT_COLOR_BIT)
	{
	}
};

class DepthRenderTarget : public RenderTargetBase {
public:
	DepthRenderTarget(VkFormat format, int width, int height, VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) :
		RenderTargetBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, width, height, 1, 1, usage, VK_IMAGE_ASPECT_DEPTH_BIT)
	{
	}
};

class Texture2DArrayRenderTarget : public RenderTargetBase {
public:
	Texture2DArrayRenderTarget(VkFormat format, int width, int height, int arrayLayers, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) :
		RenderTargetBase(format, VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY, width, height, 1, arrayLayers, usage, VK_IMAGE_ASPECT_COLOR_BIT)
	{
		arrayImageViews.reserve(arrayLayers);
		for (int i = 0; i < arrayLayers; ++i) {
			VkImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.baseArrayLayer = i;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;
			arrayImageViews.push_back(createImageView(image, VK_IMAGE_VIEW_TYPE_2D, format, subresourceRange));
		}
	}

	const std::vector<VkImageView> &getArrayImageViews() const
	{
		return arrayImageViews;
	}

private:
	std::vector<VkImageView> arrayImageViews;
};

#endif // RENDERTARGET_H
