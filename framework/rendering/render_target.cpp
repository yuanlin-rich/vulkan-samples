/* Copyright (c) 2019-2025, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "rendering/render_target.h"
#include "core/device.h"
#include "core/image.h"

namespace vkb
{
namespace
{
struct CompareExtent2D
{
	bool operator()(const VkExtent2D &lhs, const VkExtent2D &rhs) const
	{
		return !(lhs.width == rhs.width && lhs.height == rhs.height) && (lhs.width < rhs.width && lhs.height < rhs.height);
	}
};
}        // namespace

Attachment::Attachment(VkFormat format, VkSampleCountFlagBits samples, VkImageUsageFlags usage) :
    format{format},
    samples{samples},
    usage{usage}
{
}
const RenderTarget::CreateFunc RenderTarget::DEFAULT_CREATE_FUNC = [](core::Image &&swapchain_image) -> std::unique_ptr<RenderTarget> {
	// 默认的创建渲染目标的函数
	// 输入从swap chain中获取的图像，同时创建一张图像作为深度模板缓存
	VkFormat depth_format = get_suitable_depth_format(swapchain_image.get_device().get_gpu().get_handle());

	core::Image depth_image{swapchain_image.get_device(), swapchain_image.get_extent(),
	                        depth_format,
	                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
	                        VMA_MEMORY_USAGE_GPU_ONLY};

	std::vector<core::Image> images;
	images.push_back(std::move(swapchain_image));
	images.push_back(std::move(depth_image));

	return std::make_unique<RenderTarget>(std::move(images));
};

vkb::RenderTarget::RenderTarget(std::vector<core::Image> &&images) :
    device{images.back().get_device()},
    images{std::move(images)}
{
	// 根据传入的image创建渲染目标，至少一张图像
	assert(!this->images.empty() && "Should specify at least 1 image");

	std::set<VkExtent2D, CompareExtent2D> unique_extent;

	// Returns the image extent as a VkExtent2D structure from a VkExtent3D
	auto get_image_extent = [](const core::Image &image) { return VkExtent2D{image.get_extent().width, image.get_extent().height}; };

	// Constructs a set of unique image extents given a vector of images
	std::transform(this->images.begin(), this->images.end(), std::inserter(unique_extent, unique_extent.end()), get_image_extent);

	// Allow only one extent size for a render target
	// 所有的图像必须宽度高度相同
	if (unique_extent.size() != 1)
	{
		throw VulkanException{VK_ERROR_INITIALIZATION_FAILED, "Extent size is not unique"};
	}

	extent = *unique_extent.begin();

	for (auto &image : this->images)
	{
		// 只支持2d图像
		if (image.get_type() != VK_IMAGE_TYPE_2D)
		{
			throw VulkanException{VK_ERROR_INITIALIZATION_FAILED, "Image type is not 2D"};
		}

		// 根据图像创建图像视图，不额外制定参数的话，图像视图的格式等和图像保持一致
		views.emplace_back(image, VK_IMAGE_VIEW_TYPE_2D);

		// 创建attachments
		attachments.emplace_back(Attachment{image.get_format(), image.get_sample_count(), image.get_usage()});
	}
}

vkb::RenderTarget::RenderTarget(std::vector<core::ImageView> &&image_views) :
    device{const_cast<core::Image &>(image_views.back().get_image()).get_device()},
    images{},
    views{std::move(image_views)}
{
	// 根据传入的image view创建渲染目标，至少一张图像视图
	assert(!views.empty() && "Should specify at least 1 image view");

	std::set<VkExtent2D, CompareExtent2D> unique_extent;

	// Returns the extent of the base mip level pointed at by a view
	auto get_view_extent = [](const core::ImageView &view) {
		const VkExtent3D mip0_extent = view.get_image().get_extent();
		const uint32_t   mip_level   = view.get_subresource_range().baseMipLevel;
		return VkExtent2D{mip0_extent.width >> mip_level, mip0_extent.height >> mip_level};
	};

	// Constructs a set of unique image extents given a vector of image views;
	// allow only one extent size for a render target
	// 所有的图像视图尺寸必须相同
	std::transform(views.begin(), views.end(), std::inserter(unique_extent, unique_extent.end()), get_view_extent);
	if (unique_extent.size() != 1)
	{
		throw VulkanException{VK_ERROR_INITIALIZATION_FAILED, "Extent size is not unique"};
	}
	extent = *unique_extent.begin();

	for (auto &view : views)
	{
		const auto &image = view.get_image();
		attachments.emplace_back(Attachment{image.get_format(), image.get_sample_count(), image.get_usage()});
	}
}

const VkExtent2D &RenderTarget::get_extent() const
{
	return extent;
}

const std::vector<core::ImageView> &RenderTarget::get_views() const
{
	return views;
}

const std::vector<Attachment> &RenderTarget::get_attachments() const
{
	return attachments;
}

// 通过指引指定哪些attachments是input attachments
void RenderTarget::set_input_attachments(std::vector<uint32_t> &input)
{
	input_attachments = input;
}

const std::vector<uint32_t> &RenderTarget::get_input_attachments() const
{
	return input_attachments;
}

// out attachment，其实就是某个sub pass的color attachment，用索引指定
void RenderTarget::set_output_attachments(std::vector<uint32_t> &output)
{
	output_attachments = output;
}

const std::vector<uint32_t> &RenderTarget::get_output_attachments() const
{
	return output_attachments;
}

void RenderTarget::set_layout(uint32_t attachment, VkImageLayout layout)
{
	attachments[attachment].initial_layout = layout;
}

VkImageLayout RenderTarget::get_layout(uint32_t attachment) const
{
	return attachments[attachment].initial_layout;
}

}        // namespace vkb
