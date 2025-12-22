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

#include "common/helpers.h"
#include "common/vk_common.h"

namespace vkb
{
class DescriptorPool;
class ShaderModule;

struct ShaderResource;

namespace core
{
template <vkb::BindingType bindingType>
class Device;
using DeviceC = Device<vkb::BindingType::C>;
}        // namespace core

/**
 * @brief Caches DescriptorSet objects for the shader's set index.
 *        Creates a DescriptorPool to allocate the DescriptorSet objects
 */
class DescriptorSetLayout
{
  public:
	/**
	 * @brief Creates a descriptor set layout from a set of resources
	 * @param device A valid Vulkan device
	 * @param set_index The descriptor set index this layout maps to
	 * @param shader_modules The shader modules this set layout will be used for
	 * @param resource_set A grouping of shader resources belonging to the same set
	 */
	DescriptorSetLayout(vkb::core::DeviceC                &device,
	                    const uint32_t                     set_index,
	                    const std::vector<ShaderModule *> &shader_modules,
	                    const std::vector<ShaderResource> &resource_set);

	DescriptorSetLayout(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout(DescriptorSetLayout &&other);

	~DescriptorSetLayout();

	DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;

	DescriptorSetLayout &operator=(DescriptorSetLayout &&) = delete;

	// descriptor set layout的handle
	VkDescriptorSetLayout get_handle() const;

	// descriptor set 在shader中的索引
	const uint32_t get_index() const;

	// descriptor set layout中包含的绑定点
	const std::vector<VkDescriptorSetLayoutBinding> &get_bindings() const;

	// 根据索引获取绑定点
	std::unique_ptr<VkDescriptorSetLayoutBinding> get_layout_binding(const uint32_t binding_index) const;

	// 根据名字获取绑定点
	std::unique_ptr<VkDescriptorSetLayoutBinding> get_layout_binding(const std::string &name) const;

	// 获取绑定点的flags
	// 这些标志允许开发者在描述符绑定上指定一些非标准的行为，例如：
	// 允许描述符绑定中的描述符数量在运行时变化（可变大小）
	// 允许更新已绑定的描述符集
	// 允许部分绑定等。
	const std::vector<VkDescriptorBindingFlagsEXT> &get_binding_flags() const;

	// 根据索引获取绑定点的flags
	VkDescriptorBindingFlagsEXT get_layout_binding_flag(const uint32_t binding_index) const;

	// 获取descriptor set layout在哪些shader module中可见
	const std::vector<ShaderModule *> &get_shader_modules() const;

  private:
	vkb::core::DeviceC &device;

	VkDescriptorSetLayout handle{VK_NULL_HANDLE};

	const uint32_t set_index;

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	std::vector<VkDescriptorBindingFlagsEXT> binding_flags;

	std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings_lookup;

	std::unordered_map<uint32_t, VkDescriptorBindingFlagsEXT> binding_flags_lookup;

	std::unordered_map<std::string, uint32_t> resources_lookup;

	std::vector<ShaderModule *> shader_modules;
};
}        // namespace vkb
