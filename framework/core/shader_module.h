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

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#	undef None
#endif

namespace vkb
{
namespace core
{
template <vkb::BindingType bindingType>
class Device;
using DeviceC = Device<vkb::BindingType::C>;
}        // namespace core

/// Types of shader resources
// shader资源类型
enum class ShaderResourceType
{
	Input,                    // 输入资源，通常指的是顶点着色器的输入。例如，顶点属性（位置、法线、纹理坐标等）。在顶点着色器中，每个顶点都会输入这些数据。
	InputAttachment,          // 输入附件，主要用于子通道（subpass）中，允许片段着色器读取当前渲染通道（render pass）中先前子通道的帧缓冲附件数据。这在延迟渲染等场景中非常有用。
	Output,                   // 输出资源，通常指的是着色器阶段的输出。例如，顶点着色器输出处理后的顶点数据，片段着色器输出颜色和深度值到帧缓冲。
	Image,                    // 图像资源，指的是纹理图像，可以在着色器中被采样。通常用于存储颜色、法线、高光等贴图。
	ImageSampler,             // 图像采样器，是图像和采样器的组合。在着色器中，我们通常需要采样纹理，所以图像和采样器一起使用。在某些API（如Vulkan）中，图像和采样器可以是分离的，但也可以组合成一个资源。
	ImageStorage,             // 图像存储，用于图像加载/存储操作（Image Load/Store）。这是一种无序访问操作，允许着色器对图像进行随机读写，通常用于计算着色器中。
	Sampler,                  // 采样器，定义了如何对纹理进行采样，包括过滤方式（如线性、最近邻）和寻址模式（如重复、钳制等）。采样器可以与图像分开绑定。
	BufferUniform,            // 统一缓冲区（Uniform Buffer），用于存储着色器中的统一变量。这些变量在绘制调用中保持不变，通常用于传递模型视图投影矩阵等数据。
	BufferStorage,            // 存储缓冲区（Storage Buffer），类似于统一缓冲区，但支持更大的数据量，并且可以随机读写。通常用于计算着色器中存储大量数据。
	PushConstant,             // 推送常量（Push Constant），是一种高效的传递少量数据到着色器的方式。推送常量不需要缓冲区，而是直接通过命令缓冲区传递，速度很快。
	SpecializationConstant,   // specialization常量（Specialization Constant），是在管线创建时指定的常量，允许在管线创建时改变着色器中的常量值，而不需要重新编译着色器。这可以用于配置着色器行为（如定义循环次数、条件编译等）。
	All
};

/// This determines the type and method of how descriptor set should be created and bound
enum class ShaderResourceMode
{
	Static,					  // 静态资源，一旦绑定不可以修改
	Dynamic,                  // 动态资源，意味着可以在命令缓冲区记录期间通过vkCmdBindDescriptorSets时提供动态偏移来更新部分内容（例如，动态统一缓冲区或动态存储缓冲区）。
	UpdateAfterBind           // 描述符在绑定到命令缓冲区之后，仍然可以被更新（直到绘制/调度命令实际执行之前）。
};

/// A bitmask of qualifiers applied to a resource
struct ShaderResourceQualifiers
{
	enum : uint32_t
	{
		None        = 0,
		NonReadable = 1,
		NonWritable = 2,
	};
};

/// Store shader resource data.
/// Used by the shader module.
// shader资源
struct ShaderResource
{
	VkShaderStageFlags stages; 					// 在哪些shader阶段可用

	ShaderResourceType type;   					// 资源类型

	ShaderResourceMode mode;   					// 静态，动态，绑定后更新

	uint32_t set;              					// 描述符集合id

	uint32_t binding;          					// 绑定点

	uint32_t location;							// 位置

	uint32_t input_attachment_index;			// input attachment索引

	uint32_t vec_size;							// 如果是vector，指定分量个数

	uint32_t columns;							// 不知道用处

	uint32_t array_size;						// 如果是数组，指定数组元素个数

	uint32_t offset;							// 不知道用途

	uint32_t size;								// 不知道用途

	uint32_t constant_id;						// const id

	uint32_t qualifiers;						// 不知道用途

	std::string name;							// 资源名
};

/**
 * @brief Adds support for C style preprocessor macros to glsl shaders
 *        enabling you to define or undefine certain symbols
 */
class ShaderVariant
{
  public:
	ShaderVariant() = default;

	size_t get_id() const;

	/**
	 * @brief Specifies the size of a named runtime array for automatic reflection. If already specified, overrides the size.
	 * @param runtime_array_name String under which the runtime array is named in the shader
	 * @param size Integer specifying the wanted size of the runtime array (in number of elements, not size in bytes), used for automatic allocation of buffers.
	 * See get_declared_struct_size_runtime_array() in spirv_cross.h
	 */
	// 设置storage buffer中数组的运行时大小
	void add_runtime_array_size(const std::string &runtime_array_name, size_t size);

	void set_runtime_array_sizes(const std::unordered_map<std::string, size_t> &sizes);

	const std::unordered_map<std::string, size_t> &get_runtime_array_sizes() const;

	void clear();

  private:
	size_t id;

	std::unordered_map<std::string, size_t> runtime_array_sizes;
};

class ShaderSource
{
	// 代表shader源码
  public:
	ShaderSource() = default;

	ShaderSource(const std::string &filename);

	size_t get_id() const;

	const std::string &get_filename() const;

	void set_source(const std::string &source);

	const std::string &get_source() const;

  private:
	size_t id;

	std::string filename;

	std::string source;
};

/**
 * @brief Contains shader code, with an entry point, for a specific shader stage.
 * It is needed by a PipelineLayout to create a Pipeline.
 * ShaderModule can do auto-pairing between shader code and textures.
 * The low level code can change bindings, just keeping the name of the texture.
 * Variants for each texture are also generated, such as HAS_BASE_COLOR_TEX.
 * It works similarly for attribute locations. A current limitation is that only set 0
 * is considered. Uniform buffers are currently hardcoded as well.
 */
class ShaderModule
{
  public:
	ShaderModule(vkb::core::DeviceC   &device,
	             VkShaderStageFlagBits stage,
	             const ShaderSource   &shader_source,
	             const std::string    &entry_point,
	             const ShaderVariant  &shader_variant);

	ShaderModule(const ShaderModule &) = delete;

	ShaderModule(ShaderModule &&other);

	ShaderModule &operator=(const ShaderModule &) = delete;

	ShaderModule &operator=(ShaderModule &&) = delete;

	size_t get_id() const;

	VkShaderStageFlagBits get_stage() const;

	const std::string &get_entry_point() const;

	const std::vector<ShaderResource> &get_resources() const;

	const std::vector<uint32_t> &get_binary() const;

	inline const std::string &get_debug_name() const
	{
		return debug_name;
	}

	inline void set_debug_name(const std::string &name)
	{
		debug_name = name;
	}

	/**
	 * @brief Flags a resource to use a different method of being bound to the shader
	 * @param resource_name The name of the shader resource
	 * @param resource_mode The mode of how the shader resource will be bound
	 */
	void set_resource_mode(const std::string &resource_name, const ShaderResourceMode &resource_mode);

  private:
	vkb::core::DeviceC &device;

	/// Shader unique id
	size_t id;

	/// Stage of the shader (vertex, fragment, etc)
	VkShaderStageFlagBits stage{};

	/// Name of the main function
	std::string entry_point;

	/// Human-readable name for the shader
	std::string debug_name;

	/// Compiled source
	std::vector<uint32_t> spirv;

	std::vector<ShaderResource> resources;
};
}        // namespace vkb
