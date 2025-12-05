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

#include "core/vulkan_resource.h"

namespace vkb
{
namespace core
{
template <vkb::BindingType bindingType>
class Device;
using DeviceC = Device<vkb::BindingType::C>;
}        // namespace core

struct Attachment;

// 渲染子通道的信息
struct SubpassInfo
{
	// intput attachments的索引
	std::vector<uint32_t> input_attachments;

	// output attachments的索引，output_attachments其实就是color attachment
	std::vector<uint32_t> output_attachments;

	// resolve attachment，用于将多个采样点的图像转为一个采样点的图像
	std::vector<uint32_t> color_resolve_attachments;

	// 禁用深度模板缓存
	bool disable_depth_stencil_attachment;

	// 深度模板的resolve attachment
	uint32_t depth_stencil_resolve_attachment;

	// resolve的方式，不执行解析，使用第0个样本的值，使用所有样本的均值等
	VkResolveModeFlagBits depth_stencil_resolve_mode;

	// 调试tag
	std::string debug_name;
};

class RenderPass : public vkb::core::VulkanResourceC<VkRenderPass>
{
  public:
	RenderPass(vkb::core::DeviceC               &device,
	           const std::vector<Attachment>    &attachments,
	           const std::vector<LoadStoreInfo> &load_store_infos,
	           const std::vector<SubpassInfo>   &subpasses);

	RenderPass(const RenderPass &) = delete;

	RenderPass(RenderPass &&other);

	~RenderPass();

	RenderPass &operator=(const RenderPass &) = delete;

	RenderPass &operator=(RenderPass &&) = delete;

	// 获取某个子通道的颜色输出的数量
	const uint32_t get_color_output_count(uint32_t subpass_index) const;

	// 渲染区域等颗粒度，渲染目标的大小是颗粒度的整数
	VkExtent2D get_render_area_granularity() const;

  private:
	// 子通道的数量
	size_t subpass_count;

	template <typename T_SubpassDescription, typename T_AttachmentDescription, typename T_AttachmentReference, typename T_SubpassDependency, typename T_RenderPassCreateInfo>
	void create_renderpass(const std::vector<Attachment> &attachments, const std::vector<LoadStoreInfo> &load_store_infos, const std::vector<SubpassInfo> &subpasses);

	// 整个render pass颜色通道的数量
	std::vector<uint32_t> color_output_count;
};
}        // namespace vkb
