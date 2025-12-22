/* Copyright (c) 2021-2025, Sascha Willems
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

#include "acceleration_structure.h"

#include "device.h"

namespace vkb
{
namespace core
{
AccelerationStructure::AccelerationStructure(vkb::core::DeviceC            &device,
                                             VkAccelerationStructureTypeKHR type) :
    device{device},
    type{type}
{
}

AccelerationStructure::~AccelerationStructure()
{
	if (handle != VK_NULL_HANDLE)
	{
		vkDestroyAccelerationStructureKHR(device.get_handle(), handle, nullptr);
	}
}

uint64_t AccelerationStructure::add_triangle_geometry(vkb::core::BufferC &vertex_buffer,
                                                      vkb::core::BufferC &index_buffer,
                                                      vkb::core::BufferC &transform_buffer,
                                                      uint32_t            triangle_count,
                                                      uint32_t            max_vertex,
                                                      VkDeviceSize        vertex_stride,
                                                      uint32_t            transform_offset,
                                                      VkFormat            vertex_format,
                                                      VkIndexType         index_type,
                                                      VkGeometryFlagsKHR  flags,
                                                      uint64_t            vertex_buffer_data_address,
                                                      uint64_t            index_buffer_data_address,
                                                      uint64_t            transform_buffer_data_address)
{
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType                                          = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;

	// 几何体类型，三角形或者aabb包围盒，这里是三角形
	geometry.geometryType                                   = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	
	// 物体flag，物体不透明，物体只调用一次任意命中着色器等
	geometry.flags                                          = flags;

	// 由于几何体类型是三角形，因此这里填充三角形的数据
	geometry.geometry.triangles.sType                       = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

	// 顶点格式
	geometry.geometry.triangles.vertexFormat                = vertex_format;

	// 最大顶点个数（理论上线，实际上真正构建的顶点可能只是几何体的一部分）
	geometry.geometry.triangles.maxVertex                   = max_vertex;

	// 顶点步长
	geometry.geometry.triangles.vertexStride                = vertex_stride;

	// 索引类型
	geometry.geometry.triangles.indexType                   = index_type;

	// 顶点数据gpu地址
	geometry.geometry.triangles.vertexData.deviceAddress    = vertex_buffer_data_address == 0 ? vertex_buffer.get_device_address() : vertex_buffer_data_address;

	// 索引数据gpu地址
	geometry.geometry.triangles.indexData.deviceAddress     = index_buffer_data_address == 0 ? index_buffer.get_device_address() : index_buffer_data_address;

	// 坐标变换数据gpu地址
	geometry.geometry.triangles.transformData.deviceAddress = transform_buffer_data_address == 0 ? transform_buffer.get_device_address() : transform_buffer_data_address;

	// 保存了几何体，三角形数量，变换偏移量（字节为单位）
	uint64_t index = geometries.size();
	geometries.insert({index, {geometry, triangle_count, transform_offset}});
	return index;
}

void AccelerationStructure::update_triangle_geometry(uint64_t                             triangleUUID,
                                                     std::unique_ptr<vkb::core::BufferC> &vertex_buffer,
                                                     std::unique_ptr<vkb::core::BufferC> &index_buffer,
                                                     std::unique_ptr<vkb::core::BufferC> &transform_buffer,
                                                     uint32_t triangle_count, uint32_t max_vertex,
                                                     VkDeviceSize vertex_stride, uint32_t transform_offset,
                                                     VkFormat vertex_format, VkGeometryFlagsKHR flags,
                                                     uint64_t vertex_buffer_data_address,
                                                     uint64_t index_buffer_data_address,
                                                     uint64_t transform_buffer_data_address)
{
	// 更新指定的三角形几何体
	VkAccelerationStructureGeometryKHR *geometry             = &geometries[triangleUUID].geometry;
	geometry->sType                                          = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry->geometryType                                   = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry->flags                                          = flags;
	geometry->geometry.triangles.sType                       = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	geometry->geometry.triangles.vertexFormat                = vertex_format;
	geometry->geometry.triangles.maxVertex                   = max_vertex;
	geometry->geometry.triangles.vertexStride                = vertex_stride;
	geometry->geometry.triangles.indexType                   = VK_INDEX_TYPE_UINT32;
	geometry->geometry.triangles.vertexData.deviceAddress    = vertex_buffer_data_address == 0 ? vertex_buffer->get_device_address() : vertex_buffer_data_address;
	geometry->geometry.triangles.indexData.deviceAddress     = index_buffer_data_address == 0 ? index_buffer->get_device_address() : index_buffer_data_address;
	geometry->geometry.triangles.transformData.deviceAddress = transform_buffer_data_address == 0 ? transform_buffer->get_device_address() : transform_buffer_data_address;
	geometries[triangleUUID].primitive_count                 = triangle_count;
	geometries[triangleUUID].transform_offset                = transform_offset;
	geometries[triangleUUID].updated                         = true;
}

uint64_t AccelerationStructure::add_instance_geometry(std::unique_ptr<vkb::core::BufferC> &instance_buffer, uint32_t instance_count, uint32_t transform_offset, VkGeometryFlagsKHR flags)
{
	// 添加实例
	VkAccelerationStructureGeometryKHR geometry{};
	geometry.sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	
	// 集合体是实例类型
	geometry.geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;

	// 标识位
	geometry.flags                                 = flags;

	// 指定了集合体是实例类型，这里填充instances部分
	geometry.geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

	// 不使用pointer数组，data中就是实例的数组，而不是指针的数组
	geometry.geometry.instances.arrayOfPointers    = VK_FALSE;

	// instance缓存的gpu地址
	geometry.geometry.instances.data.deviceAddress = instance_buffer->get_device_address();

	uint64_t index = geometries.size();
	geometries.insert({index, {geometry, instance_count, transform_offset}});
	return index;
}

void AccelerationStructure::update_instance_geometry(uint64_t                             instance_UID,
                                                     std::unique_ptr<vkb::core::BufferC> &instance_buffer,
                                                     uint32_t instance_count, uint32_t transform_offset,
                                                     VkGeometryFlagsKHR flags)
{
	// 更新指定的实例集合体
	VkAccelerationStructureGeometryKHR *geometry    = &geometries[instance_UID].geometry;
	geometry->sType                                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry->geometryType                          = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry->flags                                 = flags;
	geometry->geometry.instances.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry->geometry.instances.arrayOfPointers    = VK_FALSE;
	geometry->geometry.instances.data.deviceAddress = instance_buffer->get_device_address();
	geometries[instance_UID].primitive_count        = instance_count;
	geometries[instance_UID].transform_offset       = transform_offset;
	geometries[instance_UID].updated                = true;
}

void AccelerationStructure::build(VkQueue queue, VkBuildAccelerationStructureFlagsKHR flags, VkBuildAccelerationStructureModeKHR mode)
{
	// 编译加速结构后，加速结构才成使用，mode指定是完整的重新重新构建，还是只是更新部分加速结构
	assert(!geometries.empty());

	// 几何体数组
	std::vector<VkAccelerationStructureGeometryKHR>       acceleration_structure_geometries;

	// 编译信息数组
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> acceleration_structure_build_range_infos;

	// 几何体图元个数数组
	std::vector<uint32_t>                                 primitive_counts;
	for (auto &geometry : geometries)
	{
		if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR && !geometry.second.updated)
		{
			// 如果只更新部分加速结构，则排除不需要更新的集合体
			continue;
		}
		// 要更新的集合体
		acceleration_structure_geometries.push_back(geometry.second.geometry);
		// Infer build range info from geometry
		// build info指定了集合体图元数量，图元偏移，第一个顶点索引，坐标变换的偏移
		VkAccelerationStructureBuildRangeInfoKHR build_range_info;
		build_range_info.primitiveCount  = geometry.second.primitive_count;

		// 索引缓冲区偏移，索引模式下以字节为单位，非索引模式则必须是0
		build_range_info.primitiveOffset = 0;

		// 从第几个顶点开始，注意必须满足firstVertex + 3 * primitiveCount < maxVertex
		build_range_info.firstVertex     = 0;
		build_range_info.transformOffset = geometry.second.transform_offset;
		acceleration_structure_build_range_infos.push_back(build_range_info);

		// 图元数量
		primitive_counts.push_back(geometry.second.primitive_count);
		geometry.second.updated = false;
	}

	// 准备编译加速结构的info
	VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info{};
	build_geometry_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	build_geometry_info.type  = type;
	build_geometry_info.flags = flags;
	build_geometry_info.mode  = mode;
	if (mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR && handle != VK_NULL_HANDLE)
	{
		// 如果是更新模式，指定源加速结构和目标加速结构
		build_geometry_info.srcAccelerationStructure = handle;
		build_geometry_info.dstAccelerationStructure = handle;
	}
	build_geometry_info.geometryCount = static_cast<uint32_t>(acceleration_structure_geometries.size());
	build_geometry_info.pGeometries   = acceleration_structure_geometries.data();

	// Get required build sizes
	// 获取加速结构所需要的缓存大小
	build_sizes_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(
	    device.get_handle(),
	    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
	    &build_geometry_info,
	    primitive_counts.data(),
	    &build_sizes_info);

	// Create a buffer for the acceleration structure
	if (!buffer || buffer->get_size() != build_sizes_info.accelerationStructureSize)
	{
		// 创建加速结构，以及加速结构所需要的缓存
		// 注意这里的buffer usage
		buffer = std::make_unique<vkb::core::BufferC>(
		    device,
		    build_sizes_info.accelerationStructureSize,
		    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		    VMA_MEMORY_USAGE_GPU_ONLY);

		VkAccelerationStructureCreateInfoKHR acceleration_structure_create_info{};
		acceleration_structure_create_info.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		acceleration_structure_create_info.buffer = buffer->get_handle();
		acceleration_structure_create_info.size   = build_sizes_info.accelerationStructureSize;
		acceleration_structure_create_info.type   = type;
		VkResult result                           = vkCreateAccelerationStructureKHR(device.get_handle(), &acceleration_structure_create_info, nullptr, &handle);

		if (result != VK_SUCCESS)
		{
			throw VulkanException{result, "Could not create acceleration structure"};
		}
	}

	// Get the acceleration structure's handle
	VkAccelerationStructureDeviceAddressInfoKHR acceleration_device_address_info{};
	acceleration_device_address_info.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	acceleration_device_address_info.accelerationStructure = handle;
	device_address                                         = vkGetAccelerationStructureDeviceAddressKHR(device.get_handle(), &acceleration_device_address_info);

	// Create a scratch buffer as a temporary storage for the acceleration structure build
	// 创建加速结构需要scatch buffer
	scratch_buffer = std::make_unique<vkb::core::BufferC>(
	    device,
	    BufferBuilderC(build_sizes_info.buildScratchSize)
	        .with_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
	        .with_vma_usage(VMA_MEMORY_USAGE_GPU_ONLY)
	        .with_alignment(scratch_buffer_alignment));

	build_geometry_info.scratchData.deviceAddress = scratch_buffer->get_device_address();
	build_geometry_info.dstAccelerationStructure  = handle;

	// Build the acceleration structure on the device via a one-time command buffer submission
	VkCommandBuffer command_buffer       = device.create_command_buffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	auto            as_build_range_infos = &*acceleration_structure_build_range_infos.data();
	vkCmdBuildAccelerationStructuresKHR(
	    command_buffer,
	    1,
	    &build_geometry_info,
	    &as_build_range_infos);
	device.flush_command_buffer(command_buffer, queue);
	scratch_buffer.reset();
}

VkAccelerationStructureKHR AccelerationStructure::get_handle() const
{
	return handle;
}

const VkAccelerationStructureKHR *AccelerationStructure::get() const
{
	return &handle;
}

uint64_t AccelerationStructure::get_device_address() const
{
	return device_address;
}

}        // namespace core
}        // namespace vkb