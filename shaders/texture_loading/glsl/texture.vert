#version 450
/* Copyright (c) 2019-2024, Sascha Willems
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

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 model;
	vec4 viewPos;
	float lodBias;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out float outLodBias;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	// 纹理坐标
	outUV = inUV;

	// lod偏移量
	outLodBias = ubo.lodBias;

	// 世界位置，但是vec3版本
	vec3 worldPos = vec3(ubo.model * vec4(inPos, 1.0));

	// 投影之后的位置
	gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);

	// 世界位置，但是vec4版本
    vec4 pos = ubo.model * vec4(inPos, 1.0);

	// 切线空间法线到世界空间的转换，outNormal可以和世界空间的光源做光照计算
	outNormal = mat3(inverse(transpose(ubo.model))) * inNormal;

	// 这里的光源是错误的，虽然输出了光源的方向
	vec3 lightPos = vec3(0.0);
	vec3 lPos = mat3(ubo.model) * lightPos.xyz;
    outLightVec = lPos - pos.xyz;

	// 视角方向
    outViewVec = ubo.viewPos.xyz - pos.xyz;		
}
