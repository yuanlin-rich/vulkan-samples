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

#pragma once

#include <unordered_set>

#include "builder_base.h"
#include "common/helpers.h"
#include "common/vk_common.h"
#include "core/allocated.h"
#include "core/vulkan_resource.h"

namespace vkb
{
namespace core
{
template <vkb::BindingType bindingType>
class Device;
using DeviceC = Device<vkb::BindingType::C>;

class Image;
using ImagePtr = std::unique_ptr<Image>;

struct ImageBuilder : public vkb::allocated::BuilderBaseC<ImageBuilder, VkImageCreateInfo>
{
  private:
	using Parent = vkb::allocated::BuilderBaseC<ImageBuilder, VkImageCreateInfo>;

  public:
	ImageBuilder(VkExtent3D const &extent) :
	    Parent(VkImageCreateInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr})
	{
		VkImageCreateInfo &create_info = get_create_info();
		create_info.extent             = extent;
		create_info.arrayLayers        = 1;
		create_info.mipLevels          = 1;
		create_info.imageType          = VK_IMAGE_TYPE_2D;
		create_info.format             = VK_FORMAT_R8G8B8A8_UNORM;
		create_info.samples            = VK_SAMPLE_COUNT_1_BIT;
	}

	ImageBuilder(uint32_t width, uint32_t height = 1, uint32_t depth = 1) :
	    ImageBuilder(VkExtent3D{width, height, depth})
	{
	}

	ImageBuilder &with_format(VkFormat format)
	{
		get_create_info().format = format;
		return *this;
	}

	ImageBuilder &with_usage(VkImageUsageFlags usage)
	{
		get_create_info().usage = usage;
		return *this;
	}

	ImageBuilder &with_flags(VkImageCreateFlags flags)
	{
		get_create_info().flags = flags;
		return *this;
	}

	ImageBuilder &with_image_type(VkImageType type)
	{
		get_create_info().imageType = type;
		return *this;
	}

	ImageBuilder &with_array_layers(uint32_t layers)
	{
		get_create_info().arrayLayers = layers;
		return *this;
	}

	ImageBuilder &with_mip_levels(uint32_t levels)
	{
		get_create_info().mipLevels = levels;
		return *this;
	}

	ImageBuilder &with_sample_count(VkSampleCountFlagBits sample_count)
	{
		get_create_info().samples = sample_count;
		return *this;
	}

	ImageBuilder &with_tiling(VkImageTiling tiling)
	{
		get_create_info().tiling = tiling;
		return *this;
	}

	template <typename ExtensionType>
	ImageBuilder &with_extension(ExtensionType &extension)
	{
		extension.pNext = create_info.pNext;

		create_info.pNext = &extension;

		return *this;
	}

	Image    build(Device<vkb::BindingType::C> &device) const;
	ImagePtr build_unique(Device<vkb::BindingType::C> &device) const;
};

class ImageView;

class Image : public vkb::allocated::AllocatedC<VkImage>
{
  public:
	Image(vkb::core::DeviceC   &device,
	      VkImage               handle,
	      const VkExtent3D     &extent,
	      VkFormat              format,
	      VkImageUsageFlags     image_usage,
	      VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

	// [[deprecated("Use the ImageBuilder ctor instead")]]
	Image(
	    vkb::core::DeviceC   &device,
	    const VkExtent3D     &extent,
	    VkFormat              format,
	    VkImageUsageFlags     image_usage,
	    VmaMemoryUsage        memory_usage       = VMA_MEMORY_USAGE_AUTO,
	    VkSampleCountFlagBits sample_count       = VK_SAMPLE_COUNT_1_BIT,
	    uint32_t              mip_levels         = 1,
	    uint32_t              array_layers       = 1,
	    VkImageTiling         tiling             = VK_IMAGE_TILING_OPTIMAL,
	    VkImageCreateFlags    flags              = 0,
	    uint32_t              num_queue_families = 0,
	    const uint32_t       *queue_families     = nullptr);

	Image(vkb::core::DeviceC &device, ImageBuilder const &builder);

	Image(const Image &) = delete;

	Image(Image &&other) noexcept;

	~Image() override;

	Image &operator=(const Image &) = delete;

	Image &operator=(Image &&) = delete;

	// 图像类型，1D，2D还是3D
	VkImageType get_type() const;

	// 图像尺寸
	const VkExtent3D &get_extent() const;

	// 图像格式
	VkFormat get_format() const;

	// 图像采样数
	VkSampleCountFlagBits get_sample_count() const;

	// 图像用途
	VkImageUsageFlags get_usage() const;

	// 图像tiling格式
	VkImageTiling get_tiling() const;

	// 图像的子资源
	const VkImageSubresource &get_subresource() const;

	// 图像layer数量
	uint32_t get_array_layer_count() const;

	std::unordered_set<ImageView *> &get_views();

	// 图像需要的内存大小
	VkDeviceSize get_image_required_size() const;

	// 图像应用的压缩格式
	VkImageCompressionPropertiesEXT get_applied_compression() const;

  private:
	/// Image views referring to this image
	// 保留常见图像的信息
	VkImageCreateInfo               create_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

	// 图像的子资源，包含三个方面
	// 图像的颜色数据，深度数据，模板数据
	// 图像mipmap的层数
	// 图像array的索引
	VkImageSubresource              subresource{};
	std::unordered_set<ImageView *> views;
};
}        // namespace core
}        // namespace vkb
