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

#include "core/swapchain.h"

#include "core/util/logging.hpp"
#include "device.h"
#include "image.h"

namespace vkb
{
namespace
{

// 选择图像数量，确保它在允许的范围内
inline uint32_t choose_image_count(
    uint32_t request_image_count,
    uint32_t min_image_count,
    uint32_t max_image_count)
{
	if (max_image_count != 0)
	{
		request_image_count = std::min(request_image_count, max_image_count);
	}

	request_image_count = std::max(request_image_count, min_image_count);

	return request_image_count;
}

// 选择图像数组层数，确保它在允许的范围内
inline uint32_t choose_image_array_layers(
    uint32_t request_image_array_layers,
    uint32_t max_image_array_layers)
{
	request_image_array_layers = std::min(request_image_array_layers, max_image_array_layers);
	request_image_array_layers = std::max(request_image_array_layers, 1u);

	return request_image_array_layers;
}

// 选择交换链的尺寸，如果当前无效，则使用请求的尺寸
// 如果当前尺寸有效，则确保请求的尺寸在允许的范围内
inline VkExtent2D choose_extent(
    VkExtent2D        request_extent,
    const VkExtent2D &min_image_extent,
    const VkExtent2D &max_image_extent,
    const VkExtent2D &current_extent)
{
	if (current_extent.width == 0xFFFFFFFF)
	{
		return request_extent;
	}

	if (request_extent.width < 1 || request_extent.height < 1)
	{
		LOGW("(Swapchain) Image extent ({}, {}) not supported. Selecting ({}, {}).", request_extent.width, request_extent.height, current_extent.width, current_extent.height);
		return current_extent;
	}

	request_extent.width = std::max(request_extent.width, min_image_extent.width);
	request_extent.width = std::min(request_extent.width, max_image_extent.width);

	request_extent.height = std::max(request_extent.height, min_image_extent.height);
	request_extent.height = std::min(request_extent.height, max_image_extent.height);

	return request_extent;
}

// 选择交换链的显示模式，确保所选模式受支持
inline VkPresentModeKHR choose_present_mode(
    VkPresentModeKHR                     request_present_mode,
    const std::vector<VkPresentModeKHR> &available_present_modes,
    const std::vector<VkPresentModeKHR> &present_mode_priority_list)
{
	auto present_mode_it = std::ranges::find(available_present_modes, request_present_mode);

	if (present_mode_it == available_present_modes.end())
	{
		// If nothing found, always default to FIFO
		VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		for (auto &present_mode : present_mode_priority_list)
		{
			if (std::ranges::find(available_present_modes, present_mode) != available_present_modes.end())
			{
				chosen_present_mode = present_mode;
				break;
			}
		}

		LOGW("(Swapchain) Present mode '{}' not supported. Selecting '{}'.", to_string(request_present_mode), to_string(chosen_present_mode));
		return chosen_present_mode;
	}
	else
	{
		LOGI("(Swapchain) Present mode selected: {}", to_string(request_present_mode));
		return *present_mode_it;
	}
}

// 选择交换链的表面格式，确保所选格式受支持
inline VkSurfaceFormatKHR choose_surface_format(
    const VkSurfaceFormatKHR               requested_surface_format,
    const std::vector<VkSurfaceFormatKHR> &available_surface_formats,
    const std::vector<VkSurfaceFormatKHR> &surface_format_priority_list)
{
	// Try to find the requested surface format in the supported surface formats
	auto surface_format_it = std::ranges::find_if(
	    available_surface_formats,
	    [&requested_surface_format](const VkSurfaceFormatKHR &surface) {
		    if (surface.format == requested_surface_format.format &&
		        surface.colorSpace == requested_surface_format.colorSpace)
		    {
			    return true;
		    }

		    return false;
	    });

	// If the requested surface format isn't found, then try to request a format from the priority list
	if (surface_format_it == available_surface_formats.end())
	{
		for (auto &surface_format : surface_format_priority_list)
		{
			surface_format_it = std::ranges::find_if(
			    available_surface_formats,
			    [&surface_format](const VkSurfaceFormatKHR &surface) {
				    if (surface.format == surface_format.format &&
				        surface.colorSpace == surface_format.colorSpace)
				    {
					    return true;
				    }

				    return false;
			    });
			if (surface_format_it != available_surface_formats.end())
			{
				LOGW("(Swapchain) Surface format ({}) not supported. Selecting ({}).", to_string(requested_surface_format), to_string(*surface_format_it));
				return *surface_format_it;
			}
		}

		// If nothing found, default to the first supported surface format
		surface_format_it = available_surface_formats.begin();
		LOGW("(Swapchain) Surface format ({}) not supported. Selecting ({}).", to_string(requested_surface_format), to_string(*surface_format_it));
	}
	else
	{
		LOGI("(Swapchain) Surface format selected: {}", to_string(requested_surface_format));
	}

	return *surface_format_it;
}

// 选择交换链的转换方式，确保所选转换受支持
inline VkSurfaceTransformFlagBitsKHR choose_transform(
    VkSurfaceTransformFlagBitsKHR request_transform,
    VkSurfaceTransformFlagsKHR    supported_transform,
    VkSurfaceTransformFlagBitsKHR current_transform)
{
	if (request_transform & supported_transform)
	{
		return request_transform;
	}

	LOGW("(Swapchain) Surface transform '{}' not supported. Selecting '{}'.", to_string(request_transform), to_string(current_transform));

	return current_transform;
}

// 选择交换链的复合Alpha模式，确保所选模式受支持
inline VkCompositeAlphaFlagBitsKHR choose_composite_alpha(VkCompositeAlphaFlagBitsKHR request_composite_alpha, VkCompositeAlphaFlagsKHR supported_composite_alpha)
{
	if (request_composite_alpha & supported_composite_alpha)
	{
		return request_composite_alpha;
	}

	static const std::vector<VkCompositeAlphaFlagBitsKHR> composite_alpha_flags = {
	    VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
	    VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
	    VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
	    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};

	for (VkCompositeAlphaFlagBitsKHR composite_alpha : composite_alpha_flags)
	{
		if (composite_alpha & supported_composite_alpha)
		{
			LOGW("(Swapchain) Composite alpha '{}' not supported. Selecting '{}.", to_string(request_composite_alpha), to_string(composite_alpha));
			return composite_alpha;
		}
	}

	throw std::runtime_error("No compatible composite alpha found.");
}

// 验证图像使用标志是否与格式特性兼容
inline bool validate_format_feature(VkImageUsageFlagBits image_usage, VkFormatFeatureFlags supported_features)
{
	switch (image_usage)
	{
		case VK_IMAGE_USAGE_STORAGE_BIT:
			return VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT & supported_features;
		default:
			return true;
	}
}

// 选择图像使用标志，确保所选标志受支持
inline std::set<VkImageUsageFlagBits> choose_image_usage(const std::set<VkImageUsageFlagBits> &requested_image_usage_flags, VkImageUsageFlags supported_image_usage, VkFormatFeatureFlags supported_features)
{
	std::set<VkImageUsageFlagBits> validated_image_usage_flags;
	for (auto flag : requested_image_usage_flags)
	{
		if ((flag & supported_image_usage) && validate_format_feature(flag, supported_features))
		{
			validated_image_usage_flags.insert(flag);
		}
		else
		{
			LOGW("(Swapchain) Image usage ({}) requested but not supported.", to_string(flag));
		}
	}

	if (validated_image_usage_flags.empty())
	{
		// Pick the first format from list of defaults, if supported
		static const std::vector<VkImageUsageFlagBits> image_usage_flags = {
		    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		    VK_IMAGE_USAGE_STORAGE_BIT,
		    VK_IMAGE_USAGE_SAMPLED_BIT,
		    VK_IMAGE_USAGE_TRANSFER_DST_BIT};

		for (VkImageUsageFlagBits image_usage : image_usage_flags)
		{
			if ((image_usage & supported_image_usage) && validate_format_feature(image_usage, supported_features))
			{
				validated_image_usage_flags.insert(image_usage);
				break;
			}
		}
	}

	if (!validated_image_usage_flags.empty())
	{
		// Log image usage flags used
		std::string usage_list;
		for (VkImageUsageFlagBits image_usage : validated_image_usage_flags)
		{
			usage_list += to_string(image_usage) + " ";
		}
		LOGI("(Swapchain) Image usage flags: {}", usage_list);
	}
	else
	{
		throw std::runtime_error("No compatible image usage found.");
	}

	return validated_image_usage_flags;
}

// 组合图像使用标志为单个VkImageUsageFlags值
inline VkImageUsageFlags composite_image_flags(std::set<VkImageUsageFlagBits> &image_usage_flags)
{
	VkImageUsageFlags image_usage{};
	for (auto flag : image_usage_flags)
	{
		image_usage |= flag;
	}
	return image_usage;
}

}        // namespace

Swapchain::Swapchain(Swapchain &old_swapchain, const VkExtent2D &extent) :
    Swapchain{old_swapchain,
              old_swapchain.device,
              old_swapchain.surface,
              old_swapchain.properties.present_mode,
              old_swapchain.present_mode_priority_list,
              old_swapchain.surface_format_priority_list,
              extent,
              old_swapchain.properties.image_count,
              old_swapchain.properties.pre_transform,
              old_swapchain.image_usage_flags,
              old_swapchain.requested_compression,
              old_swapchain.requested_compression_fixed_rate}
{}

Swapchain::Swapchain(Swapchain &old_swapchain, const uint32_t image_count) :
    Swapchain{old_swapchain,
              old_swapchain.device,
              old_swapchain.surface,
              old_swapchain.properties.present_mode,
              old_swapchain.present_mode_priority_list,
              old_swapchain.surface_format_priority_list,
              old_swapchain.properties.extent,
              image_count,
              old_swapchain.properties.pre_transform,
              old_swapchain.image_usage_flags,
              old_swapchain.requested_compression,
              old_swapchain.requested_compression_fixed_rate}
{}

Swapchain::Swapchain(Swapchain &old_swapchain, const std::set<VkImageUsageFlagBits> &image_usage_flags) :
    Swapchain{old_swapchain,
              old_swapchain.device,
              old_swapchain.surface,
              old_swapchain.properties.present_mode,
              old_swapchain.present_mode_priority_list,
              old_swapchain.surface_format_priority_list,
              old_swapchain.properties.extent,
              old_swapchain.properties.image_count,
              old_swapchain.properties.pre_transform,
              image_usage_flags,
              old_swapchain.requested_compression,
              old_swapchain.requested_compression_fixed_rate}
{}

Swapchain::Swapchain(Swapchain &old_swapchain, const VkExtent2D &extent, const VkSurfaceTransformFlagBitsKHR transform) :
    Swapchain{old_swapchain,
              old_swapchain.device,
              old_swapchain.surface,
              old_swapchain.properties.present_mode,
              old_swapchain.present_mode_priority_list,
              old_swapchain.surface_format_priority_list,
              extent,
              old_swapchain.properties.image_count,
              transform,
              old_swapchain.image_usage_flags,
              old_swapchain.requested_compression,
              old_swapchain.requested_compression_fixed_rate}
{}

Swapchain::Swapchain(Swapchain &old_swapchain, const VkImageCompressionFlagsEXT requested_compression, const VkImageCompressionFixedRateFlagsEXT requested_compression_fixed_rate) :
    Swapchain{old_swapchain,
              old_swapchain.device,
              old_swapchain.surface,
              old_swapchain.properties.present_mode,
              old_swapchain.present_mode_priority_list,
              old_swapchain.surface_format_priority_list,
              old_swapchain.properties.extent,
              old_swapchain.properties.image_count,
              old_swapchain.properties.pre_transform,
              old_swapchain.image_usage_flags,
              requested_compression,
              requested_compression_fixed_rate}
{}

Swapchain::Swapchain(vkb::core::DeviceC                       &device,
                     VkSurfaceKHR                              surface,
                     const VkPresentModeKHR                    present_mode,
                     std::vector<VkPresentModeKHR> const      &present_mode_priority_list,
                     const std::vector<VkSurfaceFormatKHR>    &surface_format_priority_list,
                     const VkExtent2D                         &extent,
                     const uint32_t                            image_count,
                     const VkSurfaceTransformFlagBitsKHR       transform,
                     const std::set<VkImageUsageFlagBits>     &image_usage_flags,
                     const VkImageCompressionFlagsEXT          requested_compression,
                     const VkImageCompressionFixedRateFlagsEXT requested_compression_fixed_rate) :
    Swapchain{*this, device, surface, present_mode, present_mode_priority_list, surface_format_priority_list, extent, image_count, transform, image_usage_flags}
{
}

Swapchain::Swapchain(Swapchain                                &old_swapchain,
                     vkb::core::DeviceC                       &device,
                     VkSurfaceKHR                              surface,
                     const VkPresentModeKHR                    present_mode,
                     std::vector<VkPresentModeKHR> const      &present_mode_priority_list,
                     const std::vector<VkSurfaceFormatKHR>    &surface_format_priority_list,
                     const VkExtent2D                         &extent,
                     const uint32_t                            image_count,
                     const VkSurfaceTransformFlagBitsKHR       transform,
                     const std::set<VkImageUsageFlagBits>     &image_usage_flags,
                     const VkImageCompressionFlagsEXT          requested_compression,
                     const VkImageCompressionFixedRateFlagsEXT requested_compression_fixed_rate) :
    device{device},
    surface{surface},
    requested_compression{requested_compression},
    requested_compression_fixed_rate{requested_compression_fixed_rate}
{
	// 优先选择的present方式列表，如果列表内的present方式都不被支持，则使用FIFO作为默认值
	this->present_mode_priority_list   = present_mode_priority_list;

	// 优先选择的表面格式列表，如果列表内的表面格式都不被支持，则使用第一个支持的格式作为默认值
	this->surface_format_priority_list = surface_format_priority_list;

	// surface的能力，包含支持的图像数量、尺寸、使用标志等信息
	VkSurfaceCapabilitiesKHR surface_capabilities{};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->device.get_gpu().get_handle(), surface, &surface_capabilities);

	// 查询支持的表面格式
	uint32_t surface_format_count{0U};
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(this->device.get_gpu().get_handle(), surface, &surface_format_count, nullptr));
	std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(this->device.get_gpu().get_handle(), surface, &surface_format_count, surface_formats.data()));

	LOGI("Surface supports the following surface formats:");
	for (auto &surface_format : surface_formats)
	{
		LOGI("  \t{}", to_string(surface_format));
	}

	// 查询支持的present方式
	uint32_t present_mode_count{0U};
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(this->device.get_gpu().get_handle(), surface, &present_mode_count, nullptr));

	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(this->device.get_gpu().get_handle(), surface, &present_mode_count, present_modes.data()));

	LOGI("Surface supports the following present modes:");
	for (auto &pm : present_modes)
	{
		LOGI("  \t{}", to_string(pm));
	}

	// Choose best properties based on surface capabilities
	properties.old_swapchain  = old_swapchain.get_handle();
	properties.image_count    = choose_image_count(image_count, surface_capabilities.minImageCount, surface_capabilities.maxImageCount);
	properties.extent         = choose_extent(extent, surface_capabilities.minImageExtent, surface_capabilities.maxImageExtent, surface_capabilities.currentExtent);
	properties.surface_format = choose_surface_format(properties.surface_format, surface_formats, surface_format_priority_list);
	properties.array_layers   = choose_image_array_layers(1U, surface_capabilities.maxImageArrayLayers);

	VkFormatProperties format_properties;
	vkGetPhysicalDeviceFormatProperties(this->device.get_gpu().get_handle(), properties.surface_format.format, &format_properties);
	this->image_usage_flags = choose_image_usage(image_usage_flags, surface_capabilities.supportedUsageFlags, format_properties.optimalTilingFeatures);

	properties.image_usage     = composite_image_flags(this->image_usage_flags);
	properties.pre_transform   = choose_transform(transform, surface_capabilities.supportedTransforms, surface_capabilities.currentTransform);
	properties.composite_alpha = choose_composite_alpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, surface_capabilities.supportedCompositeAlpha);
	properties.present_mode    = choose_present_mode(present_mode, present_modes, present_mode_priority_list);

	VkSwapchainCreateInfoKHR create_info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	create_info.minImageCount    = properties.image_count;
	create_info.imageExtent      = properties.extent;
	create_info.presentMode      = properties.present_mode;
	create_info.imageFormat      = properties.surface_format.format;
	create_info.imageColorSpace  = properties.surface_format.colorSpace;
	create_info.imageArrayLayers = properties.array_layers;
	create_info.imageUsage       = properties.image_usage;
	create_info.preTransform     = properties.pre_transform;
	create_info.compositeAlpha   = properties.composite_alpha;
	create_info.oldSwapchain     = properties.old_swapchain;
	create_info.surface          = surface;

	// fixed_rate_flags: 复制请求的固定压缩率标志
	// compression_control: 创建图像压缩控制结构体
	// flags: 设置压缩控制标志（如默认压缩、固定率压缩或禁用压缩）
	auto                         fixed_rate_flags = requested_compression_fixed_rate;
	VkImageCompressionControlEXT compression_control{VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT};
	compression_control.flags = requested_compression;

	// 利用VK_EXT_image_compression_control_swapchain扩展来控制交换链图像的压缩方式，以平衡图像质量、性能和内存使用
	if (device.is_extension_enabled(VK_EXT_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_EXTENSION_NAME))
	{
		create_info.pNext = &compression_control;

		if (VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT == requested_compression)
		{
			// Do not support compression for multi-planar formats
			// 应用特定的固定压缩率（有损压缩）
			compression_control.compressionControlPlaneCount = 1;
			compression_control.pFixedRateFlags              = &fixed_rate_flags;
		}
		else if (VK_IMAGE_COMPRESSION_DISABLED_EXT == requested_compression)
		{
			// 仅用压缩
			LOGW("(Swapchain) Disabling default (lossless) compression, which can negatively impact performance")
		}
	}
	else
	{
		if (VK_IMAGE_COMPRESSION_DEFAULT_EXT != requested_compression)
		{
			// 拓展不支持压缩，发出警告，回退到默认设置
			LOGW("(Swapchain) Compression cannot be controlled because VK_EXT_image_compression_control_swapchain is not enabled")

			this->requested_compression            = VK_IMAGE_COMPRESSION_DEFAULT_EXT;
			this->requested_compression_fixed_rate = VK_IMAGE_COMPRESSION_FIXED_RATE_NONE_EXT;
		}
	}

	VkResult result = vkCreateSwapchainKHR(device.get_handle(), &create_info, nullptr, &handle);

	if (result != VK_SUCCESS)
	{
		throw VulkanException{result, "Cannot create Swapchain"};
	}

	uint32_t image_available{0u};
	VK_CHECK(vkGetSwapchainImagesKHR(device.get_handle(), handle, &image_available, nullptr));

	images.resize(image_available);

	VK_CHECK(vkGetSwapchainImagesKHR(device.get_handle(), handle, &image_available, images.data()));

	// 检查在创建交换链后，实际的图像压缩设置是否与请求的一致，如果不一致，则进行适当的调整和日志记录。
	if (device.is_extension_enabled(VK_EXT_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_EXTENSION_NAME) &&
	    VK_IMAGE_COMPRESSION_FIXED_RATE_EXPLICIT_EXT == requested_compression)
	{
		// Check if fixed-rate compression was applied
		const auto applied_compression_fixed_rate = vkb::query_applied_compression(device.get_handle(), images[0]).imageCompressionFixedRateFlags;

		if (applied_compression_fixed_rate != requested_compression_fixed_rate)
		{
			LOGW("(Swapchain) Requested fixed-rate compression ({}) was not applied, instead images use {}",
			     image_compression_fixed_rate_flags_to_string(requested_compression_fixed_rate),
			     image_compression_fixed_rate_flags_to_string(applied_compression_fixed_rate));

			this->requested_compression_fixed_rate = applied_compression_fixed_rate;

			if (VK_IMAGE_COMPRESSION_FIXED_RATE_NONE_EXT == applied_compression_fixed_rate)
			{
				this->requested_compression = VK_IMAGE_COMPRESSION_DEFAULT_EXT;
			}
		}
		else
		{
			LOGI("(Swapchain) Applied fixed-rate compression: {}",
			     image_compression_fixed_rate_flags_to_string(applied_compression_fixed_rate));
		}
	}
}

Swapchain::~Swapchain()
{
	if (handle != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device.get_handle(), handle, nullptr);
	}
}

Swapchain::Swapchain(Swapchain &&other) :
    device{other.device},
    surface{std::exchange(other.surface, VK_NULL_HANDLE)},
    handle{std::exchange(other.handle, VK_NULL_HANDLE)},
    images{std::exchange(other.images, {})},
    properties{std::exchange(other.properties, {})},
    present_mode_priority_list{std::exchange(other.present_mode_priority_list, {})},
    surface_format_priority_list{std::exchange(other.surface_format_priority_list, {})},
    image_usage_flags{std::move(other.image_usage_flags)}
{
}

bool Swapchain::is_valid() const
{
	return handle != VK_NULL_HANDLE;
}

vkb::core::DeviceC &Swapchain::get_device()
{
	return device;
}

VkSwapchainKHR Swapchain::get_handle() const
{
	return handle;
}

VkResult Swapchain::acquire_next_image(uint32_t &image_index, VkSemaphore image_acquired_semaphore, VkFence fence) const
{
	// 获取下一张图像
	return vkAcquireNextImageKHR(device.get_handle(), handle, std::numeric_limits<uint64_t>::max(), image_acquired_semaphore, fence, &image_index);
}

const VkExtent2D &Swapchain::get_extent() const
{
	return properties.extent;
}

VkFormat Swapchain::get_format() const
{
	return properties.surface_format.format;
}

VkSurfaceFormatKHR Swapchain::get_surface_format() const
{
	return properties.surface_format;
}

const std::vector<VkImage> &Swapchain::get_images() const
{
	return images;
}

VkSurfaceTransformFlagBitsKHR Swapchain::get_transform() const
{
	return properties.pre_transform;
}

VkSurfaceKHR Swapchain::get_surface() const
{
	return surface;
}

VkImageUsageFlags Swapchain::get_usage() const
{
	return properties.image_usage;
}

VkPresentModeKHR Swapchain::get_present_mode() const
{
	return properties.present_mode;
}

VkImageCompressionFlagsEXT Swapchain::get_applied_compression() const
{
	return vkb::query_applied_compression(device.get_handle(), get_images()[0]).imageCompressionFlags;
}

std::vector<Swapchain::SurfaceFormatCompression> Swapchain::query_supported_fixed_rate_compression(vkb::core::DeviceC &device, const VkSurfaceKHR &surface)
{
	// 在支持VK_EXT_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_EXTENSION_NAME扩展的设备上查询表面格式的压缩属性
	// 每一种表面格式都会返回一个VkImageCompressionPropertiesEXT结构体，描述该格式支持的压缩特性
	std::vector<SurfaceFormatCompression> surface_format_compression_list;

	if (device.is_extension_enabled(VK_EXT_IMAGE_COMPRESSION_CONTROL_SWAPCHAIN_EXTENSION_NAME))
	{
		if (device.get_gpu().get_instance().is_enabled(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
		{
			VkPhysicalDeviceSurfaceInfo2KHR surface_info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR};
			surface_info.surface = surface;

			uint32_t surface_format_count{0U};

			VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(device.get_gpu().get_handle(), &surface_info, &surface_format_count, nullptr));

			std::vector<VkSurfaceFormat2KHR> surface_formats;
			surface_formats.resize(surface_format_count, {VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR});

			std::vector<VkImageCompressionPropertiesEXT> compression_properties;
			compression_properties.resize(surface_format_count, {VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT});

			for (uint32_t i = 0; i < surface_format_count; i++)
			{
				surface_formats[i].pNext = &compression_properties[i];
			}

			VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(device.get_gpu().get_handle(), &surface_info, &surface_format_count, surface_formats.data()));

			surface_format_compression_list.reserve(surface_format_count);
			for (uint32_t i = 0; i < surface_format_count; i++)
			{
				surface_format_compression_list.push_back({surface_formats[i], compression_properties[i]});
			}
		}
		else
		{
			LOGW("(Swapchain) To query fixed-rate compression support, instance extension VK_KHR_get_surface_capabilities2 must be enabled")
		}
	}
	else
	{
		LOGW("(Swapchain) To query fixed-rate compression support, device extension VK_EXT_image_compression_control_swapchain must be enabled")
	}

	return surface_format_compression_list;
}
}        // namespace vkb
