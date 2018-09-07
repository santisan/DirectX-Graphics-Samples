//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author(s):	Alex Nankervis
//

#include "AssimpModelLoader.h"
#include "Model.h"
#include "SkinnedModel.h"
#include <assert.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <unordered_map>
#include <queue>

using namespace Math;

std::unique_ptr<Model> AssimpModelLoader::LoadModel(const char *filename)
{
	Assimp::Importer importer;

	// remove unused data
	importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
		aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS);

	// max triangles and vertices per mesh, splits above this threshold
	importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, INT_MAX);
	importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 0xfffe); // avoid the primitive restart index

																		// remove points and lines
	importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

	const aiScene *scene = importer.ReadFile(filename,
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate |
		aiProcess_RemoveComponent |
		aiProcess_GenSmoothNormals |
		aiProcess_SplitLargeMeshes |
		aiProcess_ValidateDataStructure |
		//aiProcess_ImproveCacheLocality | // handled by optimizePostTransform()
		aiProcess_RemoveRedundantMaterials |
		aiProcess_SortByPType |
		aiProcess_FindInvalidData |
		aiProcess_GenUVCoords |
		aiProcess_TransformUVCoords |
		aiProcess_OptimizeMeshes |
		aiProcess_OptimizeGraph);

	if (scene == nullptr) {
		return nullptr;
	}
	ASSERT(scene->HasMeshes());
	SkinnedModel* skinnedModel = nullptr;
	Model* model = nullptr;

	// Map bone name to the corresponding node index in the skeleton joints array
	std::unordered_map<std::string, JointIndexType> boneIndexByNameMap;

	if (scene->HasAnimations())
	{
		skinnedModel = new SkinnedModel();
		model = static_cast<Model*>(skinnedModel);
		/*auto& skeletonJoints = skinnedModel->m_Skeleton.joints;

		// Queue stores pairs with node to process and its parents's index in the joints array
		typedef std::pair<const aiNode*, JointIndexType> NodeJointIndexPair;
		std::queue<NodeJointIndexPair> nodesQueue;

		// Skip scene->mRootNode because it's a dummy root, not the skeleton root
		for (uint32_t i = 0; i < scene->mRootNode->mNumChildren; ++i) {
			nodesQueue.push(std::make_pair(scene->mRootNode->mChildren[i], kRootJointParentIndex));
		}

		// Traverse the scene tree to create the joints and the mapping from bone name to node index
		while (!nodesQueue.empty())
		{
			const aiNode* const currentNode = nodesQueue.front().first;
			const JointIndexType currentNodeParentIndex = nodesQueue.front().second;
			ASSERT(currentNode);

			if (currentNode->mNumMeshes == 0 && currentNode->mName.length > 0)
			{
				const auto nodeIndex = static_cast<JointIndexType>(skeletonJoints.size());
				const char* const nodeName = currentNode->mName.C_Str();
				skeletonJoints.push_back(Joint{nodeName, Matrix4{kIdentity}, currentNodeParentIndex});
				ASSERT(boneIndexByNameMap.find(nodeName) == boneIndexByNameMap.end());
				boneIndexByNameMap[nodeName] = nodeIndex;
			}

			const auto parentIndex = skeletonJoints.empty() ? kRootJointParentIndex : static_cast<JointIndexType>(skeletonJoints.size() - 1);
			for (uint32_t i = 0; i < currentNode->mNumChildren; ++i) {
				nodesQueue.push(std::make_pair(currentNode->mChildren[i], parentIndex));
			}

			nodesQueue.pop();
		}
		
		// TODO: process all animations
		const aiAnimation* const aiAnim = scene->mAnimations[0];
		AnimationClip& animationClip = skinnedModel->m_AnimationClip;
		animationClip.skeleton = &skinnedModel->m_Skeleton;
		animationClip.name = aiAnim->mName.C_Str();
		animationClip.framesPerSecond = (aiAnim->mTicksPerSecond != 0.0) ? static_cast<float>(aiAnim->mTicksPerSecond) : 25.f;
		animationClip.frameCount = static_cast<uint32_t>(aiAnim->mDuration);
		animationClip.durationSeconds = animationClip.frameCount / animationClip.framesPerSecond;
		ASSERT(Floor(static_cast<float>(aiAnim->mDuration)) == static_cast<float>(aiAnim->mDuration));
		DEBUGPRINT("animation duration(ticks): %f", aiAnim->mDuration);
		DEBUGPRINT("animation duration(seconds): %f", animationClip.durationSeconds);
		DEBUGPRINT("animation fps: %f", animationClip.framesPerSecond);
	
		uint32_t numSamples = 0u;
		for (uint32_t i = 0; i < aiAnim->mNumChannels; ++i)
		{
			const aiNodeAnim* const nodeAnim = aiAnim->mChannels[i];
			numSamples = std::max(numSamples, std::max(nodeAnim->mNumPositionKeys, std::max(nodeAnim->mNumRotationKeys, nodeAnim->mNumScalingKeys)));
		}
		
		animationClip.samples.resize(numSamples);
		for (uint32_t i = 0; i < numSamples; ++i) {
			animationClip.samples[i].jointPoses.resize(skeletonJoints.size());
		}
		DEBUGPRINT("animation clips count: %d", numSamples);

		for (uint32_t channelIndex = 0; channelIndex < aiAnim->mNumChannels; ++channelIndex)
		{
			const aiNodeAnim* const nodeAnim = aiAnim->mChannels[channelIndex];
			ASSERT(boneIndexByNameMap.find(nodeAnim->mNodeName.C_Str()) != boneIndexByNameMap.end());
			const auto jointIndex = boneIndexByNameMap[nodeAnim->mNodeName.C_Str()];
			
			for (uint32_t sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
			{
				JointPose& jointPose = animationClip.samples[sampleIndex].jointPoses[jointIndex];
				if (sampleIndex < nodeAnim->mNumPositionKeys)
				{
					const auto& t = nodeAnim->mPositionKeys[sampleIndex].mValue;
					jointPose.translation = Vector3{t.x, t.y, t.z};
				}
				if (sampleIndex < nodeAnim->mNumRotationKeys)
				{
					const auto& q = nodeAnim->mRotationKeys[sampleIndex].mValue;
					const XMFLOAT4A tempF4A{q.x, q.y, q.z, q.w};
					const XMVECTOR tempVec = XMLoadFloat4A(&tempF4A);
					jointPose.rotation = Quaternion{tempVec};
				}
				if (sampleIndex < nodeAnim->mNumScalingKeys)
				{
					const auto& s = nodeAnim->mScalingKeys[sampleIndex].mValue;
					if (s.x != s.y || s.y != s.z)
					{
						static const float kScaleDiffTolerance = 1e-6f;
						if (Abs(s.x - s.y) >= kScaleDiffTolerance || Abs(s.y - s.z) >= kScaleDiffTolerance)	{
							ASSERT(false, "non-uniform scaling %.9f, %.9f, %.9f", s.x, s.y, s.z);
						}
						else {
							DEBUGPRINT("non-uniform scaling within tolerance %.9f, %.9f, %.9f", s.x, s.y, s.z);
						}
					}
					jointPose.scale = s.x;
				}

				//nodeAnim->mPositionKeys[i].mTime // TODO: need to use this?
			}
		}*/
	}
	else // no animations
	{
		model = new Model();
	}
	
	m_pCurrentModel = model;

	if (scene->HasTextures())
	{
		// embedded textures...
	}

	if (scene->HasMaterials())
	{
		model->m_Header.materialCount = scene->mNumMaterials;
		model->m_pMaterial = new Material[model->m_Header.materialCount];
		memset(model->m_pMaterial, 0, sizeof(Material) * model->m_Header.materialCount);
		for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; materialIndex++)
		{
			const aiMaterial *srcMat = scene->mMaterials[materialIndex];
			Material *dstMat = model->m_pMaterial + materialIndex;

			aiColor3D diffuse(1.0f, 1.0f, 1.0f);
			aiColor3D specular(1.0f, 1.0f, 1.0f);
			aiColor3D ambient(1.0f, 1.0f, 1.0f);
			aiColor3D emissive(0.0f, 0.0f, 0.0f);
			aiColor3D transparent(1.0f, 1.0f, 1.0f);
			float opacity = 1.0f;
			float shininess = 0.0f;
			float specularStrength = 1.0f;
			aiString texDiffusePath;
			aiString texSpecularPath;
			aiString texEmissivePath;
			aiString texNormalPath;
			aiString texLightmapPath;
			aiString texReflectionPath;
			srcMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			srcMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
			srcMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
			srcMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
			srcMat->Get(AI_MATKEY_COLOR_TRANSPARENT, transparent);
			srcMat->Get(AI_MATKEY_OPACITY, opacity);
			srcMat->Get(AI_MATKEY_SHININESS, shininess);
			srcMat->Get(AI_MATKEY_SHININESS_STRENGTH, specularStrength);
			srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), texDiffusePath);
			srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_SPECULAR, 0), texSpecularPath);
			srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), texEmissivePath);
			srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), texNormalPath);
			srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_LIGHTMAP, 0), texLightmapPath);
			srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_REFLECTION, 0), texReflectionPath);

			dstMat->diffuse = Vector3(diffuse.r, diffuse.g, diffuse.b);
			dstMat->specular = Vector3(specular.r, specular.g, specular.b);
			dstMat->ambient = Vector3(ambient.r, ambient.g, ambient.b);
			dstMat->emissive = Vector3(emissive.r, emissive.g, emissive.b);
			dstMat->transparent = Vector3(transparent.r, transparent.g, transparent.b);
			dstMat->opacity = opacity;
			dstMat->shininess = shininess;
			dstMat->specularStrength = specularStrength;

			char *pRem = nullptr;

			strncpy_s(dstMat->texDiffusePath, "models/", Material::maxTexPath - 1);
			strncat_s(dstMat->texDiffusePath, texDiffusePath.C_Str(), Material::maxTexPath - 1);
			pRem = strrchr(dstMat->texDiffusePath, '.');
			while (pRem != nullptr && *pRem != 0) *(pRem++) = 0; // remove extension

			strncpy_s(dstMat->texSpecularPath, "models/", Material::maxTexPath - 1);
			strncat_s(dstMat->texSpecularPath, texSpecularPath.C_Str(), Material::maxTexPath - 1);
			pRem = strrchr(dstMat->texSpecularPath, '.');
			while (pRem != nullptr && *pRem != 0) *(pRem++) = 0; // remove extension

			strncpy_s(dstMat->texEmissivePath, "models/", Material::maxTexPath - 1);
			strncat_s(dstMat->texEmissivePath, texEmissivePath.C_Str(), Material::maxTexPath - 1);
			pRem = strrchr(dstMat->texEmissivePath, '.');
			while (pRem != nullptr && *pRem != 0) *(pRem++) = 0; // remove extension

			strncpy_s(dstMat->texNormalPath, "models/", Material::maxTexPath - 1);
			strncat_s(dstMat->texNormalPath, texNormalPath.C_Str(), Material::maxTexPath - 1);
			pRem = strrchr(dstMat->texNormalPath, '.');
			while (pRem != nullptr && *pRem != 0) *(pRem++) = 0; // remove extension

			strncpy_s(dstMat->texLightmapPath, "models/", Material::maxTexPath - 1);
			strncat_s(dstMat->texLightmapPath, texLightmapPath.C_Str(), Material::maxTexPath - 1);
			pRem = strrchr(dstMat->texLightmapPath, '.');
			while (pRem != nullptr && *pRem != 0) *(pRem++) = 0; // remove extension

			strncpy_s(dstMat->texReflectionPath, "models/", Material::maxTexPath - 1);
			strncat_s(dstMat->texReflectionPath, texReflectionPath.C_Str(), Material::maxTexPath - 1);
			pRem = strrchr(dstMat->texReflectionPath, '.');
			while (pRem != nullptr && *pRem != 0) *(pRem++) = 0; // remove extension

			aiString matName;
			srcMat->Get(AI_MATKEY_NAME, matName);
			strncpy_s(dstMat->name, matName.C_Str(), Material::maxMaterialName - 1);
		}

		// TODO: fix 
		//model->LoadTextures();
	}

	model->m_Header.meshCount = scene->mNumMeshes;
	model->m_pMesh = new Mesh[model->m_Header.meshCount];
	memset(model->m_pMesh, 0, sizeof(Mesh) * model->m_Header.meshCount);

	// first pass, count everything
	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
	{
		const aiMesh *srcMesh = scene->mMeshes[meshIndex];
		Mesh *dstMesh = model->m_pMesh + meshIndex;

		ASSERT(srcMesh->mPrimitiveTypes == aiPrimitiveType_TRIANGLE);

		dstMesh->materialIndex = srcMesh->mMaterialIndex;

		// just store everything as float. Can quantize in Model::optimize()
		dstMesh->attribsEnabled |= attrib_mask_position;
		dstMesh->attrib[attrib_position].offset = dstMesh->vertexStride;
		dstMesh->attrib[attrib_position].normalized = 0;
		dstMesh->attrib[attrib_position].components = 3;
		dstMesh->attrib[attrib_position].format = attrib_format_float;
		dstMesh->vertexStride += sizeof(float) * 3;

		/*dstMesh->attribsEnabled |= attrib_mask_texcoord0;
		dstMesh->attrib[attrib_texcoord0].offset = dstMesh->vertexStride;
		dstMesh->attrib[attrib_texcoord0].normalized = 0;
		dstMesh->attrib[attrib_texcoord0].components = 2;
		dstMesh->attrib[attrib_texcoord0].format = attrib_format_float;
		dstMesh->vertexStride += sizeof(float) * 2;

		dstMesh->attribsEnabled |= attrib_mask_normal;
		dstMesh->attrib[attrib_normal].offset = dstMesh->vertexStride;
		dstMesh->attrib[attrib_normal].normalized = 0;
		dstMesh->attrib[attrib_normal].components = 3;
		dstMesh->attrib[attrib_normal].format = attrib_format_float;
		dstMesh->vertexStride += sizeof(float) * 3;

		dstMesh->attribsEnabled |= attrib_mask_tangent;
		dstMesh->attrib[attrib_tangent].offset = dstMesh->vertexStride;
		dstMesh->attrib[attrib_tangent].normalized = 0;
		dstMesh->attrib[attrib_tangent].components = 3;
		dstMesh->attrib[attrib_tangent].format = attrib_format_float;
		dstMesh->vertexStride += sizeof(float) * 3;

		dstMesh->attribsEnabled |= attrib_mask_bitangent;
		dstMesh->attrib[attrib_bitangent].offset = dstMesh->vertexStride;
		dstMesh->attrib[attrib_bitangent].normalized = 0;
		dstMesh->attrib[attrib_bitangent].components = 3;
		dstMesh->attrib[attrib_bitangent].format = attrib_format_float;
		dstMesh->vertexStride += sizeof(float) * 3;
		*/
		/*if (srcMesh->HasBones())
		{
			dstMesh->attribsEnabled |= attrib_mask_joint_indices;
			dstMesh->attrib[attrib_joint_indices].offset = dstMesh->vertexStride;
			dstMesh->attrib[attrib_joint_indices].normalized = 0;
			dstMesh->attrib[attrib_joint_indices].components = 4;
			dstMesh->attrib[attrib_joint_indices].format = attrib_format_ushort;
			dstMesh->vertexStride += sizeof(uint16_t) * 4;

			dstMesh->attribsEnabled |= attrib_mask_joint_weights;
			dstMesh->attrib[attrib_joint_weights].offset = dstMesh->vertexStride;
			dstMesh->attrib[attrib_joint_weights].normalized = 1;
			dstMesh->attrib[attrib_joint_weights].components = 4;
			dstMesh->attrib[attrib_joint_weights].format = attrib_format_float;
			dstMesh->vertexStride += sizeof(float) * 4;
		}*/

		// depth-only
		dstMesh->attribsEnabledDepth |= attrib_mask_position;
		dstMesh->attribDepth[attrib_position].offset = dstMesh->vertexStrideDepth;
		dstMesh->attribDepth[attrib_position].normalized = 0;
		dstMesh->attribDepth[attrib_position].components = 3;
		dstMesh->attribDepth[attrib_position].format = attrib_format_float;
		dstMesh->vertexStrideDepth += sizeof(float) * 3;

		// color rendering
		dstMesh->vertexDataByteOffset = model->m_Header.vertexDataByteSize;
		dstMesh->vertexCount = srcMesh->mNumVertices;

		dstMesh->indexDataByteOffset = model->m_Header.indexDataByteSize;
		dstMesh->indexCount = srcMesh->mNumFaces * 3;

		model->m_Header.vertexDataByteSize += dstMesh->vertexStride * dstMesh->vertexCount;
		model->m_Header.indexDataByteSize += sizeof(uint16_t) * dstMesh->indexCount;

		// depth-only rendering
		dstMesh->vertexDataByteOffsetDepth = model->m_Header.vertexDataByteSizeDepth;
		dstMesh->vertexCountDepth = srcMesh->mNumVertices;

		model->m_Header.vertexDataByteSizeDepth += dstMesh->vertexStrideDepth * dstMesh->vertexCountDepth;
	}

	model->m_VertexStride = model->m_pMesh[0].vertexStride;
	model->m_VertexStrideDepth = model->m_pMesh[0].vertexStrideDepth;

	// allocate storage
	model->m_pVertexData = new unsigned char[model->m_Header.vertexDataByteSize];
	model->m_pIndexData = new unsigned char[model->m_Header.indexDataByteSize];
	model->m_pVertexDataDepth = new unsigned char[model->m_Header.vertexDataByteSizeDepth];
	model->m_pIndexDataDepth = new unsigned char[model->m_Header.indexDataByteSize];

	// second pass, fill in vertex and index data
	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
	{
		const aiMesh *srcMesh = scene->mMeshes[meshIndex];
		Mesh *dstMesh = model->m_pMesh + meshIndex;

		float *dstPos = reinterpret_cast<float*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_position].offset);
		/*float *dstTexcoord0 = reinterpret_cast<float*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_texcoord0].offset);
		float *dstNormal = reinterpret_cast<float*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_normal].offset);
		float *dstTangent = reinterpret_cast<float*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_tangent].offset);
		float *dstBitangent = reinterpret_cast<float*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_bitangent].offset);
		uint16_t *dstJointIndices = reinterpret_cast<uint16_t*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_joint_indices].offset);
		float *dstJointWeights = reinterpret_cast<float*>(model->m_pVertexData + dstMesh->vertexDataByteOffset + dstMesh->attrib[attrib_joint_weights].offset);
		*/
		float *dstPosDepth = reinterpret_cast<float*>(model->m_pVertexDataDepth + dstMesh->vertexDataByteOffsetDepth + dstMesh->attribDepth[attrib_position].offset);

		// Vertex to bones mapping
		struct VertexBoneData {
			float boneWeight = 0.f;
			JointIndexType boneIndex = 0;
		};
		typedef std::vector<VertexBoneData> VertexBoneDataArray;
		std::vector<VertexBoneDataArray> vertexBones;

		/*if (srcMesh->HasBones())
		{
			vertexBones.resize(srcMesh->mNumVertices);
			for (auto& vertexBoneData : vertexBones) {
				vertexBoneData.reserve(4);
			}
			auto& skeletonJoints = skinnedModel->m_Skeleton.joints;

			for (uint32_t ib = 0; ib < srcMesh->mNumBones; ++ib)
			{
				aiBone* const bone = srcMesh->mBones[ib];
				const char* const boneName = bone->mName.C_Str();

				ASSERT(boneIndexByNameMap.find(boneName) != boneIndexByNameMap.end());
				const JointIndexType boneIndex = boneIndexByNameMap[boneName];
				ASSERT(boneIndex < skeletonJoints.size());
				skeletonJoints[boneIndex].inverseBindPose = Matrix4{ XMMATRIX{ &bone->mOffsetMatrix.Transpose().a1 } };

				for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex)
				{
					const aiVertexWeight& vertexWeight = bone->mWeights[weightIndex];
					ASSERT(vertexWeight.mVertexId < vertexBones.size());
					const VertexBoneData vertexBoneData{ vertexWeight.mWeight, boneIndex };
					vertexBones[vertexWeight.mVertexId].push_back(vertexBoneData);
				}
			}

			for (auto& vertexBoneData : vertexBones) 
			{
				if (vertexBoneData.size() < 4) {
					for (size_t vi = vertexBoneData.size(); vi < 4; ++vi) {
						vertexBoneData.emplace_back();
					}
				}
				else if (vertexBoneData.size() > 4) {
					ASSERT(false, "More than 4 bones affecting vertex");
				}
			}
		}*/

		for (unsigned int v = 0; v < dstMesh->vertexCount; v++)
		{
			if (srcMesh->mVertices)
			{
				dstPos[0] = srcMesh->mVertices[v].x;
				dstPos[1] = srcMesh->mVertices[v].y;
				dstPos[2] = srcMesh->mVertices[v].z;

				dstPosDepth[0] = srcMesh->mVertices[v].x;
				dstPosDepth[1] = srcMesh->mVertices[v].y;
				dstPosDepth[2] = srcMesh->mVertices[v].z;
			}
			else
			{
				// no position? That's kind of bad.
				ASSERT(0);
			}
			dstPos = (float*)((unsigned char*)dstPos + dstMesh->vertexStride);
			dstPosDepth = (float*)((unsigned char*)dstPosDepth + dstMesh->vertexStrideDepth);

			/*if (srcMesh->mTextureCoords[0])
			{
				dstTexcoord0[0] = srcMesh->mTextureCoords[0][v].x;
				dstTexcoord0[1] = srcMesh->mTextureCoords[0][v].y;
			}
			else
			{
				dstTexcoord0[0] = 0.0f;
				dstTexcoord0[1] = 0.0f;
			}
			dstTexcoord0 = (float*)((unsigned char*)dstTexcoord0 + dstMesh->vertexStride);

			if (srcMesh->mNormals)
			{
				dstNormal[0] = srcMesh->mNormals[v].x;
				dstNormal[1] = srcMesh->mNormals[v].y;
				dstNormal[2] = srcMesh->mNormals[v].z;
			}
			else
			{
				// Assimp should generate normals if they are missing, according to the postprocessing flag specified on load,
				// so we should never get here.
				ASSERT(0);
			}
			dstNormal = (float*)((unsigned char*)dstNormal + dstMesh->vertexStride);

			if (srcMesh->mTangents)
			{
				dstTangent[0] = srcMesh->mTangents[v].x;
				dstTangent[1] = srcMesh->mTangents[v].y;
				dstTangent[2] = srcMesh->mTangents[v].z;
			}
			else
			{
				// TODO: generate tangents/bitangents if missing
				dstTangent[0] = 1.0f;
				dstTangent[1] = 0.0f;
				dstTangent[2] = 0.0f;
			}
			dstTangent = (float*)((unsigned char*)dstTangent + dstMesh->vertexStride);

			if (srcMesh->mBitangents)
			{
				dstBitangent[0] = srcMesh->mBitangents[v].x;
				dstBitangent[1] = srcMesh->mBitangents[v].y;
				dstBitangent[2] = srcMesh->mBitangents[v].z;
			}
			else
			{
				// TODO: generate tangents/bitangents if missing
				dstBitangent[0] = 0.0f;
				dstBitangent[1] = 1.0f;
				dstBitangent[2] = 0.0f;
			}
			dstBitangent = (float*)((unsigned char*)dstBitangent + dstMesh->vertexStride);*/

			/*if (!vertexBones.empty())
			{
				dstJointIndices[0] = vertexBones[v][0].boneIndex;
				dstJointIndices[1] = vertexBones[v][1].boneIndex;
				dstJointIndices[2] = vertexBones[v][2].boneIndex;
				dstJointIndices[3] = vertexBones[v][3].boneIndex;
				
				dstJointWeights[0] = vertexBones[v][0].boneWeight;
				dstJointWeights[1] = vertexBones[v][1].boneWeight;
				dstJointWeights[2] = vertexBones[v][2].boneWeight;
				dstJointWeights[3] = vertexBones[v][3].boneWeight;
				ASSERT(Abs(1.f - (dstJointWeights[0] + dstJointWeights[1] + dstJointWeights[2] + dstJointWeights[3])) <= FLT_EPSILON);

				dstJointIndices = (uint16_t*)((unsigned char*)dstJointIndices + dstMesh->vertexStride);
				dstJointWeights = (float*)((unsigned char*)dstJointWeights + dstMesh->vertexStride);
			}*/
		}

		uint16_t *dstIndex = (uint16_t*)(model->m_pIndexData + dstMesh->indexDataByteOffset);
		uint16_t *dstIndexDepth = (uint16_t*)(model->m_pIndexDataDepth + dstMesh->indexDataByteOffset);
		for (unsigned int f = 0; f < srcMesh->mNumFaces; f++)
		{
			ASSERT(srcMesh->mFaces[f].mNumIndices == 3);

			*dstIndex++ = srcMesh->mFaces[f].mIndices[0];
			*dstIndex++ = srcMesh->mFaces[f].mIndices[1];
			*dstIndex++ = srcMesh->mFaces[f].mIndices[2];

			*dstIndexDepth++ = srcMesh->mFaces[f].mIndices[0];
			*dstIndexDepth++ = srcMesh->mFaces[f].mIndices[1];
			*dstIndexDepth++ = srcMesh->mFaces[f].mIndices[2];
		}
	}

	model->ComputeAllBoundingBoxes();
	//Optimize();

	DEBUGPRINT("vertex count %d", model->m_pMesh[0].vertexCount);
	DEBUGPRINT("index count %d", model->m_pMesh[0].indexCount);

	return std::unique_ptr<Model>(model);
}

std::unique_ptr<SkinnedModel> AssimpModelLoader::LoadSkinnedModel(const char* filename)
{
	SkinnedModel* const model = dynamic_cast<SkinnedModel*>(LoadModel(filename).release());
	ASSERT(model);
	return std::unique_ptr<SkinnedModel>(model);
}
